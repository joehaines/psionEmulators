Psi-Mapper/Ireland 2.2 for EPOC (Psion Series 5 etc)
=====================================================

Released as freeware May 1999 ...

A mapping information system for Ireland. Read on...

Psi-Mapper update information and new versions can be found at
http://3lib.ukonline.co.uk/

Requires Psion Series 5 with approximately 200k of free system memory
to run in.

All data sets are original work of the various contributors, collated using
information from low resolution free maps, genuine public domain
information, data gathered from personally-owned GPS units, club/hobbyist
databases.

****Please note**** that the displayed graphics, coastlines, boundaries,
railways and road vectors are shown diagrammatically, for ease of
understanding. Please especially note Psi-Mapper's low resolution.
Everything is at least +/-1km both horizontally and vertically!

Using low resolution means
(i) that the program data can be built from public domain information

(ii) that Psi-Mapper won't conflict or overlap with existing PC-based
GIS systems or infringe map copyrights

(iii) (most importantly) that quite large areas and amounts of
information can be crammed into a palmtop, where a PC version would
need megabytes!!

Components and Installation.
----------------------------
If you unzipped Psi-Mapper/Ireland OK, you should have ended up with 2
separate files. One will be this text file 8-)

The other is an SIS file.

(Experts)
If you know what a .SIS file is, just double-click on it in Win95
Explorer or tap on it on the Series 5 and you'll be installed in no
time.

(Beginners)
If you *don't* know what a .SIS file is, this must be one of the very
first third party applications you've ever installed. Don't worry,
it's all quite painless:

  IF you have the 'older' PsiWin 2.01 or if you've downloaded
  Psi-Mapper/Ireland directly onto the Psion 5, the best way to proceed is
  to copy the "mapperir.sis" file into a suitable place on your Psion.
  If the file appears on the system screen with a question mark beside
  it, this just means you haven't yet upgraded your Series 5 to accept
  EPOC install files. Grab a copy of the special INSTEXE.EXE program
  from the same place as you obtained Psi-Mapper/Ireland and tap on it on
  your Psion. You should now see a nice little icon beside the
  "mappergb.sis" file and can tap on it to complete your installation.

  IF you have PsiWin 2.1, just double-click on the "mapperir.sis" file
  to launch EPOC Install on the PC itself and to complete the
  installation.


If all went well, you should now have a new Psi-Mapper/Ireland icon on your
Extras bar.

---------------------------------
Files installed onto your Series 5 or compatible:
---------------------------------

Mapperir.app - the main program module
Mapperir.aif - the application icon file

plus some support files:

Mapihelp.hlp  - the comprehensive on-line help file

Towns.mp1   - a data-set of towns and cities
Coastlin.mp2 - a data-set showing the coastline of Ireland
Roads.mp2    - a data-set of the Irish motorway and most of the primary route
               network

(the above three files were updated and compiled by Eddie Slupski,
summer 1997)

Mapper.mbm - some icons for use with the overlays, including a default
               one, the town planning symbols and the title screen

Vector overlays (i.e. lines and boundaries):
National.mp4 - the boundary between Northern and Southern Ireland
Railways.mp4 - a vector data-set of the Irish railway network
Osgrid.mp4   - a vector data-set of the Irish Grid Reference system
Fifty.mp4    - a vector data-set of the Irish 1:50000 map index grid

Item overlays (i.e. specific points and labels and reference info):
Airports.mp3 - airports! (updated by Eddie Slupski, 1997)
Interest.mp3 - places of interest (updated by Zoe Ingle, 1997)
Camping.mp3  - camp sites
Peaks.mp3    - mountain peaks over 700m (compiled by Zoe Ingle, 1996)
Portsire.mp3 - ports capable of taking yachts (contributed by Graeme Smith)
HMCGIre.mp3  - coastguard stations (contributed by Graeme Smith)
CoastRad.mp3 - coastguard radio stations (contributed by Graeme Smith)
Coast2c.mp3  - coast to coast walk - (contr. by Paddy Dillon, July 96)
Ulstrway.mp3 - Ulster Way walk - (contr. by Paddy Dillon, July 96)
Walkfest.mp3 - Some walking festivals - (contr. by Paddy DIllon, July 96)
YHA.mp3      - Youth Hostels - (contr. by Paddy Dillon, July 96)
Forest.mp3   - Forest Parks - (contr. by Paddy Dillon, July 96)
NatTrust.mp3 - National Trust sites (Northern Ireland only) -
               (contr. by Paddy Dillon, July 96)
