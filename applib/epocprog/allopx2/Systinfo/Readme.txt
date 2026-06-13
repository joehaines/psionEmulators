SYSTINFO.OPX
-----------
Copyright (c) Otfried Cheong and RMR Software 1998

If you going to distribute the OPX further then please read Part 5 first. This is VERY IMPORTANT.

README file
-----------
Contents:

1.	Introduction
2.	File Details
3.	Installation
4.	Using the OPX
5.	Distributing the OPX
6.	Registration
7.	Other Programs from RMR Software

INTRODUCTION
------------
This OPX gives you access to a number of functions in the SYSTINFO Suite, to enable you to access
them from within a OPL program.

FILE DETAILS
------------
The archive consists of the following files:

README.TXT		This file
SYSTINFO.SIS	This is the main OPX file in SIS format
SYSTINFO.OPX	This is the WINS version of the OPX
SYSTINFO.OXH	This is the header file
SYSTINFO.OPL	This is a demonstration program that shows you how the OPX can be used

INSTALLATION
------------
1.	Install SYSTINFO.SIS

2.	Copy SYSTINFO.OXH into the \System\Opl\ folder on either the C: or D: drive

3.	Copy SYSTINFO.OPL any where you like

USING THE OPX
-------------
1.	First compile and run the SYSTINFO.OPL file to make sure everything works and it communicates with
SYSTINFO Suite correctly.

2.	To use the OPX in your program add the following line to the top of the code, immediately after
the APP...ENDA and before the first procedure

	INCLUDE "SYSTINFO.OXH"

3.	You can now use the following additional procedures in your program.

SISystemVisible&: 
=================
To find out if the 'Show System Folder' preference is checked


SIHiddenVisible&: 
=================
To find out if the 'Show Hidden files' preference is checked


SICurrencyFormat$:(BYREF ndig&, BYREF neg&, BYREF space&, BYREF pos&, BYREF triad&)
===================================================================================
Determines the Currency format as set in Control Panel


SIDateFormat:(BYREF format&, BYREF sep0%, BYREF sep1%, BYREF sep2%, BYREF sep3%)
================================================================================
Determines the Date format as set in Control Panel


SITimeFormat:(BYREF format&, BYREF sep0%, BYREF sep1%, BYREF sep2%, BYREF sep3%, BYREF ampmSpace&, BYREF ampmPos&)
SIUTCOffset&:
=================================================================================================================
Determines the Time formats as set in Control Panel


SIWorkday%:(dayNo&)
===================
Returns =1 if the day is a workday, zero otherwise. Days are numbered 0 to 6, zero being Monday.


SIDaylightSaving%:(where&)
==========================
Returns =1 if daylight saving is in effect in a certain region, zero otherwise. The region is selected using
where&: 0 for the home city, 1 for Europe, 2 for Northern Hemisphere, 4 for Southern Hemisphere.


SIHomeCountry$:
===============
Returns the name of the home country.


SIUnits:(BYREF general&, BYREF shortDistances&, BYREF longDistances&)
=====================================================================
Determines the Units format as set in Control Panel


SIIsDirectory&:(filename$)
==========================
Shows if a name is a file or directory


SIVolumeName$:(driveNo&)
========================
Returns the volume name for a drive.


