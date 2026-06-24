/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H


#include <Messenger.h>
#include <String.h>
#include <Window.h>

#include "VPNState.h"

class BButton;
class BListView;
class BStringView;
class StatusIndicator;


// The main Sotoportego window. Like the CLI, it is purely a client of the
// daemon: it subscribes over BMessage, reflects the broadcast state/stats, and
// sends connect/disconnect requests. No VPN logic lives here.
class MainWindow : public BWindow {
public:
								MainWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_BuildLayout();
			void				_EnsureSubscribed();
			void				_SendConnect();
			void				_SendDisconnect();
			void				_UpdateForState(VPNState state,
									const char* detail);
			void				_ApplyStats(const BMessage* message);

	static	BString				_FormatBytes(int64 bytes);

			BMessenger			fServer;
			VPNState			fState;

			StatusIndicator*	fIndicator;
			BStringView*		fStatusLabel;
			BStringView*		fServerLabel;
			BStringView*		fSinceValue;
			BStringView*		fDownValue;
			BStringView*		fUpValue;
			BListView*			fProfileList;
			BButton*			fActionButton;
};


#endif	// MAIN_WINDOW_H
