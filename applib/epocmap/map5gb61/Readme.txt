Psi-Mapper/GB 6.1 for the Psion Series 5 (C) UK 1995-1998 Steve Litchfield
==========================================================================

Released as freeware May 1999 ...

A mapping information system for Great Britain. Read on...

Psi-Mapper update information and new versions can be found at
http://3lib.ukonline.co.uk/

Requires Psion Series 5 with approximately 200k of free system memory
to run in.

********************    IMPORTANT!!    ********************************
This text file documents the history of the program, its installation,
and some of the more technical aspects to running it. For general help
in using and understanding Psi-Mapper/GB, please browse through the
*extensive* on-line help. The on-line help is the only documentation
of most of Psi-Mapper/GB's features, so work through it properly! 8-)
***********************************************************************

Changes for v6.1 (since v6.0)
---------------------------------------------------------------
STD code changes for phone day for over 100 places; added overlay
of Scottish Munros; Removed all references to shareware(!); a few
very minor typos corrected;
---------------------------------------------------------------

All data sets are original work of the various contributors, collated using
information from low resolution free maps, genuine public domain
information, data gathered from personally-owned GPS units, club/hobbyist
databases, and RAC and AA member information packs. 

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
If you unzipped Psi-Mapper/GB OK, you should have ended up with 2
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
  Psi-Mapper/GB directly onto the Psion 5, the best way to proceed is
  to copy the "mappergb.sis" file into a suitable place on your Psion.
  If the file appears on the system screen with a question mark beside
  it, this just means you haven't yet upgraded your Series 5 to accept
  EPOC install files. Grab a copy of the special INSTEXE.EXE program
  from the same place as you obtained Psi-Mapper/GB and tap on it on
  your Psion. You should now see a nice little icon beside the
  "mappergb.sis" file and can tap on it to complete your installation.

  IF you have PsiWin 2.1, just double-click on the "mappergb.sis" file
  to launch EPOC Install on the PC itself and to complete the
  installation.


If all went well, you should now have a new Psi-Mapper/GB icon on your
Extras bar.

---------------------------------

In this distribution are the following overlays, Contributor information
is included where known or appropriate.

TOWNS.MP1    - a data-set of approximately 1100 towns and cities,
               their post and STD codes - (updated April 1999)
COASTLIN.MP2 - a data-set showing the coastline of GB - (updated
               by David Brown, November 1996)
ROADS.MP2    - a data-set of the GB motorway and most of the primary route
               network - (updated March 97)

Vector overlays (i.e. lines and labels):
RIVERS.MP4   - a vector format data-set of rivers (contributed 1996
               by Tony Bartels and David Brown)
RAILWAYS.MP4 - a vector format data-set of the GB railway network
               (excluding some local or irregular services)
COUNTIES.MP4 - a vector format data-set of the GB county and admin
               boundaries (contributed by Mark Avey, updated 1996)
FERRIES.MP4  - a vector format data-set of the external GB ferry routes
OSGRID.MP4   - a vector format data-set of the National Grid Squares
               (contributed by Simon Pooley)

Item overlays (i.e. specific points and labels and reference info):
JUNCTION.MP3 - Motorway junctions
SERVICES.MP3 - Motorway service stations (updated July 96)
PIZZAEXP.MP3 - Pizza Express restaurants (contributed by W Croaker)
TRAVINN.MP3  - Travel Inns (updated June 1998)
TRAVELOD.MP3 - Forte Travelodges (updated September 1997)
RACECRSE.MP3 - race courses
AIRFIELD.MP3 - airfields/airports
BBCRADIO.MP3 - BBC local radio stations
MRACING.MP3  - motor racing circuits
CASTLES.MP3  - castles  (contrbuted by Mark Avey)
DAYSOUT.MP3  - days out (theme parks, attractions etc) (contributed
               by Mark Avey, updated November 1996)
NATTRUST.MP3 - National Trust Properties (contributed by Pat McMurray)
HISTSCOT.MP3 - Historic Scotland sites (contributed by A. Thomson)
WOODLAND.MP3 - Woodland Trust sites (submitted by Tim Alton)
ENGHER.MP3   - English Heritage sites (submitted by Tim Alton)
CADW.MP3     - Welsh heritage sites (submitted by Peter Buxton, 1997)
LIFEBOAT.MP3 - Lifeboat stations (submitted by Ian Henry, 1996)
EZ_VIEW.MP3  - Easy to get to breathtaking views - (submitted by
               David Brown)
WELPEAKS.MP3 - Hill peaks across Wales
SCOPEAKS.MP3 - Hill peaks across Scotland
ENGPEAKS.MP3 - Hill peaks across England
MUNSCOT.MP3  - Scottish 'Munros'

Each of the overlays usually includes reference information for each item
(for example, telephone contact numbers), and this can be displayed by
turning 'Overlay Reference Fields' On.


Deleting info from the custom town database
------------------------------------------------------------
Use the supplied CustomEd.opo utility to maintain and prune your list
of custom towns. Once in the program, use the Menu functions to make
progress.

_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_

Thanks for trying Psi-Mapper/GB!

Thanks to all the testers and contributors out there, who have
supplied ideas, support, bug-reports, data, code fragments etc. You
are too numerous to mention! If *you* find a bug or spot an error etc
in any of the overlays, then please get in touch!

Note that the Northings/Eastings and National Grid coordinate systems
are used with the kind written permission of Ordnance Survey.
The road data is "Based upon the Ordnance Survey map with the
permission of The Controller of Her Majesty's Stationery Office, (C)
Crown Copyright MC 0055"

Disclaimer: I accept no responsibility for anything happening to you
as a result of inaccurate information within Psi-Mapper! I try to
keep the overlays up-to-date as possible, but that is as far as my
liability goes! Do not use this application while on the move!
If you crash your car while looking at your Psion, don't blame me!!

Steve Litchfield
----------------
e-mail addresses:
slitchfield@cix.co.uk
slitchfield@ukonline.co.uk
