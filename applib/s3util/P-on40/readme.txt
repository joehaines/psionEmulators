PowerOn (for the Psion 3a)
Version 4.0, 14-Aug-95
(c) Matthew Goldsbrough 1994-95
Compuserve 73241,2533
-------------------------------


What does PowerOn do?
---------------------
PowerOn traps system events, including those of 'machine on' and 'machine off'. Whenever the Psion 3a is switched on or off, a specified sound file can be played. It can also be used to record a voice memo, so that you'll get a reminder the next time you turn your 3a on.

When the program starts for the first time, it asks you for the sound file you want to play whenever the Psion 3a is switched on. You locate the file in the usual manner. You are also given the option to play the same or a different file when the 3a is manually switched off. Once the files are selected, PowerOn waits for system events.


The legal bit
-------------
PowerOn is freeware. Please distribute it freely but unaltered. It is not warranted in any way.


New in version 4.0
------------------
1. Silent on/off switching by holding the shift key down.

   Embarrassed by PowerOn playing jokey sounds in public places? Just hold down the SHIFT key when the ON or OFF key is pressed, and the playing of the sound file will be suppressed.

2. Silent exit by holding the shift key down.

   Similarly, exiting the program with PSION-X will usually play a short sequence of beeps, but this can be suppressed by holding down the SHIFT key at the same time.

3. Menu and hotkey control

   All functions can be controlled from menus and hotkeys. Hit the MENU button to find out what's available.

4. Configuration file saves the set-up

   All program choices are saved to a configuration file, so that the next time you start PowerOn you don't have to set it up from scratch. The configuration file used is created in the \APP\POWERON directory on the default disk, and is called POWERON.INI.

5. Volume and file selection allowed while program is running

   Swap the files used and the volume they play at, while PowerOn is running. The same volume is used for all sounds.

6. Reminder text and sound file can be played at power on

   In addition to the ON/OFF sounds, you can record a short voice or text memo to play at power on. The purpose is to give you an untimed reminder - "something I should do next time I use my 3a".

7. Status of the program shown at all times

   The files selected, the volume for playback, and any reminder note or voice memo, is listed on the main screen.


Known limitations
-----------------
I'd like PowerOn to be more sophisticated in how it looks for files on all kinds of SSDs, but through laziness, wanting to keep PowerOn a small program, and lack of skill with OPL, it's not as smart as it might be. It worked just fine for almost everyone that's used it, but let me know if you have problems, and I'll overcome the laziness, and learn a bit more OPL!


Installation
------------
The P-ON40.ZIP file contains:

1. The program

POWERON.OPA
Transfer to an \APP\ directory on the Psion 3a and install on the system screen with PSION-I

2. This file

README.TXT
Not needed on the 3a.


Thanks to:
----------
I got the icon from a collection produced by Alan Jones (CIS 100044, 2611). Mark Esposito gave me the inspiration for the use of the .INI file, although not the code, 'cos I lost Mark's suggested OPL, and I wanted to figure it out myself anyway. Those kind enough to beta test this version were: Mark Hadfield, Ian Hunter, Steven Shone, Martin Skowronski, and Nigel Wright.


Files excluded from release 4.0 onwards
---------------------------------------
In previous releases some other files were included. They're not included in this release. Here's why:

1. A choice of sound files

AVONSCOM.WVE	Avon's communicator from Blake's Seven
KIRK.WVE	Captain Kirk's communicator from Star Trek
ORACON.WVE	ORAC from Blake's Seven switching on
ORACOFF.WVE	ORAC from Blake's Seven switching off

These files led to the previous version of PowerOn being removed from CIS because the copyright to these sounds wasn't held. I've excluded them from this release so that a similar problem doesn't prevent PowerOn being freely distributed. Don't be dismayed though, these and many other sound files are still available on CIS and other online services. You might just have to search for them a little harder, I'm afraid.

Any .WVE files you want to use can be in any directory you want on the 3a, although PowerOn wil find them easiest if they're in the \WVE\ directory of the default disk.

2. The source and icon, POWERON.OPL and POWERON.ICO

I'd like to have some contact with whatever changes people make to PowerOn from this point onwards. If you're interested in developing PowerOn further, I'll e-mail you the source code as long as you keep me posted on the changes you make. If you have these files, you'll want to put them both in an \OPL\ directory on the Psion 3a.




Have fun,

Matthew Goldsbrough
Compuserve 73241,2533
