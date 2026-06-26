/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "OpenVPNBackend.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <string>

#include <Looper.h>
#include <Message.h>
#include <Messenger.h>

#include "VPNProtocol.h"


// Private 'what' codes posted from worker contexts back to the looper.
static const uint32 kMsgInternalEvent	= 'oEvt';
static const uint32 kMsgReaderExited	= 'oRDr';
static const uint32 kMsgLogLine			= 'oLog';

// Reader buffer size; openvpn lines are short.
static const size_t kReaderBufferSize	= 2048;

// Connect attempts to the management socket before giving up. openvpn opens
// the listen socket very early but not before we return from posix_spawnp,
// so a short retry loop is enough.
static const int kMgmtConnectAttempts	= 50;	// 5s total at 100ms each
static const bigtime_t kMgmtConnectInterval	= 100000;

// Range of local TCP ports we'll try for the management interface. Picked at
// random within this range; a collision just means the retry-connect below
// fails and we surface an error.
static const int kMgmtPortBase		= 17500;
static const int kMgmtPortSpread	= 200;


extern "C" char** environ;


// --- helpers ---------------------------------------------------------------

static std::string
escape_arg(const BString& value)
{
	// The management protocol is line-oriented: '\n' (and the rare '\r')
	// embedded in an argument would terminate the command early and turn
	// the tail into a second, injected command. We have to escape those in
	// addition to '\' and '"'.
	std::string out;
	const char* s = value.String();
	for (size_t i = 0; s[i] != '\0'; i++) {
		char c = s[i];
		switch (c) {
			case '\\':
			case '"':
				out += '\\';
				out += c;
				break;
			case '\n':
				out += '\\';
				out += 'n';
				break;
			case '\r':
				out += '\\';
				out += 'r';
				break;
			default:
				out += c;
				break;
		}
	}
	return out;
}


