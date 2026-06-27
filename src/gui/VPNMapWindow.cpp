/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "VPNMapWindow.h"

#include <stdio.h>

#include <Alert.h>
#include <Box.h>
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <StringView.h>

#include "MapView.h"


// --- toolbar message codes -------------------------------------------------

static const uint32 kMsgConnectPicked	= 'mwCo';
static const uint32 kMsgRefresh			= 'mwRf';


// --- test catalogue --------------------------------------------------------

// Hardcoded list of ~10 servers placed in real cities so the world map
// shows a recognisable spread (Tokyo + Osaka show the Japan-heavy bias
// that motivates the clustering work in task #15). When the real
// VPNGate fetcher lands, this seeds away and the catalogue arrives on
// kMsgVPNGateList.
struct TestServer {
	const char*	host;
	const char*	countryShort;
	const char*	countryLong;
	const char*	logPolicy;
	float		latitude;
	float		longitude;
	int32		score;
	int32		pingMs;
	int32		speedMbps;
	int32		sessions;
};

static const TestServer kTestServers[] = {
	{ "public-vpn-219.opengw.net",	"JP", "Japan",			"2 weeks",
		35.6762f,  139.6503f,	9821,	18,	112,	83 },
	{ "vpn100383739.opengw.net",	"JP", "Japan",			"2 weeks",
		34.6937f,  135.5023f,	7402,	24,	74,		61 },
	{ "vpn-kr-01.opengw.net",		"KR", "Korea, South",	"30 days",
		37.5665f,  126.9780f,	5210,	38,	61,		44 },
	{ "vpn-sg-02.opengw.net",		"SG", "Singapore",		"7 days",
		1.3521f,   103.8198f,	4900,	72,	58,		39 },
	{ "vpn-de-fr.opengw.net",		"DE", "Germany",		"No log",
		50.1109f,  8.6821f,		6320,	95,	88,		57 },
	{ "vpn-fr-par.opengw.net",		"FR", "France",			"No log",
		48.8566f,  2.3522f,		5780,	88,	76,		51 },
	{ "vpn-uk-lon.opengw.net",		"GB", "United Kingdom",	"No log",
		51.5074f,  -0.1278f,	5990,	84,	81,		54 },
	{ "vpn-se-sto.opengw.net",		"SE", "Sweden",			"No log",
		59.3293f,  18.0686f,	4520,	112,	60,		31 },
	{ "vpn-us-nyc.opengw.net",		"US", "United States",	"30 days",
		40.7128f,  -74.0060f,	8120,	125,	102,	72 },
	{ "vpn-ca-tor.opengw.net",		"CA", "Canada",			"30 days",
		43.6532f,  -79.3832f,	4710,	131,	66,	38 },
};
static const size_t kTestServerCount
	= sizeof(kTestServers) / sizeof(kTestServers[0]);


// --- VPNMapWindow ----------------------------------------------------------

