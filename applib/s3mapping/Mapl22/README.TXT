Psi-Mapper/London 2.2 (C) UK 1997 Steve Litchfield
==================================================

Released September 8 1997

***************
If you unzipped this product from its constituent ZIP file(s) _without_
the "-d" or "Include folders/directories" option (to rebuild the original
correct directory structure), then delete this file and the other files
you've just created and try again!

If you use a Mac, please note that you should avoid any unzipping
options that involve 'smart'-handling of linefeeds.
***************

Registration fee: œ14

The program is fully working, with all data sets and functions etc, as
I trust you to do the honest thing and to "play fair with shareware".
As a reminder to do so, the status window cannot be shrunk until you
register, ensuring that "Unregistered" stares you in the face(!) while
using the program! 8-))

Registration address (send UK cheque or Eurocheque in UK sterling or
equivalent in well-wrapped dollars/francs/DM to) :

Steve Litchfield
22 Grays Crescent
Woodley
Reading, Berks
RG5 3EN

or register on Compuserve via GO SWREG, using id. 7035

or register on the net with a credit card using secure transaction
technology at http://www.swregnet.com/620p.htm

For future updates of this program, please download from
http://web.ukonline.co.uk/slitchfield/maplon.htm

Psi-Mapper/London requires a Psion Series 3a/3c or Acorn PocketBook
II, with approximately 100k of free system memory to run in.

=======================================================================

Psi-Mapper/London is shareware. This does not make it a free program! Yes,
you can copy it freely to friends or acquaintances, or even upload the
original ZIP file to bulletin boards etc, but if you like it and
continue to use it, then you are honour-bound to register.

Registration gets you:

a) a clear conscience that you're using paid-for software, and that
   you're helping support future development of the MAPPER program
   family and other quality Psion shareware software.

b) a special code that will personalise your copy of Psi-Mapper/London and
   let you shrink or remove the status window to give more room on-screen.
   No need to re-install the software, just use the registration menu
   option and type in the code!

c) A direct say in how the program develops.

********************    IMPORTANT!!    ********************************
This text file documents the history of the program, its installation,
and some of the more technical aspects to running it. For general help
in using and understanding Psi-Mapper/London, please browse through the
*extensive* on-line help. This help is the only documentation of most of
Psi-Mapper/London's features, so work through it properly! 8-)

----------------------------------------------------------------------
This is Psi-Mapper/London, released as shareware. The .OPA program
file is (C) 1997 Steve Litchfield. With the exception of the road
data, all data sets are public domain and were collated using public
domain and other freely available reference works, and data gathered
from privately-owned GPS units. Data sets remain the responsibility of
the original contributors. No liability whatsoever can be accepted for
any inaccuracies in this program or any of its data sets.

The road data was "based upon the Ordnance Survey map with the
permission of The Controller of Her Majesty's Stationery Office, (C)
Crown Copyright, MC 0055".

If you'd like to contribute an item or vector data set, then go right
ahead, subject to the restrictions on copyright mentioned at the end of
this text file.

Components and Installation.
----------------------------
Psi-Mapper/London may be one of the largest and most complex programs
you'll ever have to install on your Psion, so don't get frustrated if
you can't seem to work out how to get everything going immediately.
You're on a learning curve, so don't give up too soon!

NOTE: Before installing Psi-Mapper/London, do the following. Go into
'Set Preferences' on your system screen menu and set 'Update lists' to
"System Button". If you don't do this, the 3a/3c will try and update all
its lists after *every* file is copied, which will take ages ....

If you don't use PsiWin on a PC:
  Assuming you *did* use -d with PKUNZIP etc. and now have all the files
  in their original directory structure; with the link on between the
  Psion and the desk-top machine, go to the Psion keyboard. Position the
  cursor on the system screen on Time and press Psion-C. The 'Copy Files'
  dialog will appear with the root directory selected. Change the second
  line to point to the appropriate directory on the desk-top. Change the
  third line to be "\", i.e. a single backslash. Change the fourth line to
  be the disk you want to install Psi-Mapper onto. Set the fifth line
  ("include subdirectories") to read "Yes". Your Psion will then copy over
  the directory tree in one go to the appropriate disk.

