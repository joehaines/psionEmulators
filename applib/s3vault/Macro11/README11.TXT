Series3a Macros 1.1 (c) Psion GmbH 1994
---------------------------------------

LICENSING CONDITIONS
All rights reserved.  Psion accepts no responsibility for any loss or damage
caused by the direct or indirect use of this program.  The software remains
the property of Psion GmbH.

No fee may be charged for this product, without the express permission of
Psion GmbH.  Also, this product may not be used to distribute any product
for which a fee is charged without the express permission of Psion GmbH.

Psion GmbH
Saalburgstraáe 157
D-61350 Bad Homburg

    Tel: +49 6172 9654-0
Hotline: +49 6172 9654-28
    Fax: +49 6172 39192
Mailbox: +49 6172 37090

       Cix: mhoelscher
  Internet: mhoelscher@uk.co.compulink.cix
Compuserve: 71333,2070

This program was written by Graham Tappenden, January-March 1994

BACKGROUND

This program is an extension of the Install program.  Install (from the same
Author) allows users to re-build their System Screen from a script file after
a reset.  The program expanded to allow futher options, so that applications
could be installed from disk, as specified by their authors, without the
user needing extensive knowledge of the Series3a file system.

The whole system works on a process of Simulated Key Presses, where one
process effectively inserts keys into the keyboard buffer of another process.

The obvious question was, if we can do this with System, why not with other
applications.  How about being able to create macros for Word and
Spreadsheet, for example.

This program does just that.  The following list gives the available
commands, along with their syntax, and how to use them.

FIXES FOR VERSION 1.1

The WAIT function now operates correctly, it it wait until the correct key
is pressed.
CTRLMENU and SHIFTMENU are now supported.
Indenting is now allowed.

INSTALLATION

Installation requires the copying of two files: MACRO.APP and MACEDIT.ALS,
both into the \APP\ directory of the internal disk, or onto an SSD.

MACRO.APP is the program to call to run a macro.  MACEDIT.ALS allows you to
edit the macro files in plain-text format.

NOTE: Script file from Install will NOT work with the macro program.

COMMANDS: SELECTING YOUR APPLICATION

APP appname
This selects the application with this processname (without the extension).
The first process found will be used, so if you have two copies of WORD
open, then only one will receive the macro commands.

For those who don't have a knowledge of the Series3a operating system, the
following keywords apply:

Application                     Keyword for Process Name
-----------                     ------------------------
Database                        DATA

Word Processor                  WORD
Agenda                          AGENDA
Time                            TIME
World                           WORLD
Calculator                      CALC
Spreadsheet                     SH3
Program Editor                  WORD

NOTE: Due to the nature of the Series3a, you CANNOT use macros to control an
OPL program.

GOAPP
If you wish to move to the current application, then use this command.
Otherwise, you will see the Macro screen, and the commands will be sent to
the application without you seeing the results.


COMMANDS: CURSOR MOVEMENT

HOME    
END
LEFT    
RIGHT
UP
DOWN
PAGEUP
PAGEDOWN

Each of these commands does just what it says.  'Left', for example,
simulates pressing the Left Cursor Key.

MARKUP
MARKDOWN
MARKLEFT
MARKRIGHT

These four commands act as if shift was held at the same time as pressing
the respective cursor key.

COMMANDS: ENTERING TEXT

ENTER
SHIFTENTER
CTRLENTER
Sends the ENTER, SHIFT+ENTER, and CONTROL+ENTER combinations.

TAB
SHIFTTAB
CTRLTAB
Sends the TAB, SHIFT+TAB, and CONTROL+TAB combinations.

SPACE
SHIFTSPACE
CTRLSPACE
Sends the SPACE, SHIFT+SPACE, and CONTROL+SPACE combinations.

DEL
SHIFTDEL
Sends the DELETE and SHIFT+DELETE combinations.

TEXT xxxxxx
Sends xxxxxx as text to the selected application.

DATE
Sends the current date.

TIME
Sends the current time.


COMMANDS: SPECIAL KEYS

PSION key
This sends the hotkey to the selected application.  The key should be in
the same format used by OPL, ie. lower-case for PSION, upper-case for
PSION-SHIFT.

HELPKEY
HELPINDEX
HELPKEY brings the help for the application.  HELPINDEX brings the help
index.

MENUKEY
This acts as if the menu key had been pressed.

DIAMKEY
This acts as if the diamond key had been pressed.


COMMANDS: SOUND

BEEP
Using beep on its own, produces a 'beep'.

BEEP time pitch
With parameters, this produces a 'beep' as specified.

time is measured in 1/32 seconds.  The frequency of the beep is
512/(pitch+1) KHz.

BEEP 5 300 produces a comfortably pitched beep.

PLAY filename ticks volume
This command instructs Install to play a Series3a sound file.  The filename
MUST be specified with the path and extension, although the drive may be
omitted.  The file is searched on all three of the Series3a drives.

ticks is measured in 1/32 seconds.

volume is a number between 0 and 5, with 0 being the loudest, 5 being the
quietest.

So to play one second of the file HAHN.WVE, located on drive A, at the
loudest possible volume, you would use the command PLAY \WVE\HAHN.WVE 32 0

COMMANDS: DISPLAY COMMANDS
ECHO OFF
This stops the commands in the file being printed to the screen.

ECHO ON
This turns on the option disabled by ECHO OFF.

ECHO text
Displays the specified text to the screen.

CLS
Clears the screen where the Macro commands are displayed.

SHOWPIC fiilename
This command displays a .pic file on the screen.  The pic file format is
that used by OPL, NOT the spreadsheet format.  The pic is shown including
its grey plane.

PAUSE length
Pauses for a length of length/20 seconds.  If this is given as a negative
number, then the pause can be broken with a keypress.


COMMANDS: ADVANCED FEATURES
FOREGROUND
Brings the Macro screen to the foreground.

BACKGROUND
Puts the Macro application into the background.

LOOP
Returns the script file to the first line.

WAIT key
Waits for the specified key to be press WITH THE CONTROL KEY, so if you
enter WAIT z, then the Macro application will wait for CONTROL-Z.  This key
combination may be pressed in ANY application.

So to set up a macro on a hotkey, do the following:

APP appname
GOAPP

WAIT key
    ...
    ...
LOOP

This will then continuously wait for the key to be pressed, and start again
each time.


INTERNATIONAL USE
Unlike Install, the Macro program does not need any knowledge of which
language your Series3a runs in.  In should, therefore, be able to run on
all Series3a, although no guarantee is given that this is so.

TECHNICAL SUPPORT

If anyone has any problems with Macro 1.1, please get in touch.

Please let us know which version of Macro you are running (eg. 1.1), and
which version of Series3a you have (eg. 3.22F - Deutsch).

Please also send us a copy of your macro command file, since this makes
diagnosing problems easier.

