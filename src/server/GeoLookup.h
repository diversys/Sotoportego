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
// carrying `kFieldCountry` and `kFieldQueryIP` strings. On failure either
// (or both) field may be missing, so the caller can always rely on a final
// notification arriving.
namespace GeoLookup {

// BMessage fields in the result message.
extern const char* const kFieldCountry;
// The public IP ip-api saw the request from -- i.e. the egress IP of the
// VPN tunnel once routing is up.
extern const char* const kFieldQueryIP;
// Geo coordinates of that IP. Float fields; only present when the lookup
// succeeded AND ip-api had location data for the IP (some VPN exit ASNs
// come back without a precise lat/lon).
extern const char* const kFieldLatitude;
extern const char* const kFieldLongitude;

// Fire-and-forget. Returns immediately; the message arrives on the target's
// looper thread.
void	BackgroundLookup(const BMessenger& target, uint32 what);

}	// namespace GeoLookup


#endif	// GEO_LOOKUP_H