If you use PsiWin:
  (i) If you correctly unzipped Psi-Mapper/London with its orginal
      directory structure, then simply drag and drop this directory tree
      onto one of your Psion's disk root directories. Note: dragging and
      dropping whole directory trees so that they end up in the right
      place is not trivial and you may wish to read the manual and/or
      practice first!!

  (ii) If you've got all the Psi-Mapper/London files in the same
      directory, you're definitely going to have to do it all manually,
      but at least you can do it all with 'drag and drop'! You'll need to
      create the \APP\ and \APP\MAPPERLO\ directories by hand on your
      Psion using the 'Create Directory' menu command. A good tip is to
      sort the unzipped files by extension/type, i.e. all the .PICs
      together, all the .MP7s together, and so on. You can highlight
      blocks of files in PsiWin by clicking on the first one and then
      holding the <Shift> key down and clicking on the last one. ALl files
      in between will now be highlighted as well, and you can drag and
      drop the lot in one go across to your Psion's drives!! If all else
      fails refer either to the list below, which should give you enough
      clues as to where things go!

  TIP: - when you drag and drop, remember to hold the <Control> key down
  throughout the operation. This forces PsiWin to just copy the file(s)
  and not try to do any fancy conversions!


{Inside the ZIP are the following files, file sizes quoted to help you
 decide on whether to install certain overlays etc. Contributor
 information also included where known or appropriate}

README.TXT - this file!

MAPPERLO.OPA - the main program module. Place this in an \APP\ directory
               on any Psion drive, and install onto your system screen
               with Psion-I. Size - 52k

plus essential support files (thanks especially to Eddie Slupski for
all the updates!), to be placed in a \APP\MAPPERLO\ subdirectory on
the same disk:

MAPLHELP.RSC  - the comprehensive on-line help file - 12k
LONDON.MP5    - a data-set of approximately 900 places - 18k
COASTLON.MP6  - a data-set showing the Thames estuary and a few
                reservoirs & lakes - 2k
ROADS.MP5     - a data-set of the London road network - 38k
MAPPERLO.PIC  - essential small bitmaps - 3k

plus extra overlays, which you can install as you wish; they all
go into the aforementioned \APP\MAPPERLO\ directory ...

Vector overlays (i.e. lines and labels) (Thanks to Tony Crocker for
contributing these)

RAILTRAK.MP8  - a vector overlay of the main-line railway routes - 5k
TUBELINE.MP8  - a vector overlay of all the tube lines, laid out
                properly (i.e. geographically, not diagrammatically) - 3k
TUBEZONE.MP8  - a vector overlay of the tube charge zones - 1k

Optional item overlays (i.e. specific points and labels and reference
info, note that some overlays only cover the central portion of
Greater London, roughly the area covered by a typical A-Z guide. The
other overlays cover the whole Psi-Mapper/London region):

JUNCTION.MP7  - motorway junctions - 2k
MAINLINE.MP7  - main-line railway routes - 6k - (contributed by Tony Crocker)
TUBE.MP7      - tube (underground) stations, with their
                line name(s) shown as abbreviations - 8k - (updated
                1997 by Eddie Slupski)
PLACES.MP7    - places of interest, with their phone
                numbers - 2k
HOTELS.MP7    - a large selection of hotels across London - 6k
NCP_PARK.MP7  - NCP car parks in London - 3k
POLICE.MP7    - Police stations - 8k - (contributed by Peter Osmond)
EMBASSIE.MP7  - Embassies - 3k  - (contributed by Eddie Slupski)
PIZZA.MP7     - selected Pizza restaurants across London - 4k
KENTUCKY.MP7  - Kentucky Fried Chicken restaurants - 3k (contributed by
                someone, but I can't remember who!)
HOSPITAL.MP7  - hospitals - 4k - (contributed by Eddie Slupski)
SPORTCEN.MP7  - sports centres - 14k - (contributed by Eddie Slupski)
GOLF.MP7      - golf courses across London - 3k
FOOTBALL.MP7  - main football grounds - 1k
MARKETS.MP7   - markets, with their optimum visiting times - 2k
                (contributed by Tony Crocker)
THEATRES.MP7  - theatres across London - 3k (contributed by
                Tony Crocker & Gordon Drayson)
PETROL.MP7    - filling stations - 6k- (contributed by Gordon Drayson)
BIGMACS.MP7   - some MacDonalds restaurants - 3k - (contributed by Tony
                Crocker)

Notes:
1) Each of the .MP7 overlays listed above has an associated .PIC file of
the same name. These are the on-screen icons for overlays, and will be
used if present in the main \APP\MAPPERLO\ directory. If they are not
present, a default 'star' icon will be used ....

