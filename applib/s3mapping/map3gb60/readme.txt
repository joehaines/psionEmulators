Psi-Mapper/GB 6.0 (C) UK 1994-1998 Steve Litchfield
=======================================================
Released as shareware September 1998 ...

Note to upgraders: If you already have v5.0 installed, there may not be a
huge amount of benefit in wiping it and replacing with v6.0. See the 'changes'
section below! The purpose of v6.0 is largely to provide a neater way for
beginners to get up and working, by simplifying the files distributed.

***************
If you unzipped this product from its constituent ZIP file(s) _without_
the "-d" or "Include folders/directories" option (to rebuild the original
correct directory structure), then delete this file and the other files
you've just created and try again!

If you use a Mac, please note that you should avoid any unzipping
options that involve 'smart'-handling of linefeeds.
***************

Registration address (send cheque for 14 pounds to):

Steve Litchfield
22 Grays Crescent
Woodley
Berks
RG5 3EN

or register electronically with a credit card at
http://www.swregnet.com/758p.htm

Psi-Mapper update information and new versions can be found at
http://3lib.ukonline.co.uk/

Requires Psion Series 3a/3c/3mx, with approximately
100k of free system memory to run in.

Psi-Mapper/GB 6.0 is shareware. This does not make it a free program!
Yes, you can copy it freely to friends or acquaintances, or even upload
the original ZIP file to ftp sites, bulletin boards, but if you like it
and continue to use it, then you are honour-bound to register the program
by sending me a cheque for 14 pounds.

Registration gets you:
a) a clear conscience that you're using paid-for software, and that
   you're helping support future development of Psi-Mapper/GB and other
   quality Psion shareware software.
b) a special code that will personalise your copy of Psi-Mapper/GB,
   remove the opening shareware screen and let you access the program
   more conveniently by shrinking or removing the status window to give
   more room on-screen. No need to re-install the software, just use the
   registration menu option and type in the code!

********************    IMPORTANT!!    ********************************
This text file documents the history of the program, its installation,
and some of the more technical aspects to running it. For general help in
using and understanding Psi-Mapper/GB, please browse through the *extensive*
on-line help. The on-line help is the only documentation of most of
Psi-Mapper/GB's features, so work through it properly! 8-)
***********************************************************************

Quick summary of changes for v6.0 (since 5.0)
---------------------------------------------------------------------
Some small fixes to the towns file; added feature to add items to overlays
when within GPS moving-map mode; all town plan files now integrated into
a single database (speeds backups and saves space on SSDs);
all modules now included in a single ZIP distribution for easier
installation by beginners; major update to the Travel Inn overlay;
other new overlays, including Windsurfing centres; custom town plan
feature removed (you now have to stick to the 280 I supply);
---------------------------------------------------------------------

This is Psi-Mapper/GB, released as shareware for the Psion 3a/3c/3mx.
There is also a cut-down Siena version, currently at v1.1, using the
same registration codes (so you can use either once registered etc)

All rights reserved, and copyrights, including the database files
and their format, where appropriate (see listings below).

All data sets are original work of the various contributors, collated using
information from low resolution free maps, genuine public domain
information, data gathered from personally-owned GPS units, club/hobbyist
databases, and RAC and AA member information packs.

****Please note****
                   that the displayed graphics, coastlines, boundaries,
railways and road vectors are shown diagrammatically, for ease of
understanding. Please especially note Psi-Mapper's low resolution.
*Everything* is at *least* +/-1km both horizontally and vertically! Using
low resolution means
(i) that the program data can be built from public
domain information
(ii) that Psi-Mapper won't conflict or overlap with existing PC-based GIS
systems or infringe map copyrights
(iii) (most importantly) that quite large areas and amounts of
information can be crammed into a palmtop, where a PC version would need
megabytes!!

Components and Installation.
----------------------------
Psi-Mapper is almost certainly the largest and most complex program you'll
ever have to install on your Psion, so don't get frustrated if you can't
seem to work out how to get everything going immediately. Try installing
another (simpler) program first, and then try Psi-Mapper again when you're
more confident with files, directories etc.

If you don't use PsiWin on a PC:
  Assuming you *did* use -d with your unzipping tool and now have all the files
  in their original directory structure; with the link on between the
  Psion and the desk-top machine, go to the Psion keyboard. Position the
  cursor on the system screen on Time and press Psion-C. The 'Copy Files'
  dialog will appear with the root directory selected. Change the second
  line to point to the appropriate directory on the desk-top. Change the
  third line to be "\", i.e. a single backslash. Change the fourth line
  to be the disk you want to install Psi-Mapper onto. Set the fifth line
  ("include subdirectories") to read "Yes". Your Psion will then copy
  over the directory tree in one go to the appropriate disk.

