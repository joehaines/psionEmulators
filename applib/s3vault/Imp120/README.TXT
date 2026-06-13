DataImp - Readme file.

This ZIP file contains files for all the current DataImp modules.  If you are upgrading then please ensure all the modules are from the same build version!

FIles that are included are:

Dataimp
-------
DATAIMP.OPA    Main controlling program - required for all modules
DATAIMP.HLP    Main help - optional but highly recommended
JBSORTB.IMG    Sort procedure - the fastest there is!

Video
-----
VIDEO.OVL      Video module
VIDEO.HLP      Help for Video module - optional but highly recommended

Photo
-----
PHOTO.OVL      Photo module
PHOTO.HLP      Help for Photo module - optional but highly recommended

Music
-----
MUSIC.OVL      Music module
MUSIC.HLP      Help for Music module - optional but highly recommended

Installation
------------
If you are upgrading from versions 1.19 or below then please copy:
    IMP_UPGR.OPO to \OPO\

To \APP\
    copy JBSORTB.IMG AND DATAIMP.OPA.
To \APP\DATAIMP\
    copy DATAIMP.HLP, *.OVL, and *.HLP for the relevant modules.

You may install DATAIMP.OPA to the \APP\DATAIMP\ directory if you wish.

These must all be on the same drive.

Start up
--------

When you first run a module you will be asked which drive you wish to store your data files on.  This should always be a RAM drive (i.e not a FLASH SSD).  This data is stored in \OPD\DATAIMP.DRV so if you wish to move your data to another drive then you must delete this file and when you restart DATAIMP it will ask you for the new location of your data.

Now read DATAIMP.TXT and the relevant TXT file for your module.