/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "MetricPill.h"

#include <Font.h>


// Pastel tints chosen to match the row-background palette used in Mose, so
// the map window's metric pills feel like part of the same visual family
// (cf. HeaderView::kAccent* and the row backgrounds in MoseFirewall).
static const rgb_color kGoodFill	= { 220, 245, 225, 255 };
static const rgb_color kOkayFill	= { 255, 240, 200, 255 };
static const rgb_color kBadFill		= { 250, 220, 220, 255 };
static const rgb_color kUnknownFill	= { 230, 230, 230, 255 };

// Slightly-darker text on each tint, so the pill stays readable on light
// panel backgrounds without shouting.
static const rgb_color kGoodText	= { 25, 110, 60, 255 };
static const rgb_color kOkayText	= { 130, 90, 10, 255 };
static const rgb_color kBadText		= { 150, 35, 35, 255 };
static const rgb_color kUnknownText	= { 110, 110, 110, 255 };

static const float kHPad			= 8.0f;
static const float kVPad			= 2.0f;


MetricPill::MetricPill(const char* name)
	:
	BView(name, B_WILL_DRAW | B_SUPPORTS_LAYOUT | B_FULL_UPDATE_ON_RESIZE),
	fText("\xe2\x80\x94"),
	fTier(kTierUnknown)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


void
MetricPill::SetMetric(const char* text, Tier tier)
{
	if (text == NULL || text[0] == '\0') {
		fText = "\xe2\x80\x94";
		fTier = kTierUnknown;
	} else {
		fText = text;
		fTier = tier;
	}
	InvalidateLayout();
	Invalidate();
}


void
MetricPill::Draw(BRect /*updateRect*/)
{
	rgb_color fill;
	rgb_color textColor;
	switch (fTier) {
		case kTierGood:	fill = kGoodFill; textColor = kGoodText; break;
		case kTierOkay:	fill = kOkayFill; textColor = kOkayText; break;
		case kTierBad:	fill = kBadFill;  textColor = kBadText;  break;
		case kTierUnknown:
		default:		fill = kUnknownFill; textColor = kUnknownText; break;
	}

	BFont font(be_bold_font);
	font_height fh;
	font.GetHeight(&fh);
	float textWidth = font.StringWidth(fText.String());

	// Center the pill within our own bounds; the layout system gives us
	// our row height, the pill itself only takes the space its text needs.
	BRect b = Bounds();
	float pillHeight = fh.ascent + fh.descent + 2 * kVPad;
	float pillWidth = textWidth + 2 * kHPad;
	float pillX = b.left;
	float pillY = b.top + (b.Height() - pillHeight) / 2.0f;
	BRect pill(pillX, pillY, pillX + pillWidth, pillY + pillHeight);

	float radius = pillHeight / 2.0f;
	SetHighColor(fill);
	FillRoundRect(pill, radius, radius);

	SetFont(&font);
	SetDrawingMode(B_OP_OVER);
	SetLowColor(fill);
	SetHighColor(textColor);
	DrawString(fText.String(),
		BPoint(pill.left + kHPad,
			pill.top + kVPad + fh.ascent));
}


BSize
MetricPill::MinSize()
{
	BFont font(be_bold_font);
	font_height fh;
	font.GetHeight(&fh);
	float w = font.StringWidth(fText.String()) + 2 * kHPad;
	float h = fh.ascent + fh.descent + 2 * kVPad;
	return BSize(w, h);
}


BSize
MetricPill::PreferredSize()
{
	return MinSize();
}


MetricPill::Tier
MetricPill::TierForPing(int32 ms)
{
	if (ms <= 0)
		return kTierUnknown;
	if (ms < 50)
		return kTierGood;
	if (ms < 150)
		return kTierOkay;
	return kTierBad;
}


MetricPill::Tier
MetricPill::TierForScore(int32 score)
{
	// vpngate scores are a Mb-of-traffic-ish metric: typical "good" servers
	// sit above the 100k mark, the long tail below 30k is barely usable.
	if (score <= 0)
		return kTierUnknown;
	if (score >= 100000)
		return kTierGood;
	if (score >= 30000)
		return kTierOkay;
	return kTierBad;
}


MetricPill::Tier
MetricPill::TierForSessions(int32 sessions)
{
	// More users sharing the link == worse for us; map count to load tier.
	if (sessions < 0)
		return kTierUnknown;
	if (sessions <= 30)
		return kTierGood;
	if (sessions <= 100)
		return kTierOkay;
	return kTierBad;
}
