/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef VPN_MAP_WINDOW_H
#define VPN_MAP_WINDOW_H


#include <Window.h>

class BButton;
class BStringView;
class MapView;


// Standalone window that hosts the world MapView and a side panel
// describing whichever server pin is currently selected. The eventual
// goal is a one-click way to connect to a public VPNGate server: pick a
// pin, see its host / country / ping / score / load, hit Connect, the
// daemon fetches the .ovpn body for that host and reuses the existing
// OpenVPN flow.
//
// This first cut is the wiring shell: the toolbar and side panel are
// real, but the catalogue is a hardcoded handful of test pins so the
// rendering, selection and panel-refresh paths can be exercised before
// the daemon-side VPNGate fetcher lands (tracked separately).
//
// Like the rest of the GUI, the window is just another BMessage client
// of the daemon: future revisions will subscribe to kMsgVPNGateList for
// the catalogue, and the Connect button will dispatch a
// kMsgConnectVPNGate request, but neither protocol code exists yet.
class VPNMapWindow : public BWindow {
public:
								VPNMapWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_BuildLayout();
			void				_SeedTestPins();
			void				_RefreshSidePanel();

			MapView*			fMap;

			BStringView*		fHostValue;
			BStringView*		fCountryValue;
			BStringView*		fPingValue;
			BStringView*		fScoreValue;
			BStringView*		fSessionsValue;
			BStringView*		fLogPolicyValue;
			BStringView*		fStatusBar;
			BButton*			fConnectButton;
};


#endif	// VPN_MAP_WINDOW_H