2) Some of the overlays may also have text files (*.TXT) and/or database
files (*.DBF), depending on whether the original compiler of the overlay
supplied one. The text files can be read as usual, and describe the
overlay in more detail. The database files are for opening in DATA and
then linking into via Psi-Mapper's menu options.

3) Each of the overlays includes reference information for each item (for
example, telephone contact numbers), and this can be displayed by
turning 'Overlay Reference Fields' On.

4) Whether using the supplied vector overlays or contributing your own,
please note the low (100m) resolution used by Psi-Mapper/London. It's
no good expecting or drawing every last detail, as the resulting data
file size would be far too large. The Psi-Mapper vector overlays are
*approximate* representations only of the real world, and should not be
taken as gospel!

As I say, all the above support files live in the \APP\MAPPERLO\
directory.


Deleting info from the custom town database, \OPD\MAPPERLO.DBF
--------------------------------------------------------------
The \OPD\MAPPERLO.DBF file is editable under DATA, so you can delete
individual towns etc and modify them in this way. Please remember to
compress the file before exiting DATA !


Grabbing the current map screen as a PIC file
---------------------------------------------
You may well want to grab a map screen as a standard Psion .PIC file,
for printing or editing using DRAW (see small print at end) or PAINT or
any other 3a/3c bit-map editor. Grabbing the 3a/3c screen is done with
pressing 'Control', 'Shift', 'Psion' and 'S' simultaneously (it's
easier to do than it sounds) and lo and behold a file called SCREEN.PIC
will have appeared in the root directory (\) of your internal disk ...

_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_+_

Thanks for trying Psi-Mapper/London! If you decide to remove
Psi-Mapper/London from your Psion, please note that it creates several
small files in \OPD\ on your default disk. MAPPERLO.DBF (custom place
database), MAPPERLO.REG (your registration key, once I've given it to
you) and assorted .LSF files (used to store your preferences and position
etc from session to session). If you've created any custom overlays or
their icons, there may also be a few files in an \APP\MAPPERLO\ directory
on your internal disk ...

Small print:
------------
1) Reverse engineering of the program is strictly forbidden. Attempts to
   try and 'crack' the methods used by me to protect Psi-MAPPER from
   reverse translation are also forbidden. If you would like to create a
   complementary program and use my data-sets, then please get in touch
   with me, it would be nice to be kept informed! Psi-Mapper/London was
   built using Andy Clarkson's OPP code development system.

2) A useful related program to have available for editing and cropping
   any screen-grabs you do in MAPPER-3a/3c is DRAW 2.84, by Rick
   Andrews. This is a S3a/3c-only full bitmap editor and manipulator
   and is also shareware, available wherever you got this program
   from. Highly recommended.

3) You can contact me via email at slitchfield@cix.compulink.co.uk, from
   where I can e-mail you back your registration code. If this e-mail
   does not work for any reason, try slitchfield@ukonline.co.uk

4) If you give Psi-Mapper/London to a friend (as is encouraged with
   shareware!), then it's always best to give him/her the original ZIP
   files. If the transfer takes place by SSD, then remember to include
   both the main MAPPERLO.OPA program and the \APP\MAPPERLO\
   subdirectory full of data files. Do *not* give your friend your
   MAPPERLO.REG registration file, as this is unique to you and contains
   *your* name, and subsequent piracy will tell the world who started
   the outbreak!

5) Thanks to all the testers and contributors out there, who have
   supplied ideas, support, bug-reports, data, code fragments etc,
   especially Eddie Slupski for all those hours of tireless data
   entry. If *you* find a bug or spot an error etc in any of the
   overlays, then please get in touch!

6) Note that the Northings/Eastings and National Grid coordinate systems
   within the Psi-Mapper program family are used with the kind written
   permission of Ordnance Survey.

7) Note on copyrights etc.: be careful to only make use of publicly
   available and public domain data sources when compiling any overlays or
   town/item plans, unless of course you sourced the information (from
   memory?) in the first place! If you submit any data, be sure that you
   are not infringing anyone's legal restrictions!

Steve Litchfield