If you use PsiWin:
  (i) If you correctly unzipped Psi-Mapper/GB with its orginal directory
      structure, then simply drag and drop this directory tree onto one of
      your Psion's disk root directories. Note: dragging and dropping
      whole directory trees so that they end up in the right place is not
      trivial and you may wish to read the manual and/or practice first!!

  TIP: - when you drag and drop, remember to hold the <Control> key down
  throughout the operation. This forces PsiWin to just copy the file(s)
  and not try to do any fancy conversions!

  (ii) If you've got all the Psi-Mapper files in the same directory, you're
      definitely going to have to do it all manually, but at least you can
      do it all with 'drag and drop'! You'll need to create the \APP\ and
      \APP\MAPPERGB\ directories by hand on your Psion using the
      'Create Directory' menu command. A good tip is to sort the
      unzipped files by extension/type, i.e. all the .PICs
      together, all the .MP3s together, and so on. You can highlight
      blocks of files in PsiWin by clicking on the first one and then
      holding the <Shift> key down and clicking on the last one. ALL files
      in between will now be highlighted as well, and you can drag and
      drop the lot in one go across to your Psion's drives!! If all else
      fails refer to the list below!

This distribution comprises the following files. Contributor
information also included where known or appropriate.

(Note the files marked with -* are ESSENTIAL and MUST be included for the
program to run at all!)

README.TXT - this file!

MAPPERGB.OPA - the main program module. Place this in an \APP\ directory
               on any Psion drive, and install onto your system screen
               with Psion-I. -*

plus support files, to be placed in a \APP\MAPPERGB\
subdirectory on the same disk:

MAPHELP.RSC  - the comprehensive on-line help file -*
USEFUL.RSC   - a help file of some useful numbers
GB.MP1       - a data-set of approximately 1100 towns and cities,
               their post and STD codes -* (updated March 1997)
COASTLIN.MP2 - a data-set showing the coastline of GB -* (updated
               by David Brown, November 1996)
ROADS.MP2    - a data-set of the GB motorway and most of the primary route
               network -* (updated September 1998)
MAPPER.PIC   - some icons for use with the overlays, including a default
               one, and the town plan icons etc. -*

PLANS.DAT & PLANS.IDX - Town plan schematic database and index (280 towns)
                        A text listing of them is in PLANS.TXT

AIRFIELD.MP3 - airfields and airports
AIRMUSN.MP3  - air museums (submited by Mark Avey)
BIRDS.MP3    - Royal Society for the Protection of Birds and the 
               Wildlife and Wetlands Trust's nature reserves, together with
               a few selected other bird-watching sites (submitted by Craig Slawson)
CADW.MP3     - Welsh heritage sites (submitted by Peter Buxton, 1997)
CAMPSITE.MP3 - recommended camp sites (contributed by Hedley Wright)
CARAVANC.MP3 - Caravan Club sites (contributed by Hedley Wright)
CASTLES.MP3  - castles (contrbuted by Mark Avey)
CATHDRAL.MP3 - Anglican cathedrals in England and Wales, together with
               the ancient Scottish cathedrals (submitted by Craig Slawson)
DAYSOUT.MP3  - days out (theme parks, attractions etc) (contributed
               by Mark Avey, updated November 1996)
ENGHER.MP3   - English Heritage sites (submitted by Tim Alton)
ENGPEAKS.MP3 - All hill or mountain peaks in England (contributed by
               Hedley Wright)
EVENTS.MP3   - Annual events - (submitted by David Brown)
EZ_VIEW.MP3  - Easy to get to breathtaking views - (submitted by
               David Brown)
FOOTBALL.MP3 - league football grounds  (contributed by Trevor Ridley)
HISTSCOT.MP3 - Historic Scotland sites (contributed by A. Thomson)
HMCG.MP3     - UK Coastguard Stations and the National HQ (submitted by Graeme J W Smith)
ISLANDS.MP3  - The centred locations of the larger islands around the coast
               of England, Scotland and Wales (submitted by Craig Slawson)
JUNCTION.MP3 - Motorway junctions
LANDRNGR.MP3 and .MP4 - item and vector overlays detailing the
               OS LandRanger series of maps (submitted by Simon Pooley
               and Craig Slawson)
LIFEBOAT.MP3 - Lifeboat stations (submitted by Ian Henry, 1996)
MAZES.MP3    - Mazes (what else?)
MRACING.MP3  - motor racing circuits
NATTRUST.MP3 - National Trust Properties (contributed by Pat McMurray)
OSGRID.MP4   - a vector format data-set of the National Grid Squares
               (contributed by Simon Pooley)
