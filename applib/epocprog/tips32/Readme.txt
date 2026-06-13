Medialinx Tips32 Development Kit
Version 1.10 for EPOC/32 and OPL/32

 Revision: 12th April 1999
   Author: Graham Crichton
Publisher: Medialinx Internet Solutions
 Language: English


NEW in v1.10 - Window size remains the same even if the screen
               size changes.
             - Tips32 can now remain on top of all other windows
               if the developer wants it to.
             - Developers can protect their tips files from
               reverse engineering by now using Protect.opo.
             - The Psion SIS file now copies files to a Tips32
               folder instead of the Documents folder.


Overview
========

Tips32 is a piece of software aimed at OPL software developers to
add a Windows style tips interface to their programs. Tips32
can be customised in a number of ways.

* You can change the title of the window
* You can hide/show the "Credits" button
* You can hide/show/change the name of the author of the Tips file
* You can make the Tips32 window appear on top at all times

Tips32 is an OPM (OPL Module) and conforms to EPOC World OPM
Standards.


Installation
============

You can install Tips32 automatically by running Tips32.sis from
the Add/Remove control panel or EPOC Install.

You can install Tips32 manually by following these simple steps:

1. Create a directory (if it does not exist) \System\Opm
2. Copy Tips32.opm to C:\System\Opm\
3. Copy Tips32.mbm to C:\System\Opm\
4. Create a directory (if it does not exist) \Tips32
5. Copy Protect.opo to C:\Tips32\
6. Copy Template.tip to C:\Tips32\
7. Copy Example to C:\Tips32\
8. Copy Tips32opm.sis to C:\Tips32\
9. Copy Titanic.tip to C:\Tips32\ (Optional)

* Example is an OPL example using Tips32.
* Titanic.tip is an example of a Tips32 tips file.
* Template.tip is a template tips file that you shoud use when
  creating your own tips files.
* Protect.opo allows you to protect your Tips files from
  tampering. Protect makes a protected copy of your tips database
  which you can distribute and it leaves the original alone.
* Do NOT install Tips32opm.sis. This file is only provided for
  distribution purposes.


Adding Tips to your program
===========================

To add tips to your program you must first create a databse of
tips containing SIX Text fields (with a length of 40 characters
each). You can copy and edit the supplied template file
Template.tips to avoid creating a new one. Titanic.tip has also
been supplied with this package. It demonstrates a completed tips
file.

You must create a record for use as your index record. The first
record in the file will contain the name of the Tips Window,
the name of the author, whether the "Credits" button is to be
displayed or not and the window display status.

The format will look like this

Field  Content
=====  =======

Line1  Name of Window
Line2  Author
Line3  0 to show "Credits" button, 1 to hide
Line4  1 to keep window on top, 0 to hide

Record number 2 onwards will contain the tips. A tip card can
be made up of SIX lines of text. Tips32 records the number of
the last tip that the user read so that it can display the next
tip when the user runs the tips program again. This information
will be stored in a file called Tips.ini in the same directory
as the tips file.

To add Tips32 to your program you must add the following lines
of source code to your program:


REM -- Code Starts Here --

REM Example code to execute Tips32 with a tips file
REM Copyright © 1999 Medialinx Internet Solutions
REM Include this code in your source-code

REM ------------------------------------------------------------
REM Procedure loads the OPM, executes Tips32 and unloads the OPM
REM ------------------------------------------------------------
PROC SHOWTIPS:
	OPM_LoadModule:("Tips32")
	InitTips32:("C:\Tips32\Template.tip") REM Change the file to the location of your tips file
	OPM_UnLoadModule:("Tips32")
ENDP

REM -------------------------------------
REM Loads in a module held in /System/Opm
REM -------------------------------------
PROC OPM_LoadModule:(modulename$)
	LOCAL f$(255)
	f$=OPM_FileLoc$:("\System\Opm\"+modulename$+".opm")
	IF f$<>""
		LOADM f$ :REM Load the module
	ELSE
		ALERT("Cannot find the OPM:","\System\Opm\"+modulename$+".opm")
		STOP
	ENDIF
ENDP

REM --------------------------------
REM Unloads in a module from memory
REM --------------------------------
PROC OPM_UnloadModule:(modulename$)
	LOCAL f$(255)
	f$=OPM_FileLoc$:("\System\Opm\"+modulename$+".opm")
	ONERR notloaded::
	UNLOADM f$
notloaded::
	ONERR OFF
ENDP

REM --------------------------------------------------------
REM Returns the file position if the file exists on C: or D:
REM --------------------------------------------------------
PROC OPM_FileLoc$:(file$)
	IF EXIST ("C:"+file$)
		RETURN "C:"+file$
	ELSE
		IF EXIST ("D:"+file$)
			RETURN "D:"+file$
		ELSE
			RETURN ""	REM File does not exist
		ENDIF
	ENDIF
ENDP

REM -- Code Ends Here --


When distributing your program you must distribute your tips file,
Tips32.opm and Tips32.mbm only. You must also give users full
instructions on how to install these files.

You can distribute Tips32opm.sis with your program to
automatically install the Tips32 environment on to your clients
computer. This SIS file contains only the Tips32.opm and
Tips32.mbm files.


Staying on top of things!
=========================

You can make the Tips32 window stay on top of all other windows.
To do this set the value of Line4 in record 1 to 1. To return
the window state to normal change this value to 0.


Taking the Credit for it all!
=============================

You can hide the "Credits" button in the Tips32 program by setting
the value of Line3 in record 1 to 1. To show the button again
cnage this value to 0.


Protecting a Tips Database file
===============================

If you distribute your Tips32 Psion Database file, anyone can
make changes to it and even change the owner information and claim
it as their own work. The Tips32 Protection Engine decompiles
the Tips32 file and removes all Psion UID information from the
file, thus stopping people opening the file in Psion Data or the
equivalent.

To protect your file open Protect.opo and in the Input line
select the file that you want to protect.

In the Output line enter the name of the file you would like to
save. Press RETURN when finished. Protect.opo will display a
busy message at the bottom left hand side of the screen telling
you that it is busy. When your file is ready the program will
quit. This process may take a few minutes.

You can distribute the protected version with your program. You
do not have to protect your files, you can still distribute files
created in the Psion Data program.


Copyright
=========

Copyright © 1999 Medialinx Internet Solutions. All rights
reserved. Freeware. Use this software at your own risk.

Medialinx Internet Solutions
4 Hanwood Farm,
Dundonald,
Belfast. BT16 1YA.
Northern Ireland.

Email: support@medialinx.co.uk
Web: http://www.medialinx.co.uk

Please send bug reports to bugs@medialinx.co.uk

This product is unsupported
                ===========

You can distribute/use Tips32 in your program(s) freely in
shareware, commercial or freeware applications. If you do use
Tips32 please email me so I can keep track of who is using the
software.


Other Medialinx Titles
======================

Shortcut Wizard    5    Creates Windows style shortcuts
Startup Wizard     5    Launches shortcuts on power-up
SoundOn-SoundOff   5    Toggles system sound quickly
Tips32             5    Adds Windows style Tips to your programs
Recycle Bin        3a   Recoverable trashcan program
DriveMap5          5    Maps local and network drives
RemoteLink5        5    Disables the remote link on startup
LinkOn-LinkOff     5    Toggles the remote link quickly
Welcome5           5    Shows Tips32 tips on power-up

Under development: Gadabout5 - Coming Soon to EPOC/32!


Thank you for using this product,
Graham Crichton.