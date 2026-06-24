/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "MainWindow.h"

#include <stdio.h>
#include <time.h>

#include <Application.h>
#include <Box.h>
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <OS.h>
#include <Roster.h>
#include <ScrollView.h>
#include <StringView.h>

#include "StatusIndicator.h"
#include "VPNProfile.h"
#include "VPNProtocol.h"
#include "VPNStats.h"


// GUI-private message codes.
static const uint32 kMsgPrimaryAction	= 'gAct';
static const uint32 kMsgConnectAction	= 'gCon';
static const uint32 kMsgDisconnectAction = 'gDis';

// The single semantic accent, one color per state (see docs/GUI.md).
static const rgb_color kColorIdle		= { 0x7f, 0x7f, 0x7f, 0xff };
static const rgb_color kColorProgress	= { 0xe0, 0xa0, 0x30, 0xff };
static const rgb_color kColorConnected	= { 0x3d, 0xa3, 0x5d, 0xff };
static const rgb_color kColorError		= { 0xc8, 0x46, 0x3c, 0xff };


MainWindow::MainWindow()
	:
	BWindow(BRect(100, 100, 660, 470), "Sotoportego", B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fServer(),
	fState(VPN_STATE_DISCONNECTED),
	fIndicator(NULL),
	fStatusLabel(NULL),
	fServerLabel(NULL),
	fSinceValue(NULL),
	fDownValue(NULL),
	fUpValue(NULL),
	fProfileList(NULL),
	fActionButton(NULL)
{
	_BuildLayout();
	_UpdateForState(VPN_STATE_DISCONNECTED, NULL);
	_EnsureSubscribed();
}


void
MainWindow::_BuildLayout()
{
	BMenuBar* menuBar = new BMenuBar("menubar");

	BMenu* appMenu = new BMenu("App");
	appMenu->AddItem(new BMenuItem("About Sotoportego" B_UTF8_ELLIPSIS,
		new BMessage(B_ABOUT_REQUESTED)));
	appMenu->AddSeparatorItem();
	appMenu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(appMenu);

	BMenu* connectionMenu = new BMenu("Connection");
	connectionMenu->AddItem(new BMenuItem("Connect",
		new BMessage(kMsgConnectAction)));
	connectionMenu->AddItem(new BMenuItem("Disconnect",
		new BMessage(kMsgDisconnectAction)));
	menuBar->AddItem(connectionMenu);

	// --- Left: profiles -------------------------------------------------
	BBox* profilesBox = new BBox("profilesBox");
	profilesBox->SetLabel("Profiles");

	fProfileList = new BListView("profileList");
	fProfileList->AddItem(new BStringItem("Demo Profile"));
	fProfileList->Select(0);
	BScrollView* listScroll = new BScrollView("profileScroll", fProfileList,
		0, false, true);

	// Add/remove are placeholders until profile management lands.
	BButton* addButton = new BButton("addProfile", "+", NULL);
	BButton* removeButton = new BButton("removeProfile", "\xe2\x80\x93", NULL);
	addButton->SetEnabled(false);
	removeButton->SetEnabled(false);

	BLayoutBuilder::Group<>(profilesBox, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_INSETS,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(listScroll)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(addButton)
			.Add(removeButton)
			.AddGlue()
		.End();

	// --- Right: status + session + action -------------------------------
	fIndicator = new StatusIndicator("indicator");

	fStatusLabel = new BStringView("statusLabel", "Disconnected");
	BFont bigFont(be_bold_font);
	bigFont.SetSize(bigFont.Size() * 1.6f);
	fStatusLabel->SetFont(&bigFont);

	fServerLabel = new BStringView("serverLabel", "\xe2\x80\x94");

	BBox* sessionBox = new BBox("sessionBox");
	sessionBox->SetLabel("Session");
	fSinceValue = new BStringView("sinceValue", "\xe2\x80\x94");
	fDownValue = new BStringView("downValue", "0 B");
	fUpValue = new BStringView("upValue", "0 B");

	BLayoutBuilder::Grid<>(sessionBox, B_USE_DEFAULT_SPACING,
			B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_INSETS,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(new BStringView("sinceLabel", "Since"), 0, 0)
		.Add(fSinceValue, 1, 0)
		.Add(new BStringView("downLabel", "Download"), 0, 1)
		.Add(fDownValue, 1, 1)
		.Add(new BStringView("upLabel", "Upload"), 0, 2)
		.Add(fUpValue, 1, 2);

	fActionButton = new BButton("actionButton", "Connect",
		new BMessage(kMsgPrimaryAction));
	fActionButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(profilesBox, 0.38f)
			.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING, 0.62f)
				.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
					.Add(fIndicator)
					.AddGroup(B_VERTICAL, 0)
						.Add(fStatusLabel)
						.Add(fServerLabel)
					.End()
					.AddGlue()
				.End()
				.Add(sessionBox)
				.AddGlue()
				.AddGroup(B_HORIZONTAL, 0)
					.AddGlue()
					.Add(fActionButton)
				.End()
			.End()
		.End();
}


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgPrimaryAction:
			// The primary button means connect when idle, disconnect otherwise.
			if (fState == VPN_STATE_DISCONNECTED || fState == VPN_STATE_ERROR)
				_SendConnect();
			else
				_SendDisconnect();
			break;

		case kMsgConnectAction:
			_SendConnect();
			break;
		case kMsgDisconnectAction:
			_SendDisconnect();
			break;

		case B_ABOUT_REQUESTED:
			be_app->PostMessage(B_ABOUT_REQUESTED);
			break;

		case kMsgStatusUpdate:
		{
			int32 state = VPN_STATE_DISCONNECTED;
			message->FindInt32(kFieldState, &state);
			const char* detail = NULL;
			if (message->FindString(kFieldDetail, &detail) != B_OK)
				detail = NULL;
			_UpdateForState((VPNState)state, detail);
			_ApplyStats(message);
			break;
		}

		case kMsgStatsUpdate:
			_ApplyStats(message);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
MainWindow::_EnsureSubscribed()
{
	// Launch the daemon if needed, then subscribe and pull the current status.
	if (be_roster->Launch(kServerSignature) != B_OK
			&& be_roster->Launch(kServerSignature) != B_ALREADY_RUNNING) {
		// Best effort; the messenger check below decides whether we proceed.
	}

	for (int attempt = 0; attempt < 30; attempt++) {
		fServer = BMessenger(kServerSignature);
		if (fServer.IsValid())
			break;
		snooze(100000);
	}

	if (!fServer.IsValid())
		return;

	BMessage subscribe(kMsgSubscribe);
	subscribe.AddMessenger(kFieldClient, BMessenger(this));
	fServer.SendMessage(&subscribe);

	BMessage status(kMsgGetStatus);
	status.AddMessenger(kFieldClient, BMessenger(this));
	fServer.SendMessage(&status);
}


void
MainWindow::_SendConnect()
{
	if (!fServer.IsValid())
		return;

	// TODO: build this from the selected VPNProfile once profile management and
	// .ovpn parsing land; for now it mirrors the CLI's demo profile.
	VPNProfile profile;
	profile.fBackendType = VPN_BACKEND_OPENVPN;
	profile.fName = "Demo Profile";
	profile.fServer = "vpn.example.com";
	profile.fPort = 1194;
	profile.fUsername = "demo";

	BMessage archive;
	profile.Archive(&archive);

	BMessage connect(kMsgConnect);
	connect.AddMessenger(kFieldClient, BMessenger(this));
	connect.AddMessage(kFieldProfile, &archive);
	fServer.SendMessage(&connect);
}


void
MainWindow::_SendDisconnect()
{
	if (!fServer.IsValid())
		return;

	BMessage disconnect(kMsgDisconnect);
	disconnect.AddMessenger(kFieldClient, BMessenger(this));
	fServer.SendMessage(&disconnect);
}


void
MainWindow::_UpdateForState(VPNState state, const char* detail)
{
	fState = state;

	rgb_color accent = kColorIdle;
	const char* action = "Connect";
	bool actionEnabled = true;

	switch (state) {
		case VPN_STATE_CONNECTING:
		case VPN_STATE_AUTHENTICATING:
			accent = kColorProgress;
			action = "Disconnect";
			break;
		case VPN_STATE_CONNECTED:
			accent = kColorConnected;
			action = "Disconnect";
			break;
		case VPN_STATE_RECONNECTING:
			accent = kColorProgress;
			action = "Disconnect";
			break;
		case VPN_STATE_ERROR:
			accent = kColorError;
			action = "Connect";
			break;
		case VPN_STATE_DISCONNECTED:
		default:
			accent = kColorIdle;
			action = "Connect";
			break;
	}

	if (fIndicator != NULL)
		fIndicator->SetColor(accent);

	if (fStatusLabel != NULL) {
		BString label(vpn_state_name(state));
		if (detail != NULL && detail[0] != '\0') {
			label << " \xe2\x80\x94 ";
			label << detail;
		}
		fStatusLabel->SetText(label.String());
	}

	if (fServerLabel != NULL) {
		// Show the server only when there is an active session.
		if (state == VPN_STATE_DISCONNECTED)
			fServerLabel->SetText("\xe2\x80\x94");
		else
			fServerLabel->SetText("vpn.example.com:1194");
	}

	if (fActionButton != NULL) {
		fActionButton->SetLabel(action);
		fActionButton->SetEnabled(actionEnabled);
	}
}


void
MainWindow::_ApplyStats(const BMessage* message)
{
	VPNStats stats;
	stats.Unarchive(*message);

	if (fDownValue != NULL)
		fDownValue->SetText(_FormatBytes(stats.fBytesIn).String());
	if (fUpValue != NULL)
		fUpValue->SetText(_FormatBytes(stats.fBytesOut).String());

	if (fSinceValue != NULL) {
		if (stats.fConnectedSince > 0) {
			char buffer[32];
			struct tm local;
			time_t when = stats.fConnectedSince;
			localtime_r(&when, &local);
			strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);
			fSinceValue->SetText(buffer);
		} else {
			fSinceValue->SetText("\xe2\x80\x94");
		}
	}
}


BString
MainWindow::_FormatBytes(int64 bytes)
{
	const char* units[] = { "B", "KB", "MB", "GB", "TB" };
	double value = (double)bytes;
	int unit = 0;
	while (value >= 1024.0 && unit < 4) {
		value /= 1024.0;
		unit++;
	}

	char buffer[48];
	if (unit == 0)
		snprintf(buffer, sizeof(buffer), "%lld B", (long long)bytes);
	else
		snprintf(buffer, sizeof(buffer), "%.1f %s", value, units[unit]);

	return BString(buffer);
}
