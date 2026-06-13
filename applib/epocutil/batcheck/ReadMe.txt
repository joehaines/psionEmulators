      *****************************************************
             BatCheck for EPOC Computers v1.2
                      18 September, 1999
      *****************************************************



BatCheck is a small and extremely useful application that monitors the
status of the main and backup batteries.


FEATURES
********

 * Unlike other programs that only tell you the status of the batteries
   when they are in foreground, BatCheck will gauge the battery levels
   at certain time intervals (1-60 minutes, selectable).

 * Normally the program sits in the background using virtually no battery
   power itself, wakes up when its time for a checking, and goes back to
   sleep if the batteries are good, without disturbing you or being noticed.
   If the batteries are not good, the program comes to the foreground
   and warns you.

 * You can select two warning levels for both the main and backup batteries:
   a 'low' and a 'replace' voltage level. This is particularly useful if
   you use rechargeable cells where the voltage usually drops fast. By
   setting a warning level higher than the built-in levels you can avoid
   accidents caused by sudden battery power loss.

 * If you want, you can ask the program not to warn you repeatedly,
   only once the first time when the main or backup levels reach the low
   level. Next time it will only warn you if the machine has been switched
   off then back on, or when they reach the replace level.
 
 * You can ask the program to do a check at every switching on. If
   you have lots of other startup programs, you can select a delay time for
   battery check (0-20 seconds), so it will pop up after all the startup
   programs are done with their jobs.

 * You can also ask the program to monitor if the external power is connected
   or not. This is great if you use your machine plugged-in, and the plug
   slips out of the machine without being noticed.

 * (NEW!) You can select a top limit for the main batteries, to prevent
   accidental overcharge of rechargeable batteries by the mains adaptor.

 * The program will tell you the voltage in 2 decimal values for greater
   accuracy (e.g. 2.28 Volts).
 
 * Toolbar support and full system-compliance.


Try BatCheck, and there will be one less thing to worry about!


INSTALLATION
************

    1) BatCheck comes in either a ZIP file or a SIS installation package.
       To install a SIS package, copy it to your Series 5, and if you have
       the Add/Remove  control panel installed, just click on the SIS file,
       and it will be installed. If you  don't have this control panel yet,
       you can download it the official Psion site (Downloads section).
       Please make sure that you also install sysram1.sis and systinfo.sis.

    2) If you install from a ZIP file, copy the files manually to the right
       location after unzipping:
       Sysram1.opx, Systinfo.opx: C:\SYSTEM\OPX\ (or D:)
       All other files: C:\SYSTEM\APPS\BATCHECK\ (or D:)

    3) If you are upgrading from a previous version please delete
       c:\system\apps\batcheck.ini (or d:) manually, otherwise the program
       will report an error message and won't run.

    4) You're now ready to run BatCheck. It's icon should appear on
       the Extras bar, so just click it!


STATUS OF THE PROGRAM
*********************

    BatCheck is FREEWARE!  I made it because I have almost lost all my
    data  from  my  Psion, some program source codes even got damaged.
    Well, I learned the lesson that rechargeables tend to die fast, so
    here is a solution. Feel free to distribute the program, but don't
    change the contents and layout of the original zip file please. If
    you like this program,  please have a look at my other programs as
    well (Encrypt-it!, FlashBack, Statistics, Moon).

    The program has been fully tested and appears to have no bugs, but
    if you find any or have any comments then I welcome your input
    e-mail on:

           csutora@usa.net

    Updates of BatCheck (and my other programs) can be found on my
    web page at:

           http://members.tripod.com/~csutora/


REMARKS
*******

    The author cannot and does not accept any liability for loss caused
    by error, defect or failure of the software including any loss of
    any kind. No guarantee is either offered or implied by the author.
    Use of this software is entirely at your own risk.
    The program now uses a permanent UID.


CREDITS
*******

    Thanks to Andrew Giddings for his suggestions and coding advice.

    Thanks to Gérald Aubard for the French and to Christian Bruss for
    the German translation.

    And last but not least thanks to you for trying BatCheck! :-)



BatCheck and all associated files are Copyright (C) 1998-99 Peter Csutora.
All Rights Reserved.

End of Readme.