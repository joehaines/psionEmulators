12. Readme.txt
Customer Relation Manager 1.1 for the Psion Series 5 and 5mx (C) 2000 Marc de
Oliveira & PocketIQ 
------------------------------------------------------------------------------
------------

Released as shareware.

This tool lets you keep track of the status and progress of all your customer
related tasks.

Installation:
------------

If you are upgrading from a version prior to 1.1 then remember to execute the
CRMUpg11.opo after installation.

Installation from SIS file:

1) Execute the Customer.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.

4) Install PIQInfo.sis, pythia.sis, systinfo.sis and sysram1.sis if necessary.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it's not, enable it in the 'Preferences' dialogue.

2) Using PsiWin copy the files PYTHIA.OPO, PIQInfo.OPO, PIQInfo.RSC and
PIQInfo.MBM to the folder \system\opl (you might have to create the folder)

3) Go into \System\Apps on C: or D: and make a new folder called "Customer"

4) Using PsiWin, copy the following files to the new \System\Apps\Customer
folder:
  Customer.app
  Customer.aif
  Customer.mbm
  Customer.hlp
  CreateDB.opo

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

2000.03.01: Version 1.0

2000.03.14: Version 1.0.1
 - Fixed error occuring when using the Find function (Ctrl-F).

2000.07.31: Version 1.0.2
 - Missing file is now correctly installed.
 - Changed to 4-color mode making the menus faster.
 - A blinking "Closing" message is now shown during exit, so that you know
that you pressed the correct key.

2000.08.22: Version 1.0.3
 - Fixed error when generating lists.
Note: if you are installing this version on top of a previous version you
will need to run the FixList.opo file to fix this error.

2000.10.01: Version 1.1.0
 - New icons show if notes have been placed on the current company or person.
By clicking the icon the related note can be edited directly.
 - You can now see if there are internal task notes behind the public task
notes without having to switch view to the internal notes (and vise versa).
 - Lists are now generated correctly with the short cut commands Ctrl-O,
Ctrl-W, Shift-Ctrl-W, Ctrl-C and Ctrl-X.
 - A new "Target Date" item has been added to the Task section, so that you
can specify when you expect to complete a task.
 - Some screen redrawing has been removed to make navigation a little faster.
 - The correct CRM icon is shown during startup and in the about box.