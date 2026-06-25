/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SOTOPORTEGO_SERVER_H
#define SOTOPORTEGO_SERVER_H


#include <vector>

#include <Application.h>
#include <Messenger.h>

#include "ProfileStore.h"
#include "VPNState.h"

class VPNBackend;


// The Sotoportego daemon.
//
// This process owns the VPN lifecycle. Front-ends (GUI, Deskbar replicant,
// CLI) are separate processes that drive it over BMessage; the daemon never
// trusts a client with the connection state directly. For Milestone 1 it runs
// as the normal user and holds a single backend instance, but the
// process boundary is real so privilege separation can be added later without
// reshaping the architecture.
//
// As a BApplication it is also a BLooper, which hosts the backend handler and
// dispatches both client requests and backend events on one thread.
class SotoportegoServer : public BApplication {
public:
								SotoportegoServer();
	virtual						~SotoportegoServer();

	virtual	void				ReadyToRun();
	virtual	void				MessageReceived(BMessage* message);

private:
			void				_HandleConnect(BMessage* message);
			void				_HandleDisconnect(BMessage* message);
			void				_HandleSubscribe(BMessage* message);
			void				_HandleUnsubscribe(BMessage* message);
			void				_HandleGetStatus(BMessage* message);
			void				_HandleSaveProfile(BMessage* message);
			void				_HandleDeleteProfile(BMessage* message);

	// Watch incoming status updates for the transitions that warrant a
	// desktop notification (Connected, Disconnected, Error) and fan them
	// out via BNotification. The Connected notification kicks off a
	// background geo-lookup that updates it with the apparent country
	// once the answer comes back.
			void				_HandleStatusForNotification(
									BMessage* message);
			void				_HandleCountryResult(BMessage* message);
			void				_PostNotification(const char* title,
									const char* content, int32 type = 0);

	// Re-broadcast a backend event (state or stats) to every subscribed
	// client, pruning any that have gone away.
			void				_Broadcast(BMessage* message);

	// Build a fresh status snapshot from the backend.
			void				_FillStatus(BMessage* message);

	// Build and broadcast a kMsgListProfiles snapshot. Sent to a specific
	// client on subscribe and to everyone after a profile-store mutation.
			void				_SendProfileList(BMessenger to) const;
			void				_BroadcastProfileList();

			BMessenger			_ClientFrom(BMessage* message) const;

			VPNBackend*				fBackend;
			ProfileStore			fProfiles;
			std::vector<BMessenger>	fClients;

	// Last broadcast state we observed, so we know which transitions just
	// happened and only fire notifications once per state change.
			VPNState				fLastState;
	// Last successful connection summary, kept so the
	// geo-lookup result can rebuild the same notification text plus the
	// country tag.
			BString					fLastServerSummary;
};


#endif	// SOTOPORTEGO_SERVER_H
