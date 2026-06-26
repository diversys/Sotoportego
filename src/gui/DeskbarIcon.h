/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef DESKBAR_ICON_H
#define DESKBAR_ICON_H


#include <Messenger.h>
#include <String.h>
#include <View.h>

#include <vector>

#include "VPNProfile.h"
#include "VPNState.h"

class BBitmap;
class BPopUpMenu;


// The name DeskBar uses to refer to this replicant; also the BView name we
// register with. Must be stable across launches: DeskBar persists items by
// name, and changing it would orphan any already-installed replicant.
extern const char* const kDeskbarItemName;


// Sotoportego Deskbar replicant.
//
// Renders the "world" (mondo) brand glyph with a small open/closed padlock
// overlaid in the bottom-right to signal connection state. A left click
// pops a menu with the daemon's current profile list (selecting one fires
// kMsgConnect for that profile) plus Disconnect, "Open Sotoportego..." and
// "Remove from Deskbar" entries. A right click jumps straight to the menu
// (matching the convention of every other Haiku Deskbar item).
//
// The replicant is its own daemon client: on construction it launches the
// daemon if missing, subscribes for kMsgStatusUpdate and kMsgListProfiles,
// and redraws when the state or the profile set changes. The class is
// instantiated *inside the Deskbar process* after a reboot via the standard
// BArchivable::Instantiate hook, so the icon comes back automatically and
// re-subscribes to the daemon for live updates.
class DeskbarIcon : public BView {
public:
								DeskbarIcon(BRect frame);
								DeskbarIcon(BMessage* archive);
	virtual						~DeskbarIcon();

	// BArchivable hooks for replicant persistence. The visibility attribute
	// is what Deskbar's load_add_on() looks up after a reboot, so the
	// symbol must be exported from the binary even when it's also an app.
	__attribute__((used, visibility("default")))
	static	BArchivable*		Instantiate(BMessage* archive);
	virtual	status_t			Archive(BMessage* into,
									bool deep = true) const;

	virtual	void				AttachedToWindow();
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				MouseDown(BPoint where);
	virtual	void				Draw(BRect updateRect);

	// Add/remove the replicant from the user's Deskbar. The static helpers
	// are what the GUI's menu items wire into.
	static	status_t			AddToDeskbar();
	static	status_t			RemoveFromDeskbar();
	static	bool				IsInDeskbar();

private:
			void				_EnsureDaemon();
			void				_HandleStatusUpdate(BMessage* message);
			void				_HandleProfileList(BMessage* message);
			void				_ShowMenu(BPoint where);
			void				_OnMenuConnect(const BString& profileName);
			void				_OnMenuDisconnect();
			void				_OnMenuOpenGUI();
			void				_OnMenuRemove();

			BBitmap*			_RenderBitmap(const unsigned char* data,
									size_t size, float pixels);
			void				_ReleaseBitmaps();

			BMessenger			fServer;
			VPNState			fState;

	// Cached bitmaps, rebuilt on size change. The replicant's bounds are
	// effectively fixed by Deskbar (15px tall by default, can be set
	// taller), but they may change so we don't hardcode a size.
			BBitmap*			fBaseBitmap;
			BBitmap*			fLockOpenBitmap;
			BBitmap*			fLockClosedBitmap;
			float				fCachedSize;

	// Local copy of the daemon's profile list. Used to build the menu and
	// to drive Connect-by-name in the menu callback.
			std::vector<VPNProfile>	fProfiles;
};


#endif	// DESKBAR_ICON_H
