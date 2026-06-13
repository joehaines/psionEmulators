******************************************
KeyDirect  
******************************************
Copyright (c) 2001 Ivo Woltring
Freeware 
Version 3.01
Modified: May 18, 2002

Table of contents               
        1. File contents
        2. Introduction
        3. Upgrading
        4. Installation
        5. History
        6. Wishes

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1. FILE CONTENTS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The compressed file KeyDirect.zip contains 4 files:

        KeyDirect.sis	
        Disclaimer.txt
        Readme.txt
        leesmij.txt

KeyDirect supports 2 languages:
			English
			Dutch     

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
2. INTRODUCTION
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
KeyDirect is a very useful little app that makes redirecting Silkkeys 
(like: word, sheet, Cut & paste, zoom, ...) to another application easy.

If you use a program under the extra's bar very often and one of the Silkkeys
not... Why not redirect it to this program. Fast Easy Cool! ;-)

Keys:
Redirected Silkkey         : Rotate though open files of redirected program
Ctrl-Redirected Silkkey    : KeyDirect main screen
Ctrl-Fn-Redirected Silkkey : Quit (this combination works from almost anywhere)

Tips:
* You can redirect more Silkkeys by restarting KeyDirect from the extrasbar while
  holding the <Fn> key! (press Fn then Extras > KeyDirect). A temporary
  shortcut will then be created for restart porposes.
* You can also Create a shortcut with default keydirect settings by pressing
  'New file' from the system screen. Just give it a nice name and a shortcut
  will be created with the settings you choose in that session.
  Next time just start by tapping the shortcut. No questions will be asked.

Note!:
* All Shotcuts that are called "...\KeyDirect..." are considered temporary
  shortcuts. This means that if the system gives KeyDirect a sign to quit the
  shortcut will remain (e.g. for a backup) so that it can be restarted with
  the same options. But if closed trough KeyDirect itself the shortcut will
  be removed.
* All Shortcuts with another name are considered permanent and are meant to
  be used repeatedly. They will not be removed. Of course you can delete them
  at your leisure.


~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
3. UPGRADING
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- Now availabe in Dutch and English
- Application scanning now on all drives (S7 bug)
  
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
4. INSTALLATION
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1. There are 2 methods to install KeyDirect depending on where you
   have copied the .sis file and what setup you have;
   
   - On a PC with PsiWin
     From the Windows Explorer double click the .sis file, this will 
     automatically install KeyDirect onto your machine. If that
     doesn't work right click the My Psion icon and click the Install New 
     Program option.
     If you do not have the Install New Program option then download
     setup.exe from our site or from Psions' site <http://www.psion.com>
     install it and try again.
   - On a EPOC machine
     Tap the .sis file to install, if this does not work then you do not
     have the Add/Remove utility, in this case download
     instexe.exe from our site, or psions' site <http://www.psion.com> and
     copy it to your machine and try again. 

2. Now press Extras|KeyDirect, to start.

3. To uninstall goto the Control Panel, tap the Add/Remove icon, select 
   KeyDirect and finally press the Remove button.


~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
5. HISTORY
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
v3.01 - Fixed Netbook / series 7 bug.
v3.00 - removed the paterned screen. Took unnesasary memory.
      - now removes the Logo + sprite from memory if moved to the background (saves Memory)
      - some textual stuff
v2.10 - added RSC support
      - added dutch as supported language
      - Application scanning now on all drives (S7 bug) - dApps OPM upgraded -
      - Removed "Silkeys.opo" support
v2.09 - added Series 7 support
v2.08 - Fixed small screenproblem for revo machines
v2.07 - Added Revo support
v2.06 - fixed RunApp bug. I stupidly deleted a variable! 
v2.05 - fixed Extra space bug! Some S5mx machines generate an extra space
        when asked to give there name. So my proggy didn't work!
        Thanks to Pardo I hope to have fixed it in this version.
v2.04 - added netBook support (test phase)
      - adjusted Ericsson MC 218 support to other silkkey names
      - added support for creating a file to help me make this proggy
        work on all platforms
      - adjusted the logoscreen for all platforms?
v2.03 - added Ericsson MC 218 support
v2.02 - added shortcut handling
      - added restart after backup support
v2.01 - fixed 'update counter' bug
      - New! create shortcut with session settings for future quickstart
v2.00 - Fixed Focus bug
      - Fixed 'Zero' bug.
      - Now checks if a session is still open before trying to bring it to
        the foreground
      - Now checks if Keydirect is run on a Series 5mx. if not stop!
      - Optional 'Open session if no session is found'.
      - Optional 'Sound when quit'
      - This version has been tested! v1.04 Didn't work correctly
v1.04 - ReadUID bug fixed.
v1.03 - To Clean up the code KeyDirect Now makes use of the dApps OPM
      - KeyDirect can now rotate through up to 100 open apps.
v1.02 - Now Keydirect will close if a backup is performed.
      - Now Keydirect will close if Ctrl-shft-E is pressed from the system
        screen
      - New session can be opened through New File from the system screen
v1.01 - Fixed very small bug (no reports on it yet)
v1.00 - First public release


~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
6. WISHES
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
- Make it work on all platforms (in Finished?)
- Open session with template if template put in default dir
- Make it available in more languages (Now in progress)
- Add macro record/playback
- Make it possible to link to a document in place of an app
- Put it all in one app so no need to start new sessions

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
For the latest version of KeyDirect and to download more 

               
Enjoy,
Ivo Woltring.