// Synchronously run `ifconfig <args...>` (where the variadic list ends in a
// trailing NULL) and wait for it to finish. Returns true on a zero exit
// status. Errors are logged to stderr but don't otherwise propagate -- the
// caller decides whether to surface them as a connection-level error.
static bool
run_ifconfig(const char* const argv[])
{
	pid_t pid = -1;
	int rc = posix_spawnp(&pid, "ifconfig", NULL, NULL,
		(char* const*)argv, environ);
	if (rc != 0) {
		fprintf(stderr,
			"[OpenVPN] spawn(ifconfig) failed: %s\n", strerror(rc));
		return false;
	}
	int status = 0;
	if (waitpid(pid, &status, 0) != pid)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


// Same shape as run_ifconfig but for /bin/route. Used to install and
// uninstall the routes we need to make redirect-gateway actually work on
// Haiku.
static bool
run_route(const char* const argv[])
{
	// Build a printable command for the daemon log so the user can correlate
	// it with what they'd run by hand.
	BString line("[OpenVPN] route");
	for (int i = 1; argv[i] != NULL; i++) {
		line << " ";
		line << argv[i];
	}
	printf("%s\n", line.String());

	pid_t pid = -1;
	int rc = posix_spawnp(&pid, "route", NULL, NULL,
		(char* const*)argv, environ);
	if (rc != 0) {
		fprintf(stderr,
			"[OpenVPN] spawn(route) failed: %s\n", strerror(rc));
		return false;
	}
	int status = 0;
	if (waitpid(pid, &status, 0) != pid)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


// --- OpenVPNBackend --------------------------------------------------------

OpenVPNBackend::OpenVPNBackend()
	:
	VPNBackend("OpenVPNBackend"),
	fState(VPN_STATE_DISCONNECTED),
	fStats(),
	fProfile(),
	fLocalIP(),
	fRemoteIP(),
	fAuthUsername(),
	fAuthPassword(),
	fOrigGateway(),
	fOrigGatewayIface(),
	fTunPeer(),
	fInstalledServerIP(),
	fRoutesInstalled(false),
	fTunInterface(),
	fTunNode(),
	fManagement(),
	fPid(-1),
	fSocket(-1),
	fStderrFd(-1),
	fMgmtPort(0),
	fReader(-1),
	fStderrReader(-1),
	fStopRequested(false)
{
	srand((unsigned)time(NULL) ^ (unsigned)find_thread(NULL));
}


OpenVPNBackend::~OpenVPNBackend()
{
	_Cleanup(true);
}


status_t
OpenVPNBackend::Connect(const VPNProfile& profile)
{
	if (fState != VPN_STATE_DISCONNECTED && fState != VPN_STATE_ERROR)
		return B_NOT_ALLOWED;

	if (Looper() == NULL) {
		// The backend must be attached to the daemon's looper before use, so
		// that BMessenger(this) is addressable from the reader thread.
		return B_NO_INIT;
	}

	if (profile.fConfigPath.Length() == 0) {
		_SetState(VPN_STATE_ERROR, "profile has no .ovpn config path");
		return B_BAD_VALUE;
	}

	fProfile = profile;
	fStats.Reset();
	fLocalIP = "";
	fRemoteIP = profile.fServer;
	fStopRequested = false;

	// Take ownership of the "we're connecting" status BEFORE we do any
	// blocking work below. The check above is not an atomic test-and-set,
	// so a second Connect() arriving while we're still spawning would
	// otherwise sail past it and start a second openvpn process whose
	// pid would overwrite ours -- leaving the first openvpn unreapable.
	// _SetState here also pushes "Connecting" to subscribers immediately
	// instead of after the spawn finishes.
	_SetState(VPN_STATE_CONNECTING);

	printf("[OpenVPN] connect requested: profile='%s' config='%s'\n",
		fProfile.fName.String(), fProfile.fConfigPath.String());

	// Haiku publishes tunnel character devices at /dev/tun/N. ifconfig <name>
	// up publishes that device through the kernel add-on. A previous session
	// can leave a slot in a stuck state where ifconfig refuses to re-add it
	// ("General system error"), so we delete first (idempotent: succeeds even
	// if the slot wasn't there) and only then bring it up. We scan upward
	// until we find a slot that takes, in case multiple slots are stuck.
	//
	// Note: we deliberately do NOT pre-assign an address (e.g. 0.0.0.0)
	// before openvpn starts. With --dev-node /dev/tun/N openvpn opens the
	// device very early; if the interface already has an address bound to
	// it, that open blocks on Haiku and openvpn goes silent forever after
	// `hold release`. Plain `up` works.
	fTunInterface = "";
	fTunNode = "";
	for (int slot = 0; slot < 8; slot++) {
		BString interfaceName;
		interfaceName << "tun/" << slot;

		const char* const ifconfigDelete[] = {
			"ifconfig", "--delete", interfaceName.String(), NULL
		};
		run_ifconfig(ifconfigDelete);

		const char* const ifconfigUp[] = {
			"ifconfig", interfaceName.String(), "up", NULL
		};
		if (run_ifconfig(ifconfigUp)) {
			fTunInterface = interfaceName;
			fTunNode = "";
			fTunNode << "/dev/tun/" << slot;
			printf("[OpenVPN] using %s (%s)\n",
				fTunInterface.String(), fTunNode.String());
			break;
		}
	}
	if (fTunInterface.Length() == 0) {
		_SetState(VPN_STATE_ERROR,
			"no usable tun slot (every /dev/tun/N is stuck; reboot needed?)");
		return B_ERROR;
	}

	if (!_SpawnOpenVPN(fProfile)) {
		_SetState(VPN_STATE_ERROR,
			"could not start openvpn (is it installed?)");
		return B_ERROR;
	}

	if (!_ConnectManagementSocket()) {
		_Cleanup(true);
		_SetState(VPN_STATE_ERROR, "could not reach openvpn management port");
		return B_ERROR;
	}

	// FIRST line on the socket MUST be the random session password from
	// the file we passed via `--management ... <pwfile>`. openvpn closes
	// the connection on any other first input. This is what stops a local
	// process that races us to 127.0.0.1:<fMgmtPort> from driving the
	// tunnel (signal SIGTERM, sniff credentials, swap state).
	_SendCommand(fMgmtPassword.String());

	// Subscribe to state and throughput, then let openvpn move past the
	// management-hold gate. ROUTE_GATEWAY and PUSH_REPLY -- the values
	// _ScanLogLine needs for the route fix-up -- are NOT collected via
	// `log on` (it deadlocks openvpn on Haiku right after `hold release`)
	// but via the stderr pipe we just wired up.
	_SendCommand("state on");
	_SendCommand("bytecount 1");
	_SendCommand("hold release");

	_StartReader();
	_StartStderrReader();
	return B_OK;
}


status_t
OpenVPNBackend::Disconnect()
{
	if (fState == VPN_STATE_DISCONNECTED)
		return B_OK;

	printf("[OpenVPN] disconnect requested\n");
	fStopRequested = true;

	if (fSocket >= 0)
		_SendCommand("signal SIGTERM");
	else if (fPid > 0)
		kill(fPid, SIGTERM);

	return B_OK;
}


VPNState
OpenVPNBackend::State() const
{
	return fState;
}


VPNStats
OpenVPNBackend::Stats() const
{
	return fStats;
}


const char*
OpenVPNBackend::BackendName() const
{
	return "OpenVPN";
}


BString
OpenVPNBackend::LocalIP() const
{
	return fLocalIP;
}


BString
OpenVPNBackend::RemoteIP() const
{
	return fRemoteIP;
}


void
OpenVPNBackend::SetCredentials(const BString& user, const BString& pass)
{
	fAuthUsername = user;
	fAuthPassword = pass;
}


void
OpenVPNBackend::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgInternalEvent:
		{
			OpenVPNEvent event;
			int32 type = OPENVPN_EVENT_UNKNOWN;
			int32 mapped = (int32)VPN_STATE_ERROR;
			int64 bytesIn = 0, bytesOut = 0;
			const char* raw = "";
			const char* stateName = "";
			const char* stateDetail = "";
			const char* localIP = "";
			const char* remoteIP = "";
			const char* realm = "";
			const char* msg = "";

			message->FindInt32("type", &type);
			message->FindInt32("mapped", &mapped);
			message->FindInt64("bytesIn", &bytesIn);
			message->FindInt64("bytesOut", &bytesOut);
			message->FindString("raw", &raw);
			message->FindString("stateName", &stateName);
			message->FindString("stateDetail", &stateDetail);
			message->FindString("localIP", &localIP);
			message->FindString("remoteIP", &remoteIP);
			message->FindString("realm", &realm);
			message->FindString("message", &msg);

			event.type = (OpenVPNEventType)type;
			event.mappedState = (VPNState)mapped;
			event.bytesIn = (uint64_t)bytesIn;
			event.bytesOut = (uint64_t)bytesOut;
			event.raw = raw;
			event.stateName = stateName;
			event.stateDetail = stateDetail;
			event.localIP = localIP;
			event.remoteIP = remoteIP;
			event.realm = realm;
			event.message = msg;

			_HandleManagementEvent(event);
			break;
		}

		case kMsgReaderExited:
			// The reader is gone; tear the rest down and report Disconnected
			// (unless we already landed on ERROR from a FATAL/AUTH_FAILED).
			_Cleanup(true);
			if (fState != VPN_STATE_ERROR)
				_SetState(VPN_STATE_DISCONNECTED);
			break;

		case kMsgLogLine:
		{
			// One line of openvpn's stderr, scraped by the stderr-reader
			// thread. _ScanLogLine has to run on the looper because it
			// mutates fields that _InstallRoutes (also on the looper)
			// reads.
			const char* line = "";
			message->FindString("line", &line);
			if (line != NULL && line[0] != '\0')
				_ScanLogLine(std::string(line));
			break;
		}

		default:
			VPNBackend::MessageReceived(message);
			break;
	}
}


