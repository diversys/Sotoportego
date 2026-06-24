/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SOTOPORTEGO_APP_H
#define SOTOPORTEGO_APP_H


#include <Application.h>

class MainWindow;


// The Sotoportego GUI client. It launches the daemon if needed and shows the
// main window; all VPN state lives in the daemon, reached over BMessage.
class SotoportegoApp : public BApplication {
public:
								SotoportegoApp();

	virtual	void				ReadyToRun();
	virtual	void				AboutRequested();

private:
			MainWindow*			fWindow;
};


#endif	// SOTOPORTEGO_APP_H
