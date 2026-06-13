14. Readme.txt
Video Collection 2.7.2 for EPOC © PocketIQ & Marc de Oliveira 1998-2000
------------------------------------------------------------------------------
-----------

Released as shareware.

Organizes your film/recordings collection.


Installation:
------------

To upgrade from a version of Video Collection prior to version 2.3 see the
Upgrade section.

Installation from SIS file:

1) Execute the video.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it's not, enable it in the 'Preferences' dialogue.

2) Using PsiWin copy the files PYTHIA.OPO, EXPORT.OPO, CABINET.OPO,
PIQINFO.OPO, PIQINFO.RSC and PIQINFO.MBM to the folder \system\opl (you might
have to create the folder)

3) Go into \System\Apps on C: or D: and make a new folder called "Video"

4) Using PsiWin, copy the following files to the new \System\Apps\Video
folder:
  video.app
  video.aif
  video.mbm
  video.hlp
  upg21.opo
  upg22.opo

4) Copy the files systinfo.opx and sysram1.opx in the folder \system\opx

5) A new icon should appear on your Extras bar


Upgrade from version prior to 2.0.2:
--------

1) Backup your old Video Cabinet files.

2) Install the new files as described in the Installation section. Do not
remove old .ini and .dbf files as they contain your settings and data.

3) Copy the file upgvc202.opo to a folder on your Psion.

4) Make sure that Video Cabinet is not running.

5) Execute the upgvc202.opo.

6) Remove the upgvc202.opo file.

Your video.dbf file is now upgraded to version 2.0.2.


Upgrade from version prior to 2.3
----------

1) Backup the .DBF and .INI files of all your Collection Suite applications.

2) Perform the installation as described in the installation paragraph. Do
NOT over write or remove the files with the extention .DBF or .INI (these are
data and setup files).

3) Make sure that no Collection Suite application is being run.

4) If you are upgrading from a version prior to 2.1 execute the upg21.opo
file by double tapping on it's icon.

5) If you are upgrading from a version prior to 2.2 execute the upg22.opo
file by double tapping on it's icon.

6) Copy the CabUpg23.opo to any directory and execute it by double tapping on
it's icon. The program will upgrade all your Collection Suite .DBF files.

7) Delete the CabUpg23.opo file.

8) All your Collection Suite applications are now upgraded to version 2.3.
Note, you only need to perform the CabUpg23.opo upgrade once. You should not
try to upgrade each Cabinet Suite application individually.


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

Pricing:

Collection Suite programs are registered using either individual codes or a
Collection Gold Code. This code is valid for all present and future
Collection Suite programs. The Collection Gold Code costs USD 30.

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

1998.05.17: Version 2.0
 - The Cabinet Suite started on version 2.0 because Personal Wine Cabinet is
considered version 1 of The Cabinet Suite.

1998.05.21: Version 2.0.1
 - Export/Import bug fixed
 - Summary fails if no amounts have been entered: fixed
 - Position marker can now show more than 2 digit positions.

1998.06.08: Version 2.1
 - Tape List is added with Tape Lenth and Lent To information.
 - Find Space utility added to search for best available space on your tapes.
 - Find Note utility added to let you search the Cabinet Notes.
 - Alert box can handle longer messages.
 - Summary Information bug fixed.
 - Lent To field on Cabinet Card removed (it is more correct to use the Tape
List).

1998.07.03: Version 2.2
 - New utility to store sql queries for reuse.
 - To make advanced searches easier a list of all field names are now shown
on the Where Clause window.

1998.07.06: Version 2.3
 - The main screen has been changed to allow for a three line view of your
notes. By tapping on the three-line window you open the Notes Editor.
 - The time used to move from one element to another has been reduced
significantly.
 - Find Space now only looks for space on tapes that are home (not lent out).
 - Database files are now only compacted if they have been changed. This
makes the shutdown of the application faster when you have not updated it.
 - Find Previous/Next keys are moved from Ctrl-O/Ctrl-P to Ctrl-I/Ctrl-J to
follow Psion standard.
 - The program no longer crashes when an old version of ps.opo is used.
 - Video medias are now sorted alphabetically.
 - Find Space no longer crashes when you have less than five tapes defined.
 - Export/Import bug fixed

1998.10.01: Version 2.4
 - A new Element List (Shift-Ctrl-L) lets you browse through your collection
items quickly. The Element List is based on your current query criteria and
sort command.

1999.02.20: Version 2.5
- The Import/Export uility has been extended with enough parameters to make
it possible to export cabinet information in HTML format for direct upload on
the world wide web.
- Multible import/export settings can be named and  stored for different
types of import/export.
- The actual screen size is now exploited (horizontally) giving more room for
fields when using the EPOC emulator.
- The date format used in the program can be defined in the system control
panel.
- Video Cabinet is renamed to Video Collection.
- No Palmscape logo during startup.
- New Palmscape logo in the About box.

1999.03.28: Version 2.5.1
 - Geofox enabled: Screen no longer reacts when the mouse passes over a
field.15. Readme.txt
Stamp Collection 2.5.2 for the Psion Series 5 (C) Palmscape & Marc de
Oliveira 1999
------------------------------------------------------------------------------
------------

Released as shareware.

Organize your Stamp Collection.


Installation:
------------

Installation from SIS file:

1) Execute the stamp.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.

4) Install Pscape.sis, pythia.sis, systinfo.sis and sysram1.sis if necessary.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it's not, enable it in the 'Preferences' dialogue.

2) Using PsiWin copy the files PYTHIA.OPO, EXPORT.OPO, CABINET.OPO,
PSCAPE.OPO and PSCAPE.MBM to the folder \system\opl (you might have to create
the folder)