// --- lifecycle -------------------------------------------------------------

bool
OpenVPNBackend::_CreateMgmtPasswordFile()
{
	// 32 hex chars of entropy is plenty for a one-shot local secret.
	char token[33];
	for (int i = 0; i < 32; i++) {
		// Mix two rand() calls and a thread id so that a recently re-seeded
		// rand on an attacker process can't replay last second's token.
		unsigned mixed = (unsigned)rand() ^ (unsigned)(rand() << 4)
			^ (unsigned)find_thread(NULL) ^ (unsigned)system_time();
		token[i] = "0123456789abcdef"[mixed & 0xf];
	}
	token[32] = '\0';
	fMgmtPassword = token;

	// mkstemp() picks a unique name AND opens the fd 0600, which avoids the
	// classic "predictable temp name" race. We write the token + newline
	// (openvpn requires the trailing \n) and unlink on _Cleanup().
	char path[] = "/tmp/sotoportego.mgmtXXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		fprintf(stderr, "[OpenVPN] mkstemp failed: %s\n", strerror(errno));
		fMgmtPassword = "";
		return false;
	}
	std::string line(token);
	line.push_back('\n');
	ssize_t wrote = write(fd, line.data(), line.size());
	close(fd);
	if (wrote != (ssize_t)line.size()) {
		unlink(path);
		fMgmtPassword = "";
		return false;
	}
	fMgmtPasswordFile = path;
	return true;
}


void
OpenVPNBackend::_RemoveMgmtPasswordFile()
{
	if (fMgmtPasswordFile.Length() > 0) {
		unlink(fMgmtPasswordFile.String());
		fMgmtPasswordFile = "";
	}
	fMgmtPassword = "";
}