PIZZAEXP.MP3 - Pizza Express restaurants (contributed by W Croaker)
PORTS.MP3    - ports which can accommodate yachts of up to 20m (70ft) loa,
               5m (16ft) BMax, 2.6m (9ft) Draft, and 25m (80ft) Air Draft
               (mast height) at some state of the tide in moderate weather 
               (submitted by Graeme J W Smith)
RACECRSE.MP3 - race courses
RAILWAYS.MP4 - a vector format data-set of the GB railway network
               (excluding some local or irregular services)
RIVERS.MP4   - a vector format data-set of rivers (contributed 1996
               by Tony Bartels and David Brown)
ROMAN.MP3    - ancient roman remains (submitted, I think, by Craig Slawson)
SCOPEAKS.MP3 - All hill or mountain peaks in Scotland (contributed by
               Hedley Wright)
SERVICES.MP3 - Motorway service stations (updated July 96)
SKYDIVE.MP3  - Sky Diving centres
STEAM.MP3    - Steam railways (submitted by Chris Partis)
TOURINFO.MP3 - Tourist Information Centres (submitted by Paddy Dillon)
TRAVELOD.MP3 - Forte Travelodges (updated June 1996)
TRAVINN.MP3  - Travel Inns (updated June 1998)
TVMAINTX.MP3 - TV transmitters (contributed by Simon Pooley)
VULCAN.MP3   - surviving vulcan bombers!
WATER.MP3    - The centred locations of the majority of bays, lakes, lochs,
               sea channels and natural harbours (submitted by Craig Slawson)
WELPEAKS.MP3 - All hill or mountain peaks in Wales (contributed by
               Hedley Wright)
WILDLIFE.MP3 - County Wildlife Trusts (submitted by Craig Slawson)
WINDMILL.MP3 - windmills throughout England, Scotland and Wales (submitted by Craig Slawson)
WINDSURF.MP3 - Royal Yachting Assoc Windsurfing Centres (submitted by A. Campbell)
WOODLAND.MP3 - Woodland Trust sites (submitted by Tim Alton)
YHA.MP3      - Youth Hostels (contributed by Darren McMinn)
ZOOPARK.MP3  - Zoological parks (submitted by David Gouge, 1996)


Each of the .MP3 overlays listed has an associated .PIC file of
the same name. These are the on-screen icons for overlays, and will be
used if present in the main \APP\MAPPERGB\ directory. If they are not
present, a default 'star' icon will be used .... 

Each of the overlays usually includes reference information for each item
(for example, telephone contact numbers), and this can be displayed by
turning 'Overlay Reference Fields' On.


GPS owners
----------
If you own a GPS unit (Global Positioning Service), you can hook it up
to your Psion running Psi-Mapper to provide a moving-map display. See the
file GPSNOTES.TXT for more details!


.MP3 Overlays and their format etc.
-----------------------------------
There are two ways to create your own .MP3 overlays, you can choose
the method that works best for you:

(i)   Use the overlay creation menu options to create an overlay and
then use the point and shoot method to identify where you want each item
icon to appear, together with information for the reference field. This
method can obviously be time-consuming if you have a lot of items to
enter, but is friendly and easy to use!
(ii)   If you have a lot of items in your overlay and you know
their OS northings and eastings (as numbers from 0 to 1000, so you may
have to divide your figures by 10, 100 or 1000...), you can make the MP3
file yourself by preparing (for example) a CSV text file and then
importing into DATA. The format is:
    Standard DATA file, named xxxxxxxx.MP3 in \APP\MAPPERGB\
    4 text fields: Name, Eastings, Northings, Reference Info
    The first record is a header record and should contain
    (What the reference fields represent)
    (What the name fields represent)
    (Blank)
    (Blank)
    The second record and following records contain
    (Name)
    (Eastings)
    (northings)
    (Reference info)
If this is not clear to you, then please get in touch and I can arrange
to help you construct your overlay.

Please note that although these user-made .MP3 files are editable under
DATA, you will run into a few problems if you update the records in the
file on a *flash disk*, as the file *must be compressed* for Psi-Mapper/GB
to use it, and the Psion won't let you compress a file on a flash disk!
Thus you'll have to copy the file onto your internal or RAM disk, make the
changes, compress the file and then copy it back over into the
\APP\MAPPERGB\ directory on your flash.

Psi-Mapper/GB *should* default to creating new .MP3 files on your *internal*
disk, which is where you'll have less problems updating your overlay.

Note that some of the built-in/supplied overlays have been compressed
slightly to save space and so are not editable under DATA. If you're
desperate to see what's in them, the freeware program JBDATA will let you
do this.

