/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H


#include <InterfaceDefs.h>
#include <View.h>


// A small, layout-friendly status dot: a single filled circle whose color is
// the connection-status accent (the only hand-picked color in the UI, per
// docs/GUI.md). Kept deliberately minimal so it reads as "corporate-calm".
class StatusIndicator : public BView {
public:
								StatusIndicator(const char* name);

			void				SetColor(rgb_color color);

	virtual	void				Draw(BRect updateRect);
	virtual	BSize				MinSize();
	virtual	BSize				MaxSize();
	virtual	BSize				PreferredSize();

private:
			rgb_color			fColor;
};


#endif	// STATUS_INDICATOR_H