bool
OpenVPNBackend::_SpawnOpenVPN(const VPNProfile& profile)
{
	// Pick a port now so the same string can go into the argv. A real
	// collision is rare and surfaces as a connect failure below.
	fMgmtPort = kMgmtPortBase + (rand() % kMgmtPortSpread);
	char portStr[16];
	snprintf(portStr, sizeof(portStr), "%d", fMgmtPort);

	// Random per-session password gating the management socket. Without
	// this any local process can connect to 127.0.0.1:<fMgmtPort> and drive
	// openvpn (signal SIGTERM, swap our credentials, read tunnel events).
	if (!_CreateMgmtPasswordFile()) {
		fprintf(stderr,
			"[OpenVPN] could not write management password file\n");
		return false;
	}

	// Build argv. posix_spawnp wants char*; the strings live for the
	// duration of the call only, so casting away const here is fine.
	//
	// --management ... <pwfile> tells openvpn to read the first line of
	// pwfile and require it as the first command on the socket -- so the
	// daemon's first send is the password, NOT 'state on'.
	//
	// --route-noexec keeps openvpn out of the routing table; the Haiku
	// port adds every pushed route on the underlying physical interface
	// (e.g. wifi) instead of on the tun slot, so redirect-gateway routes
	// land on wifi and tunnel traffic disappears. _InstallRoutes()
	// re-applies the right routes once CONNECTED arrives and
	// _ScanLogLine has captured ROUTE_GATEWAY + PUSH_REPLY from the
	// child's stderr.
	//
	// --ifconfig-noexec keeps openvpn from re-running ifconfig on the
	// tunnel device; we already brought the slot up ourselves in
	// Connect() and we re-issue ifconfig with the pushed local/peer IPs
	// in _HandleManagementEvent when CONNECTED arrives.
	char* argv[] = {
		(char*)"openvpn",
		(char*)"--config", (char*)profile.fConfigPath.String(),
		(char*)"--management", (char*)"127.0.0.1", portStr,
			(char*)fMgmtPasswordFile.String(),
		(char*)"--management-hold",
		(char*)"--management-query-passwords",
		(char*)"--route-noexec",
		(char*)"--ifconfig-noexec",
		(char*)"--verb", (char*)"3",
		NULL
	};

	// The daemon is a BApplication and has a non-trivial bag of internal
	// file descriptors open (the BeAPI port / BMessage plumbing). When we
	// spawned openvpn with `posix_spawnp(..., NULL, ...)` those descriptors
	// got inherited, and on Haiku at least one of them makes openvpn go
	// completely silent right after `hold release` -- alive but never
	// emitting >STATE:RESOLVE. Manually driving openvpn from a clean shell
	// makes it proceed fine, which is what isolated the cause to fd
	// inheritance.
	//
	// posix_spawn_file_actions_addclose() can't be used to close every
	// descriptor above stderr because Haiku rejects the whole spawn the
	// moment one of those queued closes hits a descriptor that the parent
	// no longer has open (BApp closes and reopens its internal ports
	// between our scan and the spawn). fork()+execvp() lets the child
	// itself decide which descriptors to close, with no race window.
	struct rlimit rl;
	int maxFd = 1024;
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY)
		maxFd = (int)rl.rlim_cur;

	// Pipe openvpn's stderr back to us so the stderr-reader thread can
	// feed lines into _ScanLogLine() and pick up ROUTE_GATEWAY +
	// PUSH_REPLY. `log on` over management would do the same thing in
	// theory, but on Haiku enabling it just before `hold release`
	// deadlocks openvpn (no >STATE:RESOLVE ever fires).
	int stderrPipe[2];
	if (pipe(stderrPipe) != 0) {
		fprintf(stderr, "[OpenVPN] pipe failed: %s\n", strerror(errno));
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "[OpenVPN] fork failed: %s\n", strerror(errno));
		close(stderrPipe[0]);
		close(stderrPipe[1]);
		return false;
	}
	if (pid == 0) {
		// Child. Anything beyond fd surgery and exec'ing is unsafe in a
		// multi-threaded fork, so keep this section minimal. Redirect
		// stderr to the write end of our pipe BEFORE the close-all loop
		// (the loop would otherwise close the pipe's fds out from under
		// us).
		if (dup2(stderrPipe[1], STDERR_FILENO) < 0)
			_exit(127);
		for (int fd = 3; fd < maxFd; fd++)
			close(fd);
		execvp("openvpn", argv);
		// If we got here, exec failed.
		_exit(127);
	}

	// Parent: close the write end so we get EOF on the read end when
	// openvpn exits. Keep the read end for the stderr-reader thread.
	close(stderrPipe[1]);
	fStderrFd = stderrPipe[0];

	fPid = pid;
	printf("[OpenVPN] spawned openvpn pid=%d mgmt=127.0.0.1:%d\n",
		(int)fPid, fMgmtPort);
	return true;
}


