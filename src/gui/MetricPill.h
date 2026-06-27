/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef METRIC_PILL_H
#define METRIC_PILL_H


#include <String.h>
#include <View.h>


// Small rounded-rect badge that shows a metric value tinted by a quality
// tier (good/okay/bad), used in the map window's server side panel. Keeps
// the rest of the layout flush by sitting in the same cell a BStringView
// would have occupied; the parent BBox treats it as any other view.
class MetricPill : public BView {
public:
	enum Tier {
		kTierGood,		// pastel green
		kTierOkay,		// pastel amber
		kTierBad,		// pastel red
		kTierUnknown	// neutral grey (also used when the value is empty)
	};

								MetricPill(const char* name);

	// Set the text shown inside the pill and the tier that colors it.
	// Passing an empty string draws a single em-dash in the unknown tint
	// so the row keeps a stable footprint while the catalogue is loading.
			void				SetMetric(const char* text, Tier tier);

	virtual	void				Draw(BRect updateRect);
	virtual	BSize				MinSize();
	virtual	BSize				PreferredSize();

	// Tier from a ping (in milliseconds). 0 / negative -> unknown.
	static	Tier				TierForPing(int32 ms);
	// Tier from a vpngate score (higher == better, very wide range).
	static	Tier				TierForScore(int32 score);
	// Tier from current session count (more sessions == busier).
	static	Tier				TierForSessions(int32 sessions);

private:
			BString				fText;
			Tier				fTier;
};


#endif	// METRIC_PILL_H
