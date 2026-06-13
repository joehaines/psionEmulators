RMRALARM.OPX
------------
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
This OPX gives you access to a number of functions in the RMRALARM Suite, to enable you to access
them from within a OPL program.

FILE DETAILS
------------
The archive consists of the following files:

README.TXT		This file
RMRALARM.SIS	This is the main OPX file in SIS format
RMRALARM.OPX	This is the WINS version of the OPX
RMRALARM.OXH	This is the header file
RMRALARM.OPL	This is a demonstration program that shows you how the OPX can be used

INSTALLATION
------------
1.	Install RMRALARM.SIS

2.	Copy RMRALARM.OXH into the \System\Opl\ folder on either the C: or D: drive

3.	Copy RMRALARM.OPL any where you like

USING THE OPX
-------------
1.	First compile and run the RMRALARM.OPL file to make sure everything works and it communicates with
RMRALARM Suite correctly.

2.	To use the OPX in your program add the following line to the top of the code, immediately after
the APP...ENDA and before the first procedure

	INCLUDE "RMRALARM.OXH"

3.	You can now use the following additional procedures in your program.

AlmSetClockAlarm:(alarmNo&, dtime&, msg$, sound$, repeat&)
==========================================================
Where alarmno& is 0=7, dtime& is the date and time, msg$ is the RMRALARM to appear on the screen,
sound$ is which sound (built=in or name of an alarm file in \system\alarms) to use and repeat&
is the repeat mode (once, next 24 hours, daily, weekly, etc===suitable constants are defined in
RMRALARM.OXH)


AlmAlarmState&:(alarmNo&)
=========================
Returns whether the alarm is off, on, or on, but disabled. Constants for the three modes are
defined in RMRALARM.OXH


AlmAlarmEnable:(onoff&, alarmNumber&) 
=====================================
Enables or Disables the Alarm, with alarmNumber& set to 0=7 


AlmAlarmDelete:(alarmNumber&) 
=============================
Deletes Alarm number 0=7 


AlmQuietPeriodSet:(minutes&) 
============================
Sets the alarm to go quiet for a specified number of minutes


AlmQuietPeriodCancel: 
=====================
Cancels the previously set quiet period 


AlmQuietPeriodUntil:(dtime&) 
============================
This returns the time until when the alarm is quietened in dtime&. dtime& has to be created before
calling this function, otherwise all hell will break lose.


AlmSetAlarmSound:(onoff&) 
=========================
Turns the Sound setting for the alarms on/off (use one or zero for argument)


AlmAlarmSoundState&: 
====================
This returns a value depending on the sound setting. Constants are defined in RMRALARM.OXH


WldFindCity:(city$, "CityCallback") 
===================================
Accesses the database to find a city, and allows access to all the details (Lat/Long. country, time
difference, dialling code....)  This is a bit tricky: You have to provide an OPL function CityCallback&
that is called by the OPX with the information as arguments. See the example program for details.


WldSunlight:(city$, sunrise&, sunset&, dtime& )
===============================================
Gets the sunrise/sunset times for a city at a given day (passed as dtime&).  You can use &0 instead of
dtime& to get the times for today.


WldNextCity$:(city$)
WldPreviousCity$:(city$)
WldNextCountry$:(country$)
WldPreviousCountry$:(country$)
=============================
Allow you to navigate the world database as in the Time application.


DISTRIBUTING THE OPX
--------------------
If you wish to distribute the OPX as part of your program, then you need to include the unchanged
RMRALARM.SIS in the ZIP archive or SIS package for your program. Note that you may not, UNDER NO
CIRCUMSTANCES, distribute the unpacked RMRALARM.OPX file. 

If you do not follow this rule, you disable the EPOC version control over the OPX, and your
application may break when the user installs a different version of the OPX. Worse, installing
your application may break other applications, and you can imagine the reactions this may cause you.
Don't say you haven't been warned!

You may not rename the OPX that you distribute, and you may not redistribute RMRALARM.OXH or RMRALARM.OPL,
they are simply for use on the developers machine. (The first rule makes sure that multiple copies of
OPX with the same UID cannot happen, the second rule is to avoid a proliferation of outdated copies of
our OPXs. Please refer interested parties to the RMR website.)

Shareware using this OPX must include this information in the "About" screen. (A line like "Contains
RMRALARM.OPX © Otfried Cheong" or similar.)


REGISTRATION
------------
RMRALARM.OPX is free for personal use and for use in Freeware programs. If you wish to distribute it in a
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