bool
OpenVPNBackend::_ConnectManagementSocket()
{
	for (int attempt = 0; attempt < kMgmtConnectAttempts; attempt++) {
		// If a Disconnect() landed while we were sleeping in the retry
		// loop, abandon the connect attempt. Without this the loop would
		// burn through its full 5s timeout before the cleanup path could
		// run, and the user would see the state stuck on "Connecting"
		// long after they asked to cancel.
		if (fStopRequested)
			return false;

		// If the child died while we were waiting, abort early.
		int status = 0;
		pid_t reaped = waitpid(fPid, &status, WNOHANG);
		if (reaped == fPid) {
			fprintf(stderr,
				"[OpenVPN] child exited before management was ready\n");
			fPid = -1;
			return false;
		}

		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			snooze(kMgmtConnectInterval);
			continue;
		}
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(fMgmtPort);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
			fSocket = fd;
			return true;
		}
		close(fd);
		snooze(kMgmtConnectInterval);
	}
	return false;
}


void
OpenVPNBackend::_StartReader()
{
	fReader = spawn_thread(_ReaderEntry, "openvpn-reader",
		B_NORMAL_PRIORITY, this);
	if (fReader < B_OK) {
		fReader = -1;
		fprintf(stderr, "[OpenVPN] spawn_thread failed\n");
		_Cleanup(true);
		_SetState(VPN_STATE_ERROR, "could not start reader thread");
		return;
	}
	resume_thread(fReader);
}


void
OpenVPNBackend::_StartStderrReader()
{
	if (fStderrFd < 0)
		return;
	fStderrReader = spawn_thread(_StderrReaderEntry, "openvpn-stderr",
		B_NORMAL_PRIORITY, this);
	if (fStderrReader < B_OK) {
		fprintf(stderr, "[OpenVPN] spawn_thread (stderr) failed\n");
		fStderrReader = -1;
		return;
	}
	resume_thread(fStderrReader);
}


int32
OpenVPNBackend::_ReaderEntry(void* self)
{
	return ((OpenVPNBackend*)self)->_RunReaderLoop();
}


int32
OpenVPNBackend::_StderrReaderEntry(void* self)
{
	return ((OpenVPNBackend*)self)->_RunStderrLoop();
}


int32
OpenVPNBackend::_RunStderrLoop()
{
	// Accumulate the byte stream into newline-terminated lines and dispatch
	// each one to the looper. _ScanLogLine writes the fields _InstallRoutes
	// reads, so it MUST run on the looper thread to stay serialised with
	// the management-event handler (which calls _InstallRoutes on
	// CONNECTED).
	char buffer[kReaderBufferSize];
	std::string line;
	while (true) {
		ssize_t got = read(fStderrFd, buffer, sizeof(buffer));
		if (got < 0 && errno == EINTR)
			continue;
		if (got <= 0)
			break;
		for (ssize_t i = 0; i < got; i++) {
			char c = buffer[i];
			if (c == '\n') {
				if (!line.empty()) {
					BMessage message(kMsgLogLine);
					message.AddString("line", line.c_str());
					BMessenger(this).SendMessage(&message);
				}
				line.clear();
			} else if (c != '\r') {
				line += c;
			}
		}
	}
	return B_OK;
}


int32
OpenVPNBackend::_RunReaderLoop()
{
	char buffer[kReaderBufferSize];
	while (true) {
		ssize_t got = recv(fSocket, buffer, sizeof(buffer), 0);
		if (got < 0 && errno == EINTR) {
			// A signal interrupted the syscall; keep listening.
			continue;
		}
		if (got <= 0)
			break;

		std::string chunk(buffer, (size_t)got);
		std::vector<OpenVPNEvent> events = fManagement.Feed(chunk);
		for (size_t i = 0; i < events.size(); i++) {
			// Credentials are answered right here on the I/O thread so we
			// stay responsive to openvpn's prompt; everything else gets
			// dispatched to the looper.
			const OpenVPNEvent& event = events[i];
			if (event.type == OPENVPN_EVENT_PASSWORD_REQUEST
					&& fAuthUsername.Length() > 0) {
				char line[1024];
				snprintf(line, sizeof(line), "username \"%s\" \"%s\"",
					event.realm.c_str(),
					escape_arg(fAuthUsername).c_str());
				_SendCommand(line);
				snprintf(line, sizeof(line), "password \"%s\" \"%s\"",
					event.realm.c_str(),
					escape_arg(fAuthPassword).c_str());
				_SendCommand(line);
			}
			_PostEvent(event);
		}
	}

	BMessenger(this).SendMessage(kMsgReaderExited);
	return B_OK;
}


