/*
 * Copyright 2026 atomozero. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "SotoportegoApp.h"

#include <Alert.h>

#include "MainWindow.h"
#include "VPNProtocol.h"


SotoportegoApp::SotoportegoApp()
	:
	BApplication(kGUISignature),
	fWindow(NULL)
{
}


void
SotoportegoApp::ReadyToRun()
{
	fWindow = new MainWindow();
	fWindow->Show();
}


void
SotoportegoApp::AboutRequested()
{
	BAlert* alert = new BAlert("About Sotoportego",
		"Sotoportego\n\n"
		"A native VPN client for Haiku.\n"
		"Milestone 1 skeleton \xe2\x80\x94 the connection is still a stub.",
		"Close");
	alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
	alert->Go();
}
