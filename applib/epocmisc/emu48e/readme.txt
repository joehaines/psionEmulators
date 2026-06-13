Emu48E v1.5 : A freeware HP 48/49 calculator emulator for Epoc32.
-----------------------------------------------------------------
Author : Raphael MASSOT, based on the Emu48 sources of Sebastien CARLIER
Date : 04/01/2004


1) About Emu48E.
2) Installation.
3) Downloading the ROM (not included)
4) Using the emulator.
5) Menus.
6) Versions.
7) Bugs.


1) About Emu48E.
----------------
Emu48E is a port of Emu48 on EPOC32, it works on Psion Revo, series 5 (ER3 and ER5), Netbook, Ericsson MC218 and Osaris (and perhaps other ones). It's an Eikon application programmed in c++.
Now works on Nokia Communicator 9200 series (Symbian 6.0) and Sony-Ericsson P800 (Symbian 7.0, don't work in some P800, I don't know why for now).

2) Installation.
----------------
WARNING : The ROM files are no more included see 3).

Installation is done by the .sis file (language : french, english or german).
Removing the program don't delete .bak files, you don't have to use the .bak of a Sx with a Gx Rom and vice versa (I haven't test it but I think it can cause strange behaviours).

The program can be installed on drive C, D or E into \system\Apps\Emu48E\ :

  - Emu48E.app : the application.
  - Emu48E.aif : application aif file.
  - Emu48E.rsc : application resources.
  - annunc.mbm : annunciators picture.
  - main*.mbm, main2SGx.mbm, main5*.mbm : calculator pictures.
  - keya2SGx.mbm, keyl2*.mbm et keyr2*.mbm : keys pictures for the medium view.

and after the first use : 
  - chipset.bak : Saturn processor backup (480 bytes)
  - ramP0.bak : RAM port 0 backup (32KB for a Sx, 128KB for a Gx)
  - ramP1.bak : RAM port 1 backup (1 byte if empty, 128KB if used)
  - ramP2.bak : RAM port 2 backup (1 byte if empty, 128KB if used)
  - Emu48E.ini : settings backup

The three files groups (application, pictures and rom) can be installed separately on drive C, D or E.

Note : The files .bak can be backed-up and restored in case of big crash.


3) Downloading the ROM.
--------------------------

The Rom files are no more included and have to be downloaded then copied in a directory
of your choice (the first time you run the emulator, you have to select the .rom file).
The ROMs are available at www.hpcalc.org at:

http://www.hpcalc.org/hp48/pc/emulators/ for HP 48
http://www.hpcalc.org/hp49/pc/rom/ for HP 49G


  * For an HP 48 Sx :
	- download the file "HP 48Sx Revision J ROM" (sxrom-j.zip)
	- unzip
	- rename the file in sx.rom

  * For an HP 48 Gx :
	- download the file "HP 48Gx Revision R ROM" (gxrom-r.zip)
	- unzip
	- rename the file in gx.rom

  * For an HP 49 G :
	- Download the file "Unsupported Beta ROM 1.19-6" (beta1196.zip)
	- unzip rom.49g in a temporary directory
	- copy the utility conv49.exe included in Emu48E in this directory
	- run utility in command line : conv49 rom.49g 49g.rom

4) Using the emulator.
----------------------

Memory use by the differents versions (without backup and with ROM) :

Version		On the disk	Free memory to work
-------		-----------	-------------------
48 Sx		500 KB		1 MB
48 Gx		800 KB		2 MB
49 G		2.3 MB		5.3 MB
 
 At startup, the backup files (*.bak) are loaded if the emulator find them, otherwise the RAM is zeroed. If chipset.bak is not found and ram*.bak are here, only the chipset is zeroed.
 When the application is closed, the settings and backup files are saved automatically.

 Psion Keyboard:
   - 1, 2, 3, 4, 5, 6, 7, 8, 9, 0
   - A, B, C, D, E, F (HP48 menu keys)
   - *, /, +, -
   - cursors arrow
   - . -> .
   - , -> .
   - j -> *, k -> /, l -> +, m -> - (azerty)
   - y -> *, u -> /, i -> +, o -> - (qwerty)
   - Esc -> On
   - Enter -> Enter
   - Space -> Space
   - Backspace -> "<-"

 Special keys for the Nokia 9210:
   - Alphabetical keys are mapped to the HP48 alpha-keys.
   - Shift-A -> HP48 Alpha
   - Shift-LeftArrow -> HP48 left shift
   - Shift-RightArrow -> HP48 right shift
   - Shift-BackSpace -> HP48 Del