void
OpenVPNBackend::_Cleanup(bool wait)
{
	if (fSocket >= 0) {
		// Closing kicks any blocked recv() in the reader thread, if it is
		// still alive.
		shutdown(fSocket, SHUT_RDWR);
		close(fSocket);
		fSocket = -1;
	}

	if (fStderrFd >= 0) {
		// Closing makes the stderr-reader's read() return 0 next iteration
		// so the thread can exit without us having to send it a signal.
		close(fStderrFd);
		fStderrFd = -1;
	}

	if (fReader > 0 && wait) {
		status_t exitCode = 0;
		wait_for_thread(fReader, &exitCode);
		fReader = -1;
	}

	if (fStderrReader > 0 && wait) {
		status_t exitCode = 0;
		wait_for_thread(fStderrReader, &exitCode);
		fStderrReader = -1;
	}

	if (fPid > 0) {
		// Give openvpn a moment to wind down on its own; if it doesn't,
		// escalate to SIGTERM and then SIGKILL.
		int status = 0;
		pid_t reaped = -1;
		for (int i = 0; i < 20; i++) {
			reaped = waitpid(fPid, &status, WNOHANG);
			if (reaped == fPid)
				break;
			snooze(100000);
		}
		if (reaped != fPid) {
			kill(fPid, SIGTERM);
			for (int i = 0; i < 10; i++) {
				reaped = waitpid(fPid, &status, WNOHANG);
				if (reaped == fPid)
					break;
				snooze(100000);
			}
		}
		if (reaped != fPid) {
			kill(fPid, SIGKILL);
			waitpid(fPid, &status, 0);
		}
		printf("[OpenVPN] reaped openvpn pid=%d\n", (int)fPid);
		fPid = -1;
	}

	// Drop the routes we installed in _InstallRoutes BEFORE tearing the tun
	// device down, otherwise the route command can fail when the interface
	// is already gone.
	_RemoveRoutes();

	// Tear the tun slot we used out of the interface list entirely.
	// `ifconfig <iface> down` only deactivates it -- the interface stays
	// around with whatever IP openvpn pushed onto it, and the next Connect
	// would inherit a stale address. `--delete` removes the interface; the
	// kernel tunnel add-on republishes it next time we ifconfig it up.
	if (fTunInterface.Length() > 0) {
		const char* const ifconfigDelete[] = {
			"ifconfig", "--delete", fTunInterface.String(), NULL
		};
		run_ifconfig(ifconfigDelete);
	}

	fLocalIP = "";
	fAuthUsername = "";
	fAuthPassword = "";
	fOrigGateway = "";
	fOrigGatewayIface = "";
	fTunPeer = "";
	fTunInterface = "";
	fTunNode = "";
	_RemoveMgmtPasswordFile();
}


// --- protocol --------------------------------------------------------------

void
OpenVPNBackend::_SendCommand(const char* line)
{
	if (fSocket < 0 || line == NULL)
		return;

	// Build the framed line once so a partial send can be resumed from the
	// right offset. Management is line-oriented: if we only send half, the
	// remote interprets a corrupted command and we lose protocol sync.
	size_t length = strlen(line);
	std::string framed;
	framed.reserve(length + 1);
	framed.append(line, length);
	framed.push_back('\n');

	const char* data = framed.data();
	size_t remaining = framed.size();
	while (remaining > 0) {
		// MSG_NOSIGNAL stops a broken pipe from killing the whole daemon
		// when openvpn disappears mid-write; the EPIPE return is enough.
		ssize_t wrote = send(fSocket, data, remaining, MSG_NOSIGNAL);
		if (wrote > 0) {
			data += wrote;
			remaining -= (size_t)wrote;
			continue;
		}
		if (wrote < 0 && errno == EINTR)
			continue;
		// Any other error (EPIPE, ECONNRESET, EBADF, ...) means the socket
		// is gone; the reader thread will see EOF and unwind.
		break;
	}
}