VPNMapWindow::VPNMapWindow()
	:
	BWindow(BRect(120, 100, 1080, 720),
		"Browse servers on map",
		B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fMap(NULL),
	fHostValue(NULL),
	fCountryValue(NULL),
	fPingValue(NULL),
	fScoreValue(NULL),
	fSessionsValue(NULL),
	fLogPolicyValue(NULL),
	fStatusBar(NULL),
	fConnectButton(NULL)
{
	_BuildLayout();
	_SeedTestPins();
	_RefreshSidePanel();
}


void
VPNMapWindow::_BuildLayout()
{
	// --- menu bar -------------------------------------------------------
	BMenuBar* menuBar = new BMenuBar("menubar");

	BMenu* mapMenu = new BMenu("Map");
	mapMenu->AddItem(new BMenuItem("Zoom in",
		new BMessage(kMsgZoomIn), '+'));
	mapMenu->AddItem(new BMenuItem("Zoom out",
		new BMessage(kMsgZoomOut), '-'));
	mapMenu->AddItem(new BMenuItem("Fit to pins",
		new BMessage(kMsgZoomFit), 'F'));
	mapMenu->AddSeparatorItem();
	mapMenu->AddItem(new BMenuItem("Toggle background tiles",
		new BMessage(kMsgToggleTiles), 'T'));
	mapMenu->AddItem(new BMenuItem("Refresh server list",
		new BMessage(kMsgRefresh), 'R'));
	mapMenu->AddSeparatorItem();
	mapMenu->AddItem(new BMenuItem("Close",
		new BMessage(B_QUIT_REQUESTED), 'W'));
	menuBar->AddItem(mapMenu);

	// --- map ------------------------------------------------------------
	fMap = new MapView("worldMap");

	// --- side panel: server details -------------------------------------
	BBox* detailsBox = new BBox("serverDetails");
	detailsBox->SetLabel("Server");

	fHostValue = new BStringView("hostValue", "\xe2\x80\x94");
	fHostValue->SetFont(be_bold_font);
	fCountryValue = new BStringView("countryValue", "\xe2\x80\x94");
	fCountryValue->SetFont(be_bold_font);
	fPingValue = new BStringView("pingValue", "\xe2\x80\x94");
	fPingValue->SetFont(be_bold_font);
	fScoreValue = new BStringView("scoreValue", "\xe2\x80\x94");
	fScoreValue->SetFont(be_bold_font);
	fSessionsValue = new BStringView("sessionsValue", "\xe2\x80\x94");
	fSessionsValue->SetFont(be_bold_font);
	fLogPolicyValue = new BStringView("logPolicyValue", "\xe2\x80\x94");
	fLogPolicyValue->SetFont(be_bold_font);

	BLayoutBuilder::Grid<>(detailsBox, B_USE_DEFAULT_SPACING,
			B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_INSETS,
			B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(new BStringView("hostCaption", "Host:"), 0, 0)
		.Add(fHostValue, 1, 0)
		.Add(new BStringView("countryCaption", "Country:"), 0, 1)
		.Add(fCountryValue, 1, 1)
		.Add(new BStringView("pingCaption", "Ping:"), 0, 2)
		.Add(fPingValue, 1, 2)
		.Add(new BStringView("scoreCaption", "Score:"), 0, 3)
		.Add(fScoreValue, 1, 3)
		.Add(new BStringView("sessionsCaption", "Sessions:"), 0, 4)
		.Add(fSessionsValue, 1, 4)
		.Add(new BStringView("logPolicyCaption", "Log policy:"), 0, 5)
		.Add(fLogPolicyValue, 1, 5);

	fConnectButton = new BButton("connectPicked", "Connect",
		new BMessage(kMsgConnectPicked));
	fConnectButton->MakeDefault(true);
	fConnectButton->SetEnabled(false);

	// --- status bar -----------------------------------------------------
	char status[64];
	snprintf(status, sizeof(status), "%zu server(s) loaded",
		kTestServerCount);
	fStatusBar = new BStringView("statusBar", status);
	BFont small(be_plain_font);
	small.SetSize(small.Size() * 0.9f);
	fStatusBar->SetFont(&small);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS, B_USE_HALF_ITEM_SPACING,
				B_USE_WINDOW_INSETS, B_USE_HALF_ITEM_SPACING)
			.Add(fMap, 0.72f)
			.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING, 0.28f)
				.Add(detailsBox)
				.AddGlue()
				.Add(fConnectButton)
			.End()
		.End()
		.AddGroup(B_HORIZONTAL, 0)
			.SetInsets(B_USE_WINDOW_INSETS, 0,
				B_USE_WINDOW_INSETS, B_USE_HALF_ITEM_SPACING)
			.Add(fStatusBar)
			.AddGlue()
		.End();
}


