/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "DeskbarIcon.h"

#include <stdio.h>
#include <string.h>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Deskbar.h>
#include <Entry.h>
#include <IconUtils.h>
#include <Looper.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Message.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>

#include "VPNProtocol.h"

#include "mondo_icon_data.h"
#include "lock_open_icon_data.h"
#include "lock_closed_icon_data.h"


const char* const kDeskbarItemName = "Sotoportego";


// Private message codes routed back to the replicant from the popup menu.
static const uint32 kMsgMenuConnect		= 'dmCo';
static const uint32 kMsgMenuDisconnect	= 'dmDi';
static const uint32 kMsgMenuOpenGUI		= 'dmGu';
static const uint32 kMsgMenuRemove		= 'dmRm';

// BMessage field name used by the per-profile menu items to carry the
// profile they refer to.
static const char* const kFieldMenuProfile	= "soto:menu:profile";


// --- construction / archiving ---------------------------------------------

DeskbarIcon::DeskbarIcon(BRect frame)
	:
	BView(frame, kDeskbarItemName,
		B_FOLLOW_LEFT | B_FOLLOW_TOP,
		B_WILL_DRAW | B_TRANSPARENT_BACKGROUND),
	fServer(),
	fState(VPN_STATE_DISCONNECTED),
	fBaseBitmap(NULL),
	fLockOpenBitmap(NULL),
	fLockClosedBitmap(NULL),
	fCachedSize(0),
	fProfiles()
{
}


DeskbarIcon::DeskbarIcon(BMessage* archive)
	:
	BView(archive),
	fServer(),
	fState(VPN_STATE_DISCONNECTED),
	fBaseBitmap(NULL),
	fLockOpenBitmap(NULL),
	fLockClosedBitmap(NULL),
	fCachedSize(0),
	fProfiles()
{
}


DeskbarIcon::~DeskbarIcon()
{
	_ReleaseBitmaps();
}


BArchivable*
DeskbarIcon::Instantiate(BMessage* archive)
{
	// Deskbar passes any BMessage that claims to be ours; double-check the
	// class name so we don't accidentally instantiate from someone else's
	// archive.
	if (!validate_instantiation(archive, "DeskbarIcon"))
		return NULL;
	return new DeskbarIcon(archive);
}


status_t
DeskbarIcon::Archive(BMessage* into, bool deep) const
{
	status_t result = BView::Archive(into, deep);
	if (result != B_OK)
		return result;

	// Deskbar uses these to find the binary on disk and re-instantiate the
	// view after a reboot. The class is in libbe-style C++; "add_on" points
	// at the binary that owns it.
	into->AddString("class", "DeskbarIcon");
	into->AddString("add_on", kGUISignature);
	return B_OK;
}


// --- BView hooks ----------------------------------------------------------

void
DeskbarIcon::AttachedToWindow()
{
	BView::AttachedToWindow();

	// The replicant runs inside Deskbar's window. Inherit its panel colour
	// so the transparent bits around our bitmap blend in instead of staring
	// out white.
	SetViewColor(B_TRANSPARENT_COLOR);
	SetLowColor(Parent() != NULL
		? Parent()->ViewColor()
		: ui_color(B_PANEL_BACKGROUND_COLOR));

	_EnsureDaemon();
}


void
DeskbarIcon::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgStatusUpdate:
			_HandleStatusUpdate(message);
			break;

		case kMsgStatsUpdate:
			// We don't render throughput; ignore.
			break;

		case kMsgListProfiles:
			_HandleProfileList(message);
			break;

		case kMsgMenuConnect:
		{
			const char* name = NULL;
			if (message->FindString(kFieldMenuProfile, &name) == B_OK
					&& name != NULL) {
				_OnMenuConnect(BString(name));
			}
			break;
		}
		case kMsgMenuDisconnect:
			_OnMenuDisconnect();
			break;
		case kMsgMenuOpenGUI:
			_OnMenuOpenGUI();
			break;
		case kMsgMenuRemove:
			_OnMenuRemove();
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
DeskbarIcon::MouseDown(BPoint where)
{
	// Any click goes to the popup menu. Right-click would normally jump to
	// the Deskbar's own context menu; we override both because the user
	// almost always wants the profile list, not Deskbar housekeeping.
	_ShowMenu(where);
}