void
OpenVPNBackend::_PostEvent(const OpenVPNEvent& event)
{
	BMessage message(kMsgInternalEvent);
	message.AddInt32("type", (int32)event.type);
	message.AddInt32("mapped", (int32)event.mappedState);
	message.AddInt64("bytesIn", (int64)event.bytesIn);
	message.AddInt64("bytesOut", (int64)event.bytesOut);
	message.AddString("raw", event.raw.c_str());
	message.AddString("stateName", event.stateName.c_str());
	message.AddString("stateDetail", event.stateDetail.c_str());
	message.AddString("localIP", event.localIP.c_str());
	message.AddString("remoteIP", event.remoteIP.c_str());
	message.AddString("realm", event.realm.c_str());
	message.AddString("message", event.message.c_str());
	BMessenger(this).SendMessage(&message);
}


void
OpenVPNBackend::_HandleManagementEvent(const OpenVPNEvent& event)
{
	switch (event.type) {
		case OPENVPN_EVENT_STATE:
			// openvpn sends >STATE:EXITING just before it dies; if we have
			// already recorded a more specific error (auth-failed, fatal),
			// keep that as the visible state instead of overwriting it with
			// a generic Disconnected.
			if (event.mappedState == VPN_STATE_DISCONNECTED
					&& fState == VPN_STATE_ERROR) {
				break;
			}
			if (event.mappedState == VPN_STATE_CONNECTED) {
				fStats.fConnectedSince = time(NULL);
				if (!event.localIP.empty())
					fLocalIP = event.localIP.c_str();
				if (!event.remoteIP.empty())
					fRemoteIP = event.remoteIP.c_str();
				// The Haiku openvpn port runs `/bin/ifconfig tun inet
				// <ip> <peer> ...` itself, but it passes the unqualified
				// "tun" name and ifconfig rejects it with "No such
				// device". --ifconfig-noexec keeps openvpn out of our
				// way; we do the call here with the slot we actually
				// own. fTunPeer comes from the PUSH_REPLY's
				// route-gateway, captured by _ScanLogLine.
				if (fTunInterface.Length() > 0 && fLocalIP.Length() > 0
						&& fTunPeer.Length() > 0) {
					const char* const ifconfigUp[] = {
						"ifconfig", fTunInterface.String(), "inet",
						fLocalIP.String(), fTunPeer.String(),
						"mtu", "1500", "up", NULL
					};
					run_ifconfig(ifconfigUp);
				}
				// All the values needed for the route fix-up have been
				// scanned out of the preceding log lines (ROUTE_GATEWAY
				// and PUSH_REPLY). event.remoteIP is the VPN server's
				// public IP, the third piece we need.
				_InstallRoutes(BString(event.remoteIP.c_str()));
			}
			_SetState(event.mappedState,
				event.stateDetail.empty() ? NULL : event.stateDetail.c_str());
			break;

		case OPENVPN_EVENT_BYTECOUNT:
			fStats.fBytesIn = event.bytesIn;
			fStats.fBytesOut = event.bytesOut;
			NotifyStats(fStats);
			break;

		case OPENVPN_EVENT_PASSWORD_REQUEST:
			printf("[OpenVPN] credentials requested for realm '%s'\n",
				event.realm.c_str());
			if (fAuthUsername.Length() == 0) {
				// No credentials in hand and the daemon has no UI of its
				// own; openvpn will time out and exit, which we'll surface
				// via the FATAL / process-exit path.
				_SetState(VPN_STATE_AUTHENTICATING,
					"credentials required but none provided");
			}
			break;

		case OPENVPN_EVENT_AUTH_FAILED:
			_SetState(VPN_STATE_ERROR, "authentication failed");
			break;

		case OPENVPN_EVENT_FATAL:
			_SetState(VPN_STATE_ERROR,
				event.message.empty() ? "fatal error" : event.message.c_str());
			break;

		case OPENVPN_EVENT_LOG:
		case OPENVPN_EVENT_INFO:
			if (!event.message.empty()) {
				printf("[OpenVPN] %s\n", event.message.c_str());
				_ScanLogLine(event.message);
			}
			break;

		default:
			break;
	}
}


void
OpenVPNBackend::_SetState(VPNState state, const char* detail)
{
	fState = state;
	printf("[OpenVPN] state -> %s%s%s\n", vpn_state_name(state),
		detail != NULL ? ": " : "", detail != NULL ? detail : "");
	NotifyStateChanged(state, detail);
}


// --- log scanning and route fix-up ----------------------------------------

// Lift the first whitespace-delimited token starting at `pos` out of `s`.
// Stops at any of " \t,'\"". Returns the token (possibly empty).
static std::string
take_token(const std::string& s, size_t pos)
{
	while (pos < s.length() && (s[pos] == ' ' || s[pos] == '\t'))
		pos++;
	size_t end = pos;
	while (end < s.length()) {
		char c = s[end];
		if (c == ' ' || c == '\t' || c == ',' || c == '\'' || c == '"'
				|| c == '\n' || c == '\r') {
			break;
		}
		end++;
	}
	return s.substr(pos, end - pos);
}


