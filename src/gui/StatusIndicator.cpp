/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "StatusIndicator.h"


static const float kIndicatorSize = 22.0f;


StatusIndicator::StatusIndicator(const char* name)
	:
	BView(name, B_WILL_DRAW | B_SUPPORTS_LAYOUT)
{
	fColor = (rgb_color){ 0x7f, 0x7f, 0x7f, 0xff };
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


void
StatusIndicator::SetColor(rgb_color color)
{
	fColor = color;
	Invalidate();
}


void
StatusIndicator::Draw(BRect /*updateRect*/)
{
	BRect bounds = Bounds();
	float diameter = bounds.Width() < bounds.Height()
		? bounds.Width() : bounds.Height();

	BRect circle(0, 0, diameter, diameter);
	circle.OffsetTo((bounds.Width() - diameter) / 2,
		(bounds.Height() - diameter) / 2);
	circle.InsetBy(3, 3);

	SetDrawingMode(B_OP_ALPHA);
	SetHighColor(fColor);
	FillEllipse(circle);

	// A faint darker rim gives the dot a crisp, deliberate edge.
	SetHighColor(tint_color(fColor, B_DARKEN_2_TINT));
	StrokeEllipse(circle);
}


BSize
StatusIndicator::MinSize()
{
	return BSize(kIndicatorSize, kIndicatorSize);
}


BSize
StatusIndicator::MaxSize()
{
	return BSize(kIndicatorSize, kIndicatorSize);
}


BSize
StatusIndicator::PreferredSize()
{
	return BSize(kIndicatorSize, kIndicatorSize);
}
