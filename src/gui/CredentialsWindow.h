/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef CREDENTIALS_WINDOW_H
#define CREDENTIALS_WINDOW_H


#include <Messenger.h>
#include <String.h>
#include <Window.h>

class BButton;
class BCheckBox;
class BTextControl;


// BMessage field name on the OK reply: bool true when the user ticked the
// "Remember password" checkbox.
extern const char* const kFieldRemember;


// Modal credentials prompt shown before a Connect when the active profile
// might need username/password authentication. On OK, posts a configurable
// message back to the parent containing two strings and the remember flag;
// the parent builds the Connect request from them and (if asked) stores
// the credentials in BKeyStore. On Cancel, posts the cancel message
// (with no fields) so the parent knows to drop the pending Connect.
class CredentialsWindow : public BWindow {
public:
								CredentialsWindow(BWindow* parent,
									const BMessenger& target,
									uint32 onOK, uint32 onCancel,
									const char* profileName,
									const BString& prefilledUser);

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			BMessenger			fTarget;
			uint32				fOnOK;
			uint32				fOnCancel;
			BTextControl*		fUserField;
			BTextControl*		fPasswordField;
			BCheckBox*			fRememberBox;
			BButton*			fOKButton;
			BButton*			fCancelButton;
			bool				fSent;
};


#endif	// CREDENTIALS_WINDOW_H
