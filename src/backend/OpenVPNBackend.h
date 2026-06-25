/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef OPENVPN_BACKEND_H
#define OPENVPN_BACKEND_H


#include <sys/types.h>

#include <OS.h>
#include <String.h>

#include "OpenVPNManagement.h"
#include "VPNBackend.h"


// Real OpenVPN backend.
//
// On Connect() it spawns the `openvpn` binary with `--management 127.0.0.1
// <port> --management-hold --management-query-passwords --config <ovpn>`,
// connects a TCP socket to that management port, and starts a reader thread
// that streams bytes through OpenVPNManagement::Feed(). Every parsed event is
// posted back to the daemon's looper via a private BMessage so all backend
// state mutations happen on the looper thread (no locks).
//
// On Disconnect() it asks openvpn to terminate via `signal SIGTERM` on the
// management socket; the child's exit closes the socket, the reader sees
// EOF, the looper reaps the team and the state machine settles on
// Disconnected.
class OpenVPNBackend : public VPNBackend {
public:
								OpenVPNBackend();
	virtual						~OpenVPNBackend();

	virtual	status_t			Connect(const VPNProfile& profile);
	virtual	status_t			Disconnect();
	virtual	VPNState			State() const;
	virtual	VPNStats			Stats() const;
	virtual	const char*			BackendName() const;
	virtual	BString				LocalIP() const;
	virtual	BString				RemoteIP() const;
	virtual	void				SetCredentials(const BString& user,
									const BString& pass);

	virtual	void				MessageReceived(BMessage* message);

private:
	// --- lifecycle -----------------------------------------------------
			bool				_SpawnOpenVPN(const VPNProfile& profile);
			bool				_ConnectManagementSocket();
			void				_StartReader();
			void				_Cleanup(bool wait);
			int32				_RunReaderLoop();
	static	int32				_ReaderEntry(void* self);

	// --- protocol ------------------------------------------------------
			void				_SendCommand(const char* line);
			void				_PostEvent(const OpenVPNEvent& event);
			void				_HandleManagementEvent(
									const OpenVPNEvent& event);

	// Scan a single openvpn log line for the values our route fix needs:
	// ROUTE_GATEWAY (the original gateway + iface) and PUSH_REPLY's
	// route-gateway (the in-tunnel peer).
			void				_ScanLogLine(const std::string& line);

	// Routing fix-up. The Haiku openvpn port hardcodes rgi->iface for
	// every route it adds, so the pushed default route ends up on the
	// wifi interface and packets disappear. We pass --route-noexec and
	// install the right routes ourselves once the tunnel is up.
			void				_InstallRoutes(const BString& vpnServerIP);
			void				_RemoveRoutes();

			void				_SetState(VPNState state,
									const char* detail = NULL);

			VPNState			fState;
			VPNStats			fStats;
			VPNProfile			fProfile;
			BString				fLocalIP;
			BString				fRemoteIP;
			BString				fAuthUsername;
			BString				fAuthPassword;
	// Values harvested from openvpn's log stream and used by _InstallRoutes.
	// `fOrigGatewayIface` is the path the kernel's route command wants
	// (e.g. "/dev/net/iprowifi4965/0"); `fTunPeer` is the in-tunnel peer
	// IP that the pushed default route should target.
			BString				fOrigGateway;
			BString				fOrigGatewayIface;
			BString				fTunPeer;
			BString				fInstalledServerIP;
			bool				fRoutesInstalled;
			OpenVPNManagement	fManagement;

	// Live process / connection. Owned by Connect()/_Cleanup().
			pid_t				fPid;			// openvpn pid, -1 when none
			int					fSocket;		// management TCP fd, -1 when none
			int					fMgmtPort;		// chosen at spawn time
			thread_id			fReader;		// -1 when no thread alive
			bool				fStopRequested;	// true after Disconnect()
};


#endif	// OPENVPN_BACKEND_H