void
DeskbarIcon::Draw(BRect /*updateRect*/)
{
	BRect bounds = Bounds();
	float size = bounds.Width() < bounds.Height()
		? bounds.Width() + 1 : bounds.Height() + 1;

	// Re-render the bitmaps if the slot grew/shrank since last paint.
	if (fBaseBitmap == NULL || size != fCachedSize) {
		_ReleaseBitmaps();
		fCachedSize = size;
		fBaseBitmap = _RenderBitmap(kMondoHvif, kMondoHvifSize, size);
		// The lock overlay sits at ~75% of the base, anchored bottom-right.
		// At Deskbar's default 16px slot that's 12px, which is the smallest
		// size at which the open/closed shackle is still legible.
		float overlaySize = (size * 0.75f);
		if (overlaySize < 10.0f)
			overlaySize = 10.0f;
		fLockOpenBitmap = _RenderBitmap(kLockOpenHvif, kLockOpenHvifSize,
			overlaySize);
		fLockClosedBitmap = _RenderBitmap(kLockClosedHvif, kLockClosedHvifSize,
			overlaySize);
	}

	if (fBaseBitmap == NULL)
		return;

	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	DrawBitmap(fBaseBitmap, bounds.LeftTop());

	BBitmap* overlay = (fState == VPN_STATE_CONNECTED)
		? fLockClosedBitmap : fLockOpenBitmap;
	if (overlay != NULL) {
		BRect ob = overlay->Bounds();
		float ox = bounds.right - ob.Width();
		float oy = bounds.bottom - ob.Height();
		DrawBitmap(overlay, BPoint(ox, oy));
	}
	SetDrawingMode(B_OP_COPY);
}


// --- daemon glue ----------------------------------------------------------