5) Menus.
---------

 File menu :
 -----------
  - New : delete all backups and zero the RAM.
  - Reload : reload the last backup.
  - Save : save the calculator RAM to file.
  - Reset : calculator reset (the RAM isn't zeroed)

  - Settings :
     . display
        * Bitmap real : display is updated every time the display memory is writed. The update is slow especially whith games.
        * Bitmap fast : display is updated only when the HP48 processeur goes to shutdown. It's faster than bitmap real.
        * Frame buffer (for Psion only): write directly in screen memory. It's the fastest.
     . Keyboard :
        * normal
        * simulated : simulate key up event if other keyboard application delete it.
     . Key pressed : minimum of time for which the key must staying down.
     . Grayscale sync (on Psion): screen scanning synchronisation for grayscale display.
     . Backup : save Ram to disk on exit.

  - Exit : close the emulator (in option: backup RAM on disk).

 Edit Menu :
 -----------
  - Save object : save the first object of the stack in a file.
  - Load object : put a object on the stack from a file.
  - Copy : copy a string from the stack to the clipboard.
  - Paste : paste a string from the clipboard to the stack.

 View Menu :
 -----------
  - View select :
      * small : simple Lcd bitmap, for the Psion Revo
      * medium : Lcd bitmap doubled, large keys, the keys picture change with the mode (shift and alpha), for Revo and large screen.
      * large : Lcd bitmap doubled, large keys, labels above the keys, for large screen.


6) Versions.
------------

 * v1.5:
 -------
 - ROM file selector at startup.
 - Copy/paste real number.
 - Copy/paste big integer HP49.
 - Keys *,/,+,- are language depended.
 - German install

 * v1.4:
 -------
 - HP 49G ROM support.
 - Port P2 support for HP 48Gx (128 Ko).
 - New item : Save RAM.
 - New item : Reload last backup.
 - Bugfix kern-exec 3 in bitmap mode and in frame-buffer mode with the Netbook.
 - Bugfix in loading backup files on C: if the program is installed on drive D: or E:

 * v1.3:
 -------
 - Use of frame buffer for a fast display (problem with Netbook : see 6) Bugs).
 - Grayscale renderer is acceptable (adjustable in the settings).
 - Minimum of time for key pressed in ms than in ticks.
 - Time display is correct (timer bugfix)
 - Settings are saved.
 - RAM Backup in option
 - several keys can be pressed at the same time
 - Bugfix : error at backup if all files are installed on drive D:
 - Default skin more readable.

 * v1.2:
 -------
 - We can now load and save object from files.
 - Clipboard support for strings.
 - We can use the psion keyboard for + - * /, Enter, cursor, On, space, and backspace.
 - Bugfix : too short pen click on a key can cause the key not to be detected.
 - The keys picture are changing when the mode is shift or alpha in the 'medium' view.
 - Installation on C, D or E or the three.
 - Bugfix : the port P1 was zeroed at each start.
 - Bugfix : one day was missing to the date.
 - Skins with white background for the screen with low contrast.

 * v1.1:
 -------
 - There is now 3 views whith differents sizes for the Revo and lage screen.
 - Bugfix when the program is installed on drive D.
 - Bugfix of the dark screen when the calculator is turned off.
 - Bugfix of the E32User-CBase 42 error at startup after the program was closed with a blinking cursor.

 * v1.0:
 -------
 - It's the first release, then it's minimal comparing to Emu48.
 - The emulation speed is lower than the speed of a real HP 48.
 - The copy from/to the stack from/to the clipboard is not yet implemented.


7) Bugs.
--------
 - Don't run on some P800 (error when creating/loading CFbsBitmap)
 - no access to 1MB flash memory with the HP 49G
 - No beep support


--
Raphael MASSOT
Besancon, France
Mail : psiomas@free.fr
Web : http://psiomas.free.fr