Cathedr.mp3  - Cathedrals, abbeys, ruins etc - (contr. by Zoe Ingle,
               Jul 96)
Viewpont.mp3 - Viewpoints - (contr. by Zoe Ingle, Jul 96)
Museum.mp3   - Museums etc - (contr. by Zoe Ingle, Jul 96)
Tourinfo.mp3 - Tourist Info Centres - (contr. by Paddy Dillon, July 96)

Each of the .mp3 overlays listed above has an associated .MBM file of
the same name. These are the on-screen icons for overlays, and will be
used if present in the main \System\Apps\MapperIR folder. If they are
not present, a default 'star' icon will be used....

Each of the overlays includes reference information for each item, and
this can be displayed by turning 'Overlay Reference Fields' On.


.MP3 Overlays and their format etc.
-----------------------------------
There are three ways to create your own .MP3 overlays, you can choose
the method that works best for you:

(i)   Use the overlay creation menu options to create an overlay and
then use the point and shoot method to identify where you want each item
icon to appear, together with information for the reference field. This
method can obviously be time-consuming if you have a lot of items to
enter, but is friendly and easy to use! Note that once you've created an
overlay with this method you cannot open it in Psion's DATA application.
This is a DATA limitation on the Series 5, unfortunately. Use the
supplied utility CustomEd.OPO to edit these special overlays.

(ii)   If you have a lot of items in your overlay and you know
their OS northings and eastings (as numbers from 0 to 1000, so you may
have to divide your figures by 10, 100 or 1000...), you can make the MP3
file yourself by preparing (for example) a tab-delimited text file and then
importing into DATA. I've supplied a template file (MP3-Temp.lat) which you
can copy and rename as appropriate. The general format is:
    Standard DATA file, named xxxxxxxx.MP3 in \SYSTEM\APPS\MAPPERIR\
    4 fields: Name (text), Eastings (numeric, 0 to 1000),
              Northings (numeric, 0 to 1000), Reference Info (text)

    The first record is a header record and should contain
    (What the reference fields represent)
    0
    0
    (Blank)

    The second record and following records contain
    (Name)
    (Eastings)
    (northings)
    (Reference info)

(iii) Use the supplied CustomEd.opo utility to add to and to edit your
     custom overlay. This is a simple data editor and is still in the
     prototype stage.

You will probably want to create a custom icon as well. In this case,
Psi-Mapper needs a 17 by 17 single-plane .MBM file of the same
filename as the .MP3 overlay, but with a .MBM extension. There are
detailed instructions on how to do this in the on-line Psi-Mapper
help database.


.MP4 overlays and their format
-------------------------------
These vector overlays are non-trivial to create, and are best only
tackled if you know what you're doing. For the record, here is a quick
summary. Note that you can copy and rename the supplied MP4-Temp.lat
template database as a way of getting started. Then use DATA itself to
construct your records.

The MP4 format is simply groups of 5 eastings/northings pairs per 
record. The 1st/last duplicate/line-up. All ferries/railways etc thus 
are fitted into block of 5 coordinate pairs. The vector *name* is 
printed if required to the top right of the third coordinate pair,
normally, or to the bottom left if you flag this to Psi-Mapper by setting
the eastings coordinate of the *first* point in a record to be negative.

First field is text format, all the rest are numeric.
All numbers are in 1000s of eastings (i.e. 1km resolution).

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
 

Deleting info from the custom town database
------------------------------------------------------------
Use the supplied CustomEd.opo utility to maintain and prune your list
of custom towns. Once in the program, use the Menu functions to make
progress.


Grabbing the current map screen as a .MBM file
---------------------------------------------
You may well want to grab a map screen as a standard Psion .MBM file, for
printing or editing using SKETCH or any other Psion bit-map editor.
Grabbing the Psion screen is done with pressing 'Control', 'Shift',
'Function' and 'S' simultaneously (it's easier to do than it sounds).

