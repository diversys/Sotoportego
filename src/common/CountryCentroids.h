/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef COUNTRY_CENTROIDS_H
#define COUNTRY_CENTROIDS_H


#include <String.h>


// Static ISO-3166-1 alpha-2 → (latitude, longitude) lookup, used to place
// VPNGate servers on the world map when their catalogue entry only carries a
// country code. The centroid is the population-weighted approximate centre of
// each country, picked so the resulting pin lands on something recognisable
// (Tokyo for JP, not the middle of the Sea of Japan).
//
// Returns true on a known code; lat/lon left untouched on miss. Case
// insensitive; whitespace is trimmed.
namespace CountryCentroids {

bool	Lookup(const BString& iso2, float& latOut, float& lonOut);

}	// namespace CountryCentroids


#endif	// COUNTRY_CENTROIDS_H
