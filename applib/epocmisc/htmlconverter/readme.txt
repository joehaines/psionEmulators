9. Readme.txt
HTML Converter 1.3 for the Psion Series 5 and 5mx (C) Marc de Oliveira &
PocketIQ 1999-2000
------------------------------------------------------------------------------
------------

Released as shareware.

Manage your home pages on your Psion.


Installation:
------------

Installation from SIS file:

1) Execute the HTMLConv.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it's not, enable it in the 'Preferences' dialogue.

2) Using PsiWin copy the files PYTHIA.OPO, PIQInfo.OPO, PIQInfo.RSC and
PIQInfo.MBM to the folder \system\opl (you might have to create the folder)

3) Go into \System\Apps on C: or D: and make a new folder called "HTMLConv"

4) Using PsiWin, copy the following files to the new \System\Apps\HTMLConv
folder:
  HTMLConv.app
  HTMLConv.aif
  HTMLConv.mbm
  HTMLConv.hlp
  HTMLCrea.opo

4) Copy the files systinfo.opx and sysram1.opx in the folder \system\opx

5) A new icon should appear on your Extras bar


Problems
--------

1. Read only files

Some times files become read only while being installed. None of the files
should be read only. If you get any errors like the program is unable to
write information to your Psion go through the program files to see if some
of them are read only.

In the system-view you can open the file property window by selecting
(highlighting) a file and pressing Ctrl-P. In the property window you can see
and set/clear the read only checkbox.

2. Copying between C and D

Be very careful when you move or copy applications between the C and D
drives. It seems that the Psion is not correctly updated when applicaions are
copied from C to D or vice versa.

Always delete the application directory from one drive before reinstalling on
the other drive.

3. Low on battery

When starting up this program for the first time a lot of writing is done to
the disk. If your machine is low on battery these writes can go wrong and
turn the application files bad. This should only be an issue when running the
application the first time.

4. Not enough memory

This program needs up to 300 Kb of free memory on the C drive. If you try to
run the program with less than 300 Kb of free memory on the C drive it may
not behave as expected.


Usage:
------

Press Ctrl+Shift+H to see help file.


Bugs:
-----

 - When opening the same HTML file twice with the Web Browser it might be
cashed and, hence, not automatically reloaded. To reload the file press
Ctrl-R.


Registering:
------------

Registering the program will give you the following benefits:

 1) The register window will not pop up on startup
 2) You get free support via e-mail (Pythia@PocketIQ.com)
 3) Your suggestions to new functionalities will be implemented (as far as
resources permits it)


How to register

Go to the PocketIQ home page at:
  http://www.PocketIQ.com

Here you can register using credit card or cheque.

After receiving your registering fee I will mail you a Key that you can use
to register the program.


Made by:
--------

Marc de Oliveira
for Pythia Information & PocketIQ

Web: http://www.PocketIQ.com
E-mail: Pythia@PocketIQ.com


History:
--------

1999.08.14: Version 1.0

1999.08.29: Version 1.0.1
 - Installation bug fixed

1999.09.11: Version 1.1.4
 - Source files can now be selected for editing or convertion from a pop-list.
 - The name of the default file can now be defined as a preference (Ctrl-K).
 - The delay needed when starting up the web browser can be setup in the
preference screen (default is 120).
 - When opening a HTML document in the web browser a reload is made to
prevent a previously cached version to be displayed.
 - Bug fix: Last character is duplicated.
 - Bug fix: Using a source folder with no .TXT files gave an error.

2000.01.20: Version 1.2
 - Released through PocketIQ.
 - Graphics are adjusted to the current screen size, so that it should work
on any EPOC machine.
 - After agreement with Igor Akaev the HTML.S5 help file is shipped with HTML
Converter. You can open the file by pressing Shift-Ctrl-I.

2000.04.06: Version 1.2.1
 - A new preference lets you set up your default converter (Standard or Raw).

2000.05.01: Version 1.2.2
 - The maksimum size of Ascii source files is increased from 2Kb to 10Kb.

2000.07.31: Version 1.3
 - "Editor" by Symbian is integrated in HTML Converter as the editor for
maintaining text files. This removes the size restriction of text files and
adds features like search, zoom etc.