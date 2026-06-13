APPINFO.OPX Version 1.20
Copyright 1998-1999 Twiddlebit Software
All rights reserved

CONTACT
-------

Note that this OPX is provided on an "as is" basis with no guarantee
of support.

For the latest version see the following www page:

	http://www.twiddlebit.com

INTRODUCTION
------------

AppInfo.OPX provides information about applications installed on an
EPOC32 machine. This information includes:

	- filenames for the .app file
	- caption names in the current language
	- application capabilities (hidden, embedability, support for new file)
	- icons

The OPX will scan all drives starting at y: down to a: and then z: for
apps. For performance reasons the data it gathers will be cached in memory.
There is a function which allows this cached data to be refreshed as needed.

EXTRACTING ICONS FROM AN AIF FILE
---------------------------------

Given an AIF file use the following function to extract an icon from
the file:

	AppExtractAifIcon:(aifFile$, iconSize&, iconType&, iconFile$)

	aifFile$  = full filename for aif file
	iconSize& = 16, 24, 32, 48,...
	iconType& = 0 for the normal icon, 1 for the mask icon
	iconFile$ = full filename for icon mbm file to be created

NOTE: see also the function AppExtractIcon:() which uses cached data.

SCANNING FOR APPS
-----------------

The first thing that you need to do is to tell the OPX to scan for all
the apps. This is done using:

	AppUpdate:

This only needs to be done once, the info will then be cached.
To rescan the disks for new/deleted apps use the AppUpdate: function again.
This will return 1 if any changes were detected.

To scan the list of cached apps use the following type of code:

	AppStartScan:
	WHILE AppNext:

		REM add your code here

	ENDWH

When using AppNext: the OPX maintains a pointer to the current app info.
Information about this app can be obtained using the following functions:

	AppFileName$:

		The full filename for the app

	AppName$:

		The name of the app file (without path or extension)

	AppUID&:

		The UID for the app

	AppCaption$:

		The name of the app in the current language

	AppCapability:(BYREF hidden&, BYREF newFile&, BYREF embed&)

		This sets the supplied parameters as follows:

		hidden&		= 1 if the app is marked as hidden
		newFile&	= 1 if the app supports the system->new file command
		embed&		= 0 if the app does not support embedding
					= 1 if the app supports embedding
					= 2 if the app supports embedding only

You can also obtain one of the 3 icons for the app using this
function:

	AppExtractIcon:(iconIndex&, iconType&, iconFile$)

		iconIndex$	= 0, 1 or 2 (0 for the smallest icon)
		iconType& = 0 for the normal icon, 1 for the mask icon
		iconFile$ = full filename for icon mbm file to be created

SIMULATING EVENTS
-----------------

AppInfo.OPX also allows the simulation of any type of event,
including keypress and pointer events using:

	AppSimulateEvent&:(type&,param1&,param2&)

Constants for the value of type& are defined in the appinfo.oxh file
as follows. The type governs the meaning of param1 and param2 as
follows:

TYPE				PARAM1		PARAM2
KAppEvNone&			-		-
KAppEvPointerSwitchOn&		-		-
KAppEvRedraw&			-		-
KAppEvSwitchOn&			-		-
KAppEvActive&			-		-
KAppEvInactive&			-		-
KAppEvUpdateModifiers&		-		-
KAppEvSwitchOff&		-		-

KAppEvKeyDown&			key scan code	-
KAppEvKeyUp&			key scan code	-

KAppEvPointerMove&		x coord		y coord
KAppEvButton1Down&		x coord		y coord
KAppEvButton1Up&		x coord		y coord
KAppEvButton2Down&		x coord		y coord
KAppEvButton2Up&		x coord		y coord
KAppEvButton3Down&		x coord		y coord
KAppEvButton4Up&		x coord		y coord

Note that the event will be sent to whatever app happens to be
in the foreground or has keyboard focus.

OPX FUNCTION SUMMARY
--------------------

	AppExtractAifIcon:(aifFile$, iconSize&, iconType&, iconFile$)

	AppUpdate:
	AppStartScan:
	AppNext:

	AppName$:
	AppUID&:
	AppFileName$:
	AppCaption$:
	AppCapability:(BYREF hidden&, BYREF newFile&, BYREF embed&)

	AppExtractIcon:(iconIndex&, iconType&, iconFile$)

	AppSimulateEvent&:(type&,param1&,param2&)
	
