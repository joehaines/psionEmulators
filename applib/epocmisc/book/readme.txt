13. Readme.txt
Book Collection 2.7.2 for EPOC (C) Marc de Oliveira 1998-2000 & PocketIQ
------------------------------------------------------------------------------
------------

Released as shareware.

Organizes your book collection and book notes.


Installation:
------------

If you are upgrading from version 2.1.1 or lower, please, read the upgrade
section.


Installation from SIS file:

1) Execute the book.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it's not, enable it in the 'Preferences' dialogue.

2) Using PsiWin copy the files PYTHIA.OPO, EXPORT.OPO, CABINET.OPO,
PSCAPE.OPO and PSCAPE.MBM to the folder \system\opl (you might have to create
the folder)

3) Go into \System\Apps on C: or D: and make a new folder called "Book"

4) Using PsiWin, copy the following files to the new \System\Apps\Book folder:
  book.app
  book.aif
  book.mbm
  book.hlp

4) Copy the file sysram1.opx in the folder \system\opx

5) A new icon should appear on your Extras bar


Upgrade:
--------

To upgrade to version 2.3 from a previous version:

1) Backup the .DBF and .INI files of all your Cabinet Suite applications.

2) Perform the installation as described above. Do NOT over write or remove
the files with the extention .DBF or .INI.

3) Make sure that no Collection Suite application is being run.

4) Copy the CabUpg23.opo to any directory and execute it by double tapping on
it's icon. The program will upgrade all your Collection Suite .DBF files.

5) Delete the CabUpg23.opo file.

6) All your Collection Suite applications are now upgraded to version 2.3.
Note, you only need to perform this upgrade once. You should not try to
upgrade each Collection Suite application individually.


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


Registering the program will give you the following benefits:

 1) The register window will not pop up on startup
 2) You get free support via e-mail (Pythia@Palmscape.com)
 3) Your suggestions to new functionalities will be implemented (as far as
resources permits it)

Pricing:

Collection Suite programs are registered using a Collection Gold Code. This
code is valid for all present and future Collection Suite programs. The
Collection Gold Code costs USD 30.

How to register:

Go to the Palmscape home page at:
  http://www.Palmscape.com

Here you can register using credit card or cheque.

After receiving your registering fee I will mail you a Key that you can use
to register the program.


Made by:
--------

Marc de Oliveira 
for Palmscape & Pythia Information

Web: http://www.Palmscape.com
E-mail: Pythia@Palmscape.com


History:
--------

1998.05.09: Version 2.0
 - The Cabinet Suite started on version 2.0 because Personal Wine Cabinet is
considered version 1 of The Cabinet Suite.

1998.05.14: Version 2.0.1
 - Palmscape splash screen shown during startup.

1998.05.18: Version 2.0.2
 - Cabinet Gold Code enabled
 - Fixed: When toolbar is hidden fields disappear

1998.05.21: Version 2.0.3
 - Export/Import bug fixed
 - Summary fails if no amounts have been entered: fixed
 - Position marker can now show more than 2 digit positions.

1998.06.08: Version 2.1
 - New utility (Find Note) to search through the Cabinet Notes
 - Serious increase of size for the Where Clause specification from about 30
charaters to about 220 characters. Now you can really make advanced searches
 - Faster
 - Some bug fixes

1998.07.03: Version 2.2
 - New utility to store sql queries for reuse.
 - To make advanced searches easier a list of all field names are now shown
on the Where Clause window.

1998.07.06: Version 2.3
 - The main screen has been changed to allow for a three line view of your
notes. By tapping on the three-line window you open the Notes Editor.
 - The time used to move from one element to another has been reduced
significantly.
 - Find Previous/Next keys are moved from Ctrl-O/Ctrl-P to Ctrl-I/Ctrl-J to
follow Psion standard.
 - The program no longer crashes when old version of ps.opo is used.
 - Database files are now only compacted if they have been changed. This
makes the shutdown of the application faster when you have not updated it.
 - Book formats are now sorted alphabetically.
 - Import/Export bug fixed.

1998.10.01: Version 2.4
 - A new Element List (Shift-Ctrl-L) lets you browse through your collection
items quickly. The Element List is based on your current query criteria and
sort command.

1999.02.25: Version 2.5
- The Import/Export uility has been extended with enough parameters to make
it possible to export cabinet information in HTML format for direct upload on
the world wide web.
- Multible import/export settings can be named and  stored for different
types of import/export.
- The actual screen size is now exploited (horizontally) giving more room for
fields when using the EPOC emulator.
- The date format used in the program can be defined in the system control
panel.
- Book Cabinet has been renamed to Book Collection.
- No Palmscape logo during startup.
- New Palmscape logo in the About box.

1999.03.28: Version 2.5.1
 - Geofox enabled: Screen no longer reacts when the mouse passes over a field.

1999.06.09: Version 2.5.2
 - To avoid a OPL bug the where clause is now cleared when you create a new
record.

1999.06.29: Version 2.6
 - The application has been updated to work on both the Psion 5 and 5mx.

2000.06.01: Version 2.7.2
 - Released as a PocketIQ product.