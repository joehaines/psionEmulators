MUSIC.OPX
---------
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
This OPX gives you access to a number of functions in the MUSIC Suite, to enable you to access
them from within a OPL program.

FILE DETAILS
------------
The archive consists of the following files:

README.TXT	This file
MUSIC.SIS	This is the main OPX file in SIS format
MUSIC.OPX	This is the WINS version of the OPX
MUSIC.OXH	This is the header file
MUSIC.OPL	This is a demonstration program that shows you how the OPX can be used

INSTALLATION
------------
1.	Install MUSIC.SIS

2.	Copy MUSIC.OXH into the \System\Opl\ folder on either the C: or D: drive

3.	Copy MUSIC.OPL any where you like

USING THE OPX
-------------
1.	First compile and run the MUSIC.OPL file to make sure everything works and it communicates with
MUSIC Suite correctly.

2.	To use the OPX in your program add the following line to the top of the code, immediately after
the APP...ENDA and before the first procedure

	INCLUDE "MUSIC.OXH"

3.	You can now use the following additional procedures in your program.


MusicDTMF:(dtmf$)
=================
Dials a phone number in dtmf$. dtmf$ can contain *,# digits and uppercase letters.


MusicSetVolume:(vol%)
=====================
Sets the volume of the sound device used by all functions in the OPX. Same meaning
as the volume& argument to Playsound: in SYSTEM.OPX.


MusicNote:(freq&, amp&)
=======================
Starts playing a sine wave of frequency freq& and amplitude&. The freq& is as for
BEEP, the amplitude is a number between 0 and 100. The note lasts until MusicNote:
or MusicStop: is called again, or otherwise for at most 30 seconds (for battery
conservation).


MusicStop:
==========
Terminates the playing note. 


DISTRIBUTING THE OPX
--------------------
If you wish to distribute the OPX as part of your program, then you need to include the unchanged
MUSIC.SIS in the ZIP archive or SIS package for your program. Note that you may not, UNDER NO
CIRCUMSTANCES, distribute the unpacked MUSIC.OPX file. 

If you do not follow this rule, you disable the EPOC version control over the OPX, and your
application may break when the user installs a different version of the OPX. Worse, installing
your application may break other applications, and you can imagine the reactions this may cause you.
Don't say you haven't been warned!

You may not rename the OPX that you distribute, and you may not redistribute MUSIC.OXH or MUSIC.OPL,
they are simply for use on the developers machine. (The first rule makes sure that multiple copies of
OPX with the same UID cannot happen, the second rule is to avoid a proliferation of outdated copies of
our OPXs. Please refer interested parties to the RMR website.)

Shareware using this OPX must include this information in the "About" screen. (A line like "Contains
MUSIC.OPX © Otfried Cheong" or similar.)


REGISTRATION
------------
MUSIC.OPX is free for personal use and for use in Freeware programs. If you wish to distribute it in a
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
