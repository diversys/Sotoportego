/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "VPNBackend.h"

#include <Message.h>

#include "VPNProtocol.h"


VPNBackend::VPNBackend(const char* name)
	:
	BHandler(name),
	fObserver()
{
}


VPNBackend::~VPNBackend()
{
}


void
VPNBackend::SetObserver(const BMessenger& observer)
{
	fObserver = observer;
}


void
VPNBackend::NotifyStateChanged(VPNState state, const char* detail)
{
	if (!fObserver.IsValid())
		return;

	BMessage message(kMsgStatusUpdate);
	message.AddInt32(kFieldState, (int32)state);
	message.AddString(kFieldBackend, BackendName());
	if (detail != NULL)
		message.AddString(kFieldDetail, detail);

	// Fold in current counters so a single update carries everything a client
	// needs to render the connection.
	Stats().Archive(&message);

	fObserver.SendMessage(&message);
}


void
VPNBackend::NotifyStats(const VPNStats& stats)
{
	if (!fObserver.IsValid())
		return;

	BMessage message(kMsgStatsUpdate);
	stats.Archive(&message);

	fObserver.SendMessage(&message);
}
