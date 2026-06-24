/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef OPENVPN_BACKEND_H
#define OPENVPN_BACKEND_H


#include "VPNBackend.h"

class BMessageRunner;


// Stub OpenVPN backend for Milestone 1.
//
// This does NOT talk to OpenVPN. It exists to exercise the IPC + backend seam
// end to end: on Connect() it walks a scripted state machine driven by a
// BMessageRunner timer (Connecting -> Authenticating -> Connected, then fake
// throughput ticks), emitting the same events a real backend would. On
// Disconnect() it unwinds to Disconnected.
//
// TODO(milestone-2): replace the timer-driven script with a real connection
// managed through the OpenVPN management interface (spawn the openvpn binary,
// drive it over its management socket, parse state/bytecount events).
class OpenVPNBackend : public VPNBackend {
public:
								OpenVPNBackend();
	virtual						~OpenVPNBackend();

	virtual	status_t			Connect(const VPNProfile& profile);
	virtual	status_t			Disconnect();
	virtual	VPNState			State() const;
	virtual	VPNStats			Stats() const;
	virtual	const char*			BackendName() const;

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_SetState(VPNState state,
									const char* detail = NULL);
			void				_Tick();
			void				_StartTimer(bigtime_t interval);
			void				_StopTimer();

			VPNState			fState;
			VPNStats			fStats;
			VPNProfile			fProfile;
			BMessageRunner*		fTimer;
};


#endif	// OPENVPN_BACKEND_H
