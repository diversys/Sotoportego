/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef GEO_LOOKUP_H
#define GEO_LOOKUP_H


#include <Messenger.h>
#include <SupportDefs.h>


// Best-effort "what country is my apparent IP in?" lookup.
//
// Spawns a short-lived worker thread that asks ip-api.com (HTTP, no TLS) for
// the country name of whoever's making the request. Run from the daemon
// AFTER the VPN tunnel is up: the request goes out through the tunnel, so
// the answer is the country we appear to come from now, not the one we'd
// come from on the underlying carrier.
//
// On completion the worker sends `target` a BMessage with what == `what`,
// carrying a single `kFieldCountry` string. On failure the same message is
// sent without that field, so the caller can always rely on a final
// notification.
namespace GeoLookup {

// BMessage field carrying the country name in the result message.
extern const char* const kFieldCountry;

// Fire-and-forget. Returns immediately; the message arrives on the target's
// looper thread.
void	BackgroundLookup(const BMessenger& target, uint32 what);

}	// namespace GeoLookup


#endif	// GEO_LOOKUP_H