void
OpenVPNBackend::_ScanLogLine(const std::string& line)
{
	// openvpn prints the underlying default route once it has worked out
	// where to send tunnel packets:
	//   ROUTE_GATEWAY 192.168.188.1 IFACE=/dev/net/iprowifi4965/0
	if (fOrigGateway.Length() == 0) {
		size_t pos = line.find("ROUTE_GATEWAY ");
		if (pos != std::string::npos) {
			std::string gw = take_token(line, pos + 14);
			size_t ifacePos = line.find("IFACE=", pos);
			std::string iface;
			if (ifacePos != std::string::npos)
				iface = take_token(line, ifacePos + 6);
			if (!gw.empty() && !iface.empty()) {
				fOrigGateway = gw.c_str();
				fOrigGatewayIface = iface.c_str();
				printf("[OpenVPN] captured default route: %s via %s\n",
					fOrigGateway.String(), fOrigGatewayIface.String());
			}
		}
	}

	// The PUSH_REPLY carries the in-tunnel peer/gateway under a comma-
	// separated key/value pair like "route-gateway 10.245.243.246". The
	// PUSH_REPLY message also contains commas, which is why we needed the
	// LOG-parser fix in OpenVPNManagement; the substring search here is
	// agnostic to surrounding noise.
	if (fTunPeer.Length() == 0) {
		size_t pos = line.find("route-gateway ");
		if (pos != std::string::npos) {
			std::string peer = take_token(line, pos + 14);
			if (!peer.empty()) {
				fTunPeer = peer.c_str();
				printf("[OpenVPN] captured tunnel peer: %s\n",
					fTunPeer.String());
			}
		}
	}
}


void
OpenVPNBackend::_InstallRoutes(const BString& vpnServerIP)
{
	if (fRoutesInstalled)
		return;

	// All three values are needed -- without them we can't even guess at the
	// right route commands, so refuse rather than do something destructive.
	if (fOrigGateway.Length() == 0 || fOrigGatewayIface.Length() == 0
			|| fTunPeer.Length() == 0 || vpnServerIP.Length() == 0) {
		fprintf(stderr,
			"[OpenVPN] missing routing info, skipping route fix-up "
			"(server=%s gw=%s iface=%s peer=%s)\n",
			vpnServerIP.String(),
			fOrigGateway.String(),
			fOrigGatewayIface.String(),
			fTunPeer.String());
		return;
	}

	// 1) Pin the VPN server to the underlying default route so openvpn's
	// UDP/TCP packets don't try to flow back through the tunnel they're
	// carrying.
	const char* const serverPin[] = {
		"route", "add", fOrigGatewayIface.String(), "inet",
		vpnServerIP.String(), "gw", fOrigGateway.String(),
		"netmask", "255.255.255.255", NULL
	};
	run_route(serverPin);

	// 2,3) Replace the system default route with two /1 routes pointing at
	// the tunnel peer over the tun interface this session is using. Two
	// halves win over the existing 0.0.0.0/0 by longest-prefix match, so
	// we don't have to touch the original default.
	const char* const lower[] = {
		"route", "add", fTunInterface.String(), "inet", "0.0.0.0",
		"gw", fTunPeer.String(), "netmask", "128.0.0.0", NULL
	};
	const char* const upper[] = {
		"route", "add", fTunInterface.String(), "inet", "128.0.0.0",
		"gw", fTunPeer.String(), "netmask", "128.0.0.0", NULL
	};
	run_route(lower);
	run_route(upper);

	fInstalledServerIP = vpnServerIP;
	fRoutesInstalled = true;
}


void
OpenVPNBackend::_RemoveRoutes()
{
	if (!fRoutesInstalled)
		return;

	const char* const upper[] = {
		"route", "delete", fTunInterface.String(), "inet", "128.0.0.0",
		"gw", fTunPeer.String(), "netmask", "128.0.0.0", NULL
	};
	const char* const lower[] = {
		"route", "delete", fTunInterface.String(), "inet", "0.0.0.0",
		"gw", fTunPeer.String(), "netmask", "128.0.0.0", NULL
	};
	const char* const serverPin[] = {
		"route", "delete", fOrigGatewayIface.String(), "inet",
		fInstalledServerIP.String(), "gw", fOrigGateway.String(),
		"netmask", "255.255.255.255", NULL
	};
	run_route(upper);
	run_route(lower);
	run_route(serverPin);

	fRoutesInstalled = false;
	fInstalledServerIP = "";
}