void
VPNMapWindow::_SeedTestPins()
{
	if (fMap == NULL)
		return;

	fMap->ClearServers();
	for (size_t i = 0; i < kTestServerCount; i++) {
		const TestServer& s = kTestServers[i];
		ServerPin pin;
		pin.host = s.host;
		pin.countryShort = s.countryShort;
		pin.countryLong = s.countryLong;
		pin.logPolicy = s.logPolicy;
		pin.latitude = s.latitude;
		pin.longitude = s.longitude;
		pin.score = s.score;
		pin.pingMs = s.pingMs;
		pin.speedMbps = s.speedMbps;
		pin.sessions = s.sessions;
		fMap->AddServer(pin);
	}
	fMap->ZoomToFit();
}


void
VPNMapWindow::_RefreshSidePanel()
{
	const ServerPin* picked = fMap != NULL ? fMap->SelectedServer() : NULL;

	if (picked == NULL) {
		const char* dash = "\xe2\x80\x94";
		if (fHostValue != NULL)		fHostValue->SetText(dash);
		if (fCountryValue != NULL)	fCountryValue->SetText(dash);
		if (fPingValue != NULL)		fPingValue->SetText(dash);
		if (fScoreValue != NULL)	fScoreValue->SetText(dash);
		if (fSessionsValue != NULL)	fSessionsValue->SetText(dash);
		if (fLogPolicyValue != NULL) fLogPolicyValue->SetText(dash);
		if (fConnectButton != NULL)	fConnectButton->SetEnabled(false);
		return;
	}

	if (fHostValue != NULL)
		fHostValue->SetText(picked->host.String());

	if (fCountryValue != NULL) {
		BString country(picked->countryLong);
		if (picked->countryShort.Length() > 0) {
			country << " (";
			country << picked->countryShort;
			country << ")";
		}
		fCountryValue->SetText(country.String());
	}

	char buf[64];
	if (fPingValue != NULL) {
		snprintf(buf, sizeof(buf), "%" B_PRId32 " ms", picked->pingMs);
		fPingValue->SetText(buf);
	}
	if (fScoreValue != NULL) {
		snprintf(buf, sizeof(buf), "%" B_PRId32, picked->score);
		fScoreValue->SetText(buf);
	}
	if (fSessionsValue != NULL) {
		snprintf(buf, sizeof(buf), "%" B_PRId32, picked->sessions);
		fSessionsValue->SetText(buf);
	}
	if (fLogPolicyValue != NULL) {
		fLogPolicyValue->SetText(picked->logPolicy.Length() > 0
			? picked->logPolicy.String() : "\xe2\x80\x94");
	}

	if (fConnectButton != NULL)
		fConnectButton->SetEnabled(true);
}


void
VPNMapWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgServerSelected:
			// MapView re-posts the click as kMsgServerSelected with the
			// host string; the side panel reads through
			// MapView::SelectedServer() which has been updated already.
			_RefreshSidePanel();
			break;

		case kMsgZoomIn:
		case kMsgZoomOut:
		case kMsgZoomFit:
		case kMsgToggleTiles:
			if (fMap != NULL)
				fMap->MessageReceived(message);
			break;

		case kMsgRefresh:
			// Until the daemon-side VPNGate fetcher lands (task #12), the
			// catalogue is the static test list; "Refresh" just re-seeds
			// it so the user sees the action took effect.
			_SeedTestPins();
			_RefreshSidePanel();
			break;

		case kMsgConnectPicked:
		{
			// Click-to-connect through the daemon is task #14. Until then
			// surface a helpful alert that shows what would happen rather
			// than failing silently.
			const ServerPin* picked = fMap != NULL
				? fMap->SelectedServer() : NULL;
			if (picked == NULL)
				break;
			BString body("Connect-to-pin is not wired up yet.\n\n");
			body << "Would request the .ovpn body for ";
			body << picked->host;
			body << " from the daemon, then run the existing OpenVPN flow.";
			BAlert* alert = new BAlert("notYetWired", body.String(), "OK");
			alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
			alert->Go(NULL);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
