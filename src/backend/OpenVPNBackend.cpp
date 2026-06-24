/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "OpenVPNBackend.h"

#include <stdio.h>
#include <time.h>

#include <Looper.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Messenger.h>

#include "VPNProtocol.h"


// Internal timer tick. Private to this backend, so it lives here rather than
// in the public protocol header.
static const uint32 kMsgInternalTick = 'oTik';

// How often the scripted state machine advances / emits a throughput tick.
static const bigtime_t kTickInterval = 800000;	// 0.8s

// Fake throughput added per tick once "connected", just so clients have
// something moving to display.
static const int64 kFakeBytesInPerTick = 12 * 1024;
static const int64 kFakeBytesOutPerTick = 3 * 1024;


OpenVPNBackend::OpenVPNBackend()
	:
	VPNBackend("OpenVPNBackend"),
	fState(VPN_STATE_DISCONNECTED),
	fStats(),
	fProfile(),
	fTimer(NULL)
{
}


OpenVPNBackend::~OpenVPNBackend()
{
	_StopTimer();
}


status_t
OpenVPNBackend::Connect(const VPNProfile& profile)
{
	if (fState != VPN_STATE_DISCONNECTED && fState != VPN_STATE_ERROR)
		return B_NOT_ALLOWED;

	if (Looper() == NULL) {
		// The backend must be attached to the daemon's looper before use, so
		// that its timer can target it.
		return B_NO_INIT;
	}

	fProfile = profile;
	fStats.Reset();

	printf("[OpenVPN] connect requested: profile='%s' server='%s:%u'\n",
		fProfile.fName.String(), fProfile.fServer.String(), fProfile.fPort);

	// TODO(milestone-2): launch the openvpn process here and attach to its
	// management interface instead of starting a scripted timer.
	_SetState(VPN_STATE_CONNECTING);
	_StartTimer(kTickInterval);

	return B_OK;
}


status_t
OpenVPNBackend::Disconnect()
{
	if (fState == VPN_STATE_DISCONNECTED)
		return B_OK;

	printf("[OpenVPN] disconnect requested\n");

	// TODO(milestone-2): send 'signal SIGTERM' over the management interface
	// and wait for the process to exit before reporting Disconnected.
	_StopTimer();
	_SetState(VPN_STATE_DISCONNECTED);

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


void
OpenVPNBackend::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgInternalTick:
			_Tick();
			break;

		default:
			VPNBackend::MessageReceived(message);
			break;
	}
}


// Advance the scripted state machine one step. Real backends would instead
// react to events parsed from the OpenVPN management interface.
void
OpenVPNBackend::_Tick()
{
	switch (fState) {
		case VPN_STATE_CONNECTING:
			_SetState(VPN_STATE_AUTHENTICATING);
			break;

		case VPN_STATE_AUTHENTICATING:
			fStats.fConnectedSince = time(NULL);
			_SetState(VPN_STATE_CONNECTED);
			break;

		case VPN_STATE_CONNECTED:
		case VPN_STATE_RECONNECTING:
			// Pretend traffic is flowing and publish a throughput update.
			fStats.fBytesIn += kFakeBytesInPerTick;
			fStats.fBytesOut += kFakeBytesOutPerTick;
			NotifyStats(fStats);
			break;

		default:
			// Nothing scheduled in terminal states; stop ticking.
			_StopTimer();
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


void
OpenVPNBackend::_StartTimer(bigtime_t interval)
{
	_StopTimer();

	BMessage tick(kMsgInternalTick);
	// Target ourselves; we are attached to the daemon's looper.
	fTimer = new BMessageRunner(BMessenger(this), &tick, interval);
	if (fTimer->InitCheck() != B_OK) {
		delete fTimer;
		fTimer = NULL;
		_SetState(VPN_STATE_ERROR, "failed to start timer");
	}
}


void
OpenVPNBackend::_StopTimer()
{
	delete fTimer;
	fTimer = NULL;
}
