/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef VPN_GATE_FETCHER_H
#define VPN_GATE_FETCHER_H


#include <Messenger.h>
#include <SupportDefs.h>


// Fire-and-forget VPNGate public-server-list fetcher.
//
// VPNGate publishes its full catalogue (host, country, score, ping, sessions,
// log policy, and the literal .ovpn config body, all base64-encoded) at:
//
//   http://www.vpngate.net/api/iphone/
//
// The response is a single CSV blob (a few megabytes, ~100-300 servers
// depending on the day). One worker thread does the HTTP GET, parses the
// rows, geocodes each server by country code via CountryCentroids, and posts
// the result back to `target` as a BMessage:
//
//   * `what` = `whatOnReady`
//   * one nested BMessage per server under kFieldVPNGateServer, with the
//     fields from VPNProtocol.h (kFieldVPNGateHost, ...CountryShort,
//     ...Latitude, ...ConfigBase64 etc.)
//   * kFieldError (string) on failure; in that case no server entries are
//     attached.
//
// The caller always receives exactly one message, even on errors, so the
// UI never gets stuck waiting.
namespace VPNGateFetcher {

void	BackgroundFetch(const BMessenger& target, uint32 whatOnReady);

}	// namespace VPNGateFetcher


#endif	// VPN_GATE_FETCHER_H