3) Go into \System\Apps on C: or D: and make a new folder called "Stamp"

4) Using PsiWin, copy the following files to the new \System\Apps\Stamp
folder:
  Stamp.app
  Stamp.aif
  Stamp.mbm
  Stamp.hlp

4) Copy the files sysram1.opx and systinfo.opx into the folder \system\opx

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
 2) You get free support via e-mail (Pythia@Palmscape.com)
 3) Your suggestions to new functionalities will be implemented (as far as
resources permits it)

Pricing:

Collection Suite programs are registered using either individual codes or a
single Collection Gold Code. This code is valid for all present and future
Collection Suite programs. The Collection Gold Code costs USD 30.

How to register

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

1999.02.14: Version 2.5
 - The Stamp Collection started on version 2.5 to synchronize with the other
Collection Suite applications.

1999.03.28: Version 2.5.1
 - Geofox enabled: Screen no longer reacts when the mouse passes over a
field.15. Readme.txt
Music Collection 2.5.2 for the Psion Series 5 (C) Palmscape & Marc de
Oliveira 1998-1999
------------------------------------------------------------------------------
------------

Released as shareware.

Organizes your Record collection.


Installation:
------------

To upgrade from version 2.3 see the Upgrade section.

Installation from SIS file:

1) Execute the music.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.

4) Install Pscape.sis, pythia.sis, systinfo.sis and sysram1.sis if necessary.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it's not, enable it in the 'Preferences' dialogue.

2) Using PsiWin copy the files PYTHIA.OPO, EXPORT.OPO, CABINET.OPO,
PSCAPE.OPO and PSCAPE.MBM to the folder \system\opl (you might have to create
the folder)

3) Go into \System\Apps on C: or D: and make a new folder called "Music"

4) Using PsiWin, copy the following files to the new \System\Apps\Music
folder:
  music.app
  music.aif
  music.mbm
  music.hlp

4) Copy the files systinfo.opx and sysram1.opx in the folder \system\opx

5) A new icon should appear on your Extras bar


Upgrade
-------

1) Perform the installation as described above. Make sure not to remove your
Music.dbf and Music.ini files.

2) Copy the file upg231.opo to anywhere on your psion.

3) Execute the upg231.opo file.

4) After execution you can delete the upg231.opo file.


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
 2) You get free support via e-mail (Pythia@Palmscape.com)
 3) Your suggestions to new functionalities will be implemented (as far as
resources permits it)

Pricing:

Collection Suite programs are registered using either individual codes or a
Collection Gold Code. This code is valid for all present and future
Collection Suite programs. The Collection Gold Code costs USD 30.

How to register

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

1998.08.15: Version 2.3
 - The Music Cabinet started on version 2.3.

1998.09.24: Version 2.3.1
 - Recordings are sorted by Band (sort code) and release year as default.
 - Help file updated in relation to band sort keys.
 - Bug concerning changing of sort keys fixed.

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
- Music Cabinet is renamed to Music Collection.
- No Palmscape logo during startup
- New Palmscape logo in the About box.

1999.03.28: Version 2.5.1
 - Geofox enabled: Screen no longer reacts when the mouse passes over a
field.15. Readme.txt
Home Collection 2.5.2 for the Psion Series 5 (C) Palmscape & Marc de Oliveira
1999
------------------------------------------------------------------------------
------------

Released as shareware.

Organize your Home Inventory.


Installation:
------------

Installation from SIS file:

1) Execute the home.sis file on your pc or Psion.

2) Choose C or D drive for the installation (make sure that there is enought
disk space).

3) The program will install itself.

4) Install Pscape.sis, pythia.sis, systinfo.sis and sysram1.sis if necessary.


Installation from ZIP file:

1) Make sure the \System folder is shown on your Series 5 system screen. If
it's not, enable it in the 'Preferences' dialogue.

2) Using PsiWin copy the files PYTHIA.OPO, EXPORT.OPO, CABINET.OPO,
PSCAPE.OPO and PSCAPE.MBM to the folder \system\opl (you might have to create
the folder)

3) Go into \System\Apps on C: or D: and make a new folder called "Home"

4) Using PsiWin, copy the following files to the new \System\Apps\Home folder:
  Home.app
  Home.aif
  Home.mbm
  Home.hlp

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
 2) You get free support via e-mail (Pythia@Palmscape.com)
 3) Your suggestions to new functionalities will be implemented (as far as
resources permits it)

Pricing:

Collection Suite programs are registered using either individual codes or a
Collection Gold Code. This code is valid for all present and future
Collection Suite programs. The Collection Gold Code costs USD 30.

How to register

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

1999.02.20: Version 2.5
 - The Home Collection started on version 2.5 to synchronize with the other
Collection Suite applications.

1999.03.28: Version 2.5.1
 - Geofox enabled: Screen no longer reacts when the mouse passes over a field.

1999.06.09: Version 2.5.2
 - To avoid a OPL bug the where clause is now cleared when you create a new
record.

1999.06.29: Version 2.6
 - The application has been updated to work on both the Psion 5 and 5mx.

1999.08.23: Version 2.6.1
 - Jump directly from the current recording to the edit tape dialog with
Ctrl-B.

2000.03.28: Version 2.7
 - Released as a PocketIQ product.
 - Ctrl-B (View Tapes) creates a new tape if a matching one does not already
exist.

2000.04.28: Version 2.7.1
 - Install error fixed.

2000.05.28: Version 2.7.2
 - Toolbar problem fixed.