You will probably want to create a custom icon as well. In this case,
Psi-Mapper/GB needs a 17 by 17 single-plane PIC file of the same filename as
the .MP3 overlay, but with a .PIC extension. I normally create these icons
by starting with a 48x48 blank icon using DRAW 2.85 (see appendix notes),
drawing my icon in the top-left corner, and then using 'Draw/Dimensions'
to create a 17x17 PIC. Save and exit.



.MP4 overlays and their format
-------------------------------
These vector overlays are non-trivial to create, and are best only
tackled if you know what you're doing. For the record, here is a quick
summary:

The MP4 format is simply groups of 5 eastings/northings pairs per 
record. The 1st/last duplicate/line-up. All ferries/railways etc thus 
are fitted into block of 5 coordinate pairs. The vector *name* is 
printed if required to the top right of the third coordinate pair,
normally, or to the bottom left if you flag this to Psi-Mapper/GB by setting
the eastings coordinate of the *first* point in a record to be negative.

Usual Psion OPL database format. First field is $ format, the numbers 
are all % format. All numbers in 1000s of eastings (i.e. 1km resolution).

e.g. Record 1:

Basingstoke              
E1:110               
N1:234                   
E2:114
N2:227
E3:118
N3:220
E4:125
N4:210
E5:132
N5:200

Record 2:

Basingstoke
E1:132
N1:200

 etc etc
 
Deleting info from the custom town database, \OPD\MAPPER.DBF
------------------------------------------------------------
The \OPD\MAPPER.DBF file is editable under DATA, so you can delete
individual towns etc and modify them in this way. Please remember to
compress the file before exiting DATA !


Keeping a standard/default settings file
----------------------------------------
Note that because Psi-Mapper/GB can cope with read-only settings files,
you can set everything up how *you* like it (i.e. your own private
default for all settings) and then save it under a chosen name and make
it read-only. You can then start Psi-Mapper/GB with your preferred
settings and it will be prevented from overwriting them when you exit the
program.


Grabbing the current map screen as a PIC file
---------------------------------------------
You may well want to grab a map screen as a standard Psion .PIC file, for
printing or editing using DRAW or any other Psion bit-map editor.
Grabbing the Psion screen is done with pressing 'Control', 'Shift',
'Psion' and 'S' simultaneously (it's easier to do than it sounds) and lo
and behold a file called SCREEN.PIC will have appeared in the root
directory (\) of your internal disk ...

_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_

Thanks for trying Psi-Mapper/GB! If you decide to remove Psi-Mapper/GB from
your Psion, please note that it creates several small files in \OPD\ on
your default disk. MAPPER.DBF (custom place database), MAPPER.REG (your
registration key, once I've given it to you) and assorted .MSF files
(used to store your preferences and position etc from session to
session). If you've created any custom overlays or their icons, there may
also be a few files in an \APP\MAPPERGB\ directory on your internal disk.

Small print:
------------
1) Reverse engineering of the program or data modules is strictly
   forbidden. If you would like to create a complementary program and use
   my data-sets, then please get in touch with me.

2) To speed up registrations, you can quote your email
   address for quick return of your registration code, or use the
   RegNet credit card scheme.

3) On-line support for Psi-Mapper/GB can be found on the World-Wide-Web
   at http://3lib.ukonline.co.uk/

4) If you give MAPPER to a friend (as is encouraged with shareware!),
   then it's always best to give him/her the original ZIP files. If the
   transfer takes place by SSD, then remember to include both the main
   MAPPERGB.OPA program and the \APP\MAPPERGB\ subdirectory, full of data files.
   Do *not* give your friend your MAPPER.REG registration file, as this is
   unique to you and contains *your* name, and subsequent piracy will tell
   the world who started the outbreak!

5) Thanks to all the testers and contributors out there, who have
   supplied ideas, support, bug-reports, data, code fragments etc. You
   are too numerous to mention! If *you* find a bug or spot an error etc
   in any of the overlays, then please get in touch!

6) Note that the Northings/Eastings and National Grid coordinate systems
   are used with the kind written permission of Ordnance Survey.

7) Note on copyrights etc.: be careful to only make use of publicly
   available and public domain data sources when compiling any overlays
   or town/item plans, unless of course you sourced the information (from
   memory?) in the first place! If you submit an overlay to myself for
   inclusion in future versions of Psi-Mapper/GB, please state your
   source(s) of information and their copyright status.

8) Disclaimer: I accept no responsibility for anything happening to you
    as a result of inaccurate information within Psi-Mapper! I try to
    keep the overlays up-to-date as possible, but that is as far as my
    liability goes!

9) Do not use this application while on the move! If you crash your car
    while looking at your Psion, don't blame me!!

Steve Litchfield
e-mail addresses:
slitchfield@cix.compulink.co.uk
slitchfield@ukonline.co.uk