SIUniqueFilename$:(filename$)
=============================
Returns the filename unchanged, if it does not exist. If it exists, a uniquefied version is returned (for
instance "name.opl" may become "name(1).opl".


SIIsPathVisible&:(path$)
========================
Returns 1 if every directory on the path$ has neither the system nor hidden attribute set, and zero otherwise.
This is useful to work around a bug in dFILE, which doesn't work if the seed path contains hidden/system
directories.


SIBookmark$:
============
Returns the shell's bookmark.


SIStandardFolder$:
==================
Returns the system's standard folder.


SIDisplayContrast&:
===================
Returns the current display contrast.


SIOwner$:
=========
Returns the owner information from the Control Panel.


SIBatteryVolts:(BYREF mainVolt&, BYREF mainMax&, BYREF backVolt&, BYREF backMax&)
=================================================================================
Returns the battery voltage (in millivolts).


SIBatteryCurrent:(BYREF curCons&, BYREF totalCons&, BYREF inUseTime&, BYREF extPwrTime&, BYREF extPwr&, battInserted&)
======================================================================================================================
Returns the current battery current, the total current, the time of use of this battery, time of use of external
power, whether external power is present, and the time of insertion of the battery.


SIMemory:(BYREF ram&, BYREF rom&, BYREF maxFreeRam&, BYREF freeRam&, BYREF ramDisk&)
====================================================================================
Returns the memory status.


SIKeyClickEnabled%:
SIKeyClickLoud%:
SIKeyClickOverridden%:
SIPointerClickEnabled%:
SIPointerClickLoud%:
SIBeepEnabled%:
SIBeepLoud%:
SISoundDriverEnabled%:
SISoundDriverLoud%:
SISoundEnabled%:
================
Return the sound settings. 


SIAutoSwitchOffBehaviour&:
SIAutoSwitchOffTime&:
SIBacklightBehaviour&:
SIBacklightOnTime&:
===================
Return the switch off behaviour and the backlight behaviour.


SIKeyboardIndex&:
SILanguageIndex&:
SIXYInputPresent%:
SIKeyboardPresent%:
SIMaximumColors&:
SIProcessorClock&:
SISpeedFactor&:
===============
Return hardware characteristics.


SIMachine$:
===========
Returns a string consisting of 16 character chunks describing the different machine parts.


SIDisplaySize:(BYREF x&, BYREF y&, BYREF inX&, BYREF inY&, BYREF physX&, BYREF physY&)
======================================================================================
Returns the dimensions of the screen and the input device (Pen, Mouse).


SIRemoteLinkStatus&:
====================
Returns the remote link status: zero if it is disabled, one if disconnected, two if connected.


SIRemoteDisable:
================
Disables the remote link.


SIRemoteLinkEnable:
====================
Enables the remote link.


SIIsPathVisible&:(path$):
=========================
Checks to see if a file/folder is Hidden


SIPWIsEnabled%:
SIPWSetEnabled:(password$, enable%)
SIPWIsValid%:(password$)
SIPWSet:(OldPw$, NewPw$)
========================
Functions to check and change the machine password.


SILedSet:(on%)
==============
Switch the red external (recording indicator) LED on or off.


DISTRIBUTING THE OPX
--------------------
If you wish to distribute the OPX as part of your program, then you need to include the unchanged
SYSTINFO.SIS in the ZIP archive or SIS package for your program. Note that you may not, UNDER NO
CIRCUMSTANCES, distribute the unpacked SYSTINFO.OPX file. 

If you do not follow this rule, you disable the EPOC version control over the OPX, and your
application may break when the user installs a different version of the OPX. Worse, installing
your application may break other applications, and you can imagine the reactions this may cause you.
Don't say you haven't been warned!

You may not rename the OPX that you distribute, and you may not redistribute SYSTINFO.OXH or SYSTINFO.OPL,
they are simply for use on the developers machine. (The first rule makes sure that multiple copies of
OPX with the same UID cannot happen, the second rule is to avoid a proliferation of outdated copies of
our OPXs. Please refer interested parties to the RMR website.)

Shareware using this OPX must include this information in the "About" screen. (A line like "Contains
SYSTINFO.OPX © Otfried Cheong" or similar.)


REGISTRATION
------------
SYSTINFO.OPX is free for personal use and for use in Freeware programs. If you wish to distribute it in a
Shareware Package, then we ask that you register it by E-Mailing us at opx@rmrsoft.com. We are asking
for a nominal fee of twice the registration fee of your program. This also includes full backup support,
such as a WINS copy, e-mailing of enhancments and influence over future development of the OPX. Hope you
think this is acceptable, we are not trying to make money on this, just cover out costs.


OTHER PROGRAMS FROM RMR
-----------------------
If you like the look of this OPX, why not have a look at our programs.

A full list is as follows:

S5BANK			: A Personal Accounts Suite
RMRTASK			: An Extended Task (ToDo) Manager.
RMRNOTES		: A Note Taker/Jotter program
S5HOME			: A Home Inventory program.
S5FUEL			: A Fuel Consumption Monitor.
RMRUTILS		: A Utility/Conversion program
RMRZIP			: A Compression/Archive Utility
VACTRAC5		: A Holiday/Leave Tracking program
RMRSOL			: The classic "Solitaire" patience game
RMRFILE			: The premier file manager for EPOC machines
CONTACT			: The only Contact Manager available for EPOC machines

Some of these are also available in other languages, such as French, German, Spanish etc.. See the
Home Page http://www.rmrsoft.com/ for details.