void
DeskbarIcon::_EnsureDaemon()
{
	// Best-effort launch + subscribe. We don't surface failures: the
	// Deskbar isn't a place for modal alerts, and the icon will simply
	// stay in the "Disconnected" look until the daemon shows up.
	be_roster->Launch(kServerSignature);

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
DeskbarIcon::_HandleStatusUpdate(BMessage* message)
{
	int32 state = VPN_STATE_DISCONNECTED;
	message->FindInt32(kFieldState, &state);
	if ((VPNState)state == fState)
		return;
	fState = (VPNState)state;
	Invalidate();
}


void
DeskbarIcon::_HandleProfileList(BMessage* message)
{
	fProfiles.clear();
	BMessage archive;
	for (int32 i = 0; message->FindMessage(kFieldProfile, i, &archive) == B_OK;
			i++) {
		VPNProfile profile;
		profile.Unarchive(archive);
		fProfiles.push_back(profile);
	}
}


// --- menu -----------------------------------------------------------------

void
DeskbarIcon::_ShowMenu(BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("DeskbarMenu", false, false);

	// Profile list (or a disabled placeholder when nothing's imported).
	if (fProfiles.empty()) {
		BMenuItem* empty = new BMenuItem("(no profiles imported)", NULL);
		empty->SetEnabled(false);
		menu->AddItem(empty);
	} else {
		bool busy = fState != VPN_STATE_DISCONNECTED
			&& fState != VPN_STATE_ERROR;
		for (size_t i = 0; i < fProfiles.size(); i++) {
			BMessage* msg = new BMessage(kMsgMenuConnect);
			msg->AddString(kFieldMenuProfile, fProfiles[i].fName);
			BMenuItem* item = new BMenuItem(fProfiles[i].fName.String(), msg);
			// Disable Connect entries while a session is up: the daemon
			// would reject the request anyway, and a greyed-out item is
			// clearer than a silent no-op.
			item->SetEnabled(!busy);
			menu->AddItem(item);
		}
	}

	menu->AddSeparatorItem();

	BMenuItem* disconnect = new BMenuItem("Disconnect",
		new BMessage(kMsgMenuDisconnect));
	disconnect->SetEnabled(fState == VPN_STATE_CONNECTING
		|| fState == VPN_STATE_AUTHENTICATING
		|| fState == VPN_STATE_CONNECTED
		|| fState == VPN_STATE_RECONNECTING);
	menu->AddItem(disconnect);

	menu->AddItem(new BMenuItem("Open Sotoportego" B_UTF8_ELLIPSIS,
		new BMessage(kMsgMenuOpenGUI)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Remove from Deskbar",
		new BMessage(kMsgMenuRemove)));

	// All items send their messages back to us; the menu is short-lived.
	menu->SetTargetForItems(BMessenger(this));
	menu->SetAsyncAutoDestruct(true);

	BPoint screenPoint = ConvertToScreen(where);
	menu->Go(screenPoint, true, true, true);
}


void
DeskbarIcon::_OnMenuConnect(const BString& profileName)
{
	if (!fServer.IsValid() || profileName.Length() == 0)
		return;

	// Look the profile up by name in our cached list -- we send the full
	// archive in the Connect message, just like the GUI does.
	const VPNProfile* profile = NULL;
	for (size_t i = 0; i < fProfiles.size(); i++) {
		if (fProfiles[i].fName == profileName) {
			profile = &fProfiles[i];
			break;
		}
	}
	if (profile == NULL)
		return;

	BMessage archive;
	profile->Archive(&archive);

	BMessage connect(kMsgConnect);
	connect.AddMessenger(kFieldClient, BMessenger(this));
	connect.AddMessage(kFieldProfile, &archive);
	// We don't have a credentials prompt here: hand the daemon whatever
	// the profile already carries. If the .ovpn needs interactive auth
	// the user will see Error after a few seconds and they can fall back
	// to the GUI for the password dialog.
	fServer.SendMessage(&connect);
}


void
DeskbarIcon::_OnMenuDisconnect()
{
	if (!fServer.IsValid())
		return;
	BMessage disconnect(kMsgDisconnect);
	disconnect.AddMessenger(kFieldClient, BMessenger(this));
	fServer.SendMessage(&disconnect);
}


void
DeskbarIcon::_OnMenuOpenGUI()
{
	// Launch (or focus) the GUI binary by its MIME signature. be_roster
	// handles the "already running" case for us.
	be_roster->Launch(kGUISignature);
}


void
DeskbarIcon::_OnMenuRemove()
{
	RemoveFromDeskbar();
}


// --- bitmap helpers -------------------------------------------------------

BBitmap*
DeskbarIcon::_RenderBitmap(const unsigned char* data, size_t size, float px)
{
	if (px < 1.0f)
		return NULL;
	BBitmap* bitmap = new BBitmap(BRect(0, 0, px - 1, px - 1), 0, B_RGBA32);
	if (bitmap == NULL || bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	if (BIconUtils::GetVectorIcon(data, size, bitmap) != B_OK) {
		delete bitmap;
		return NULL;
	}
	return bitmap;
}


void
DeskbarIcon::_ReleaseBitmaps()
{
	delete fBaseBitmap;
	delete fLockOpenBitmap;
	delete fLockClosedBitmap;
	fBaseBitmap = NULL;
	fLockOpenBitmap = NULL;
	fLockClosedBitmap = NULL;
	fCachedSize = 0;
}


// --- install / remove -----------------------------------------------------

status_t
DeskbarIcon::AddToDeskbar()
{
	BDeskbar deskbar;
	if (!deskbar.IsRunning())
		return B_ERROR;

	// If we're already in, leave it -- replicants don't tolerate duplicates
	// well and the user probably hit Install twice by mistake.
	if (deskbar.HasItem(kDeskbarItemName))
		return B_OK;

	// Mose-style: hand Deskbar a freshly constructed BView and let it take
	// over. Archive() encodes our binary's MIME signature so Deskbar can
	// reload the class after a reboot via load_add_on()/Instantiate().
	float side = deskbar.MaxItemHeight();
	if (side < 16)
		side = 16;

	DeskbarIcon* view = new DeskbarIcon(BRect(0, 0, side, side));
	int32 id = -1;
	status_t result = deskbar.AddItem(view, &id);
	// Deskbar keeps its own copy of the archived view; delete the local
	// instance whether or not the add succeeded.
	delete view;
	return result;
}


status_t
DeskbarIcon::RemoveFromDeskbar()
{
	BDeskbar deskbar;
	if (!deskbar.IsRunning())
		return B_ERROR;

	int32 removed = 0;
	while (deskbar.HasItem(kDeskbarItemName)) {
		status_t result = deskbar.RemoveItem(kDeskbarItemName);
		if (result != B_OK)
			return removed > 0 ? B_OK : result;
		removed++;
	}
	return B_OK;
}


bool
DeskbarIcon::IsInDeskbar()
{
	BDeskbar deskbar;
	return deskbar.IsRunning() && deskbar.HasItem(kDeskbarItemName);
}
