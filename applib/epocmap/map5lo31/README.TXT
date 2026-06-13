Psi-Mapper/London 3.1 for Psion Series 5 (C) UK 1997-99 Steve Litchfield
========================================================================

Released May 1999

Changes for 3.1 (over 3.0)
------------------------------------------------------------------
Bug fix in the Centre Select function; now made freeware throughout;
------------------------------------------------------------------

Psi-Mapper update information and new versions can be found at
http://3lib.ukonline.co.uk/

Requires Psion Series 5 or compatible with approximately 200k of free
system memory to run in.

********************    IMPORTANT!!    ********************************
This text file documents the history of the program, its installation,
and some of the more technical aspects to running it. For general help
in using and understanding Psi-Mapper/London, please browse through the
*extensive* on-line help. The on-line help is the only documentation of
most of Psi-Mapper/London's features, so work through it properly! 8-)
***********************************************************************

----------------------------------------------------------------------
This is Psi-Mapper/London, released as freeware. The program and
concept are (C) 1997 Steve Litchfield. With the exception of the road
data, all data sets are public domain and were collated using public
domain and other freely available reference works, and data gathered
from privately-owned GPS units. Data sets remain the copyright and
responsibility of the original contributors. No liability whatsoever
can be accepted for any inaccuracies in this program or any of its data
sets.

The road data is "Based upon the Ordnance Survey map with the
written permission of The Controller of Her Majesty's Stationery Office,
(C) Crown Copyright MC 0055"

============
Installation
============
If you unzipped MAP5LO31.ZIP OK, you should have ended up with
2 files. One will be this text file 8-)

The other is an SIS file.

(Experts)
If you know what a .SIS file is, just double-click on each one in Win95 Explorer
or tap on it on the Series 5 and you'll be installed in no time.

(Beginners)
If you *don't* know what a .SIS file is, this must be one of the very first
third party applications you've ever installed. Don't worry, it's all quite 
painless:

  IF you have the 'older' PsiWin 2.01 or if you've downloaded Psi-Mapper/London directly
  onto the Psion 5, the best way to proceed is to copy the "mapperlo.sis" file into a
  suitable place on your Psion. If the file appears on the system screen
  with a question mark beside it, this just means you haven't yet upgraded your
  Series 5 to accept EPOC install files. Grab a copy of the special INSTEXE.EXE
  program from the same place as you obtained Psi-Mapper/London and tap on it on your Psion.
  You should now see a nice little icon beside the "mapperlo.sis" file and can tap
  on it to complete your installation.

  IF you have PsiWin 2.1, just double-click on the "mapperlo.sis" file to launch
  EPOC Install on the PC itself and to complete the installation of the program.

If all went well, you should now have a new "MapperLo" icon on your
Extras bar.

For the technically-minded, Psi-Mapper/London and its overlays etc. will
end up in \System\Apps\MapperLo on your chosen disk.

==================================

In this distribution are the following overlays, Contributor information
also included where known or appropriate.

Vector overlays (i.e. lines and labels) (Thanks to Tony Crocker for
contributing these)

RAILTRAK.MP8  - a vector overlay of the main-line railway routes
TUBELINE.MP8  - a vector overlay of all the tube lines, laid out
                properly (i.e. geographically, not diagrammatically)
TUBEZONE.MP8  - a vector overlay of the tube charge zones

Optional item overlays (i.e. specific points and labels and reference
info, note that some of them cover the whole M25 area and others just
the central "A-Z" area):

The following 5 were all contributed by Eddie Slupski (thanks!)
HOSPITAL.MP7 - Hospitals
FOOTBALL.MP7 - Football clubs
EMBASSY.MP7  - Embassies
SPORTCEN.MP7 - Sports Centres
JUNCTION.MP7 - Motorway exits/junctions

MAINLINE.MP7  - main-line railway routes - (contributed by Tony Crocker)
TUBE.MP7      - tube (underground) stations, with their
                line name(s) shown as abbreviations
PLACES.MP7    - places of interest, with their phone numbers
HOTELS.MP7    - a large selection of hotels across London
NCP_PARK.MP7  - NCP car parks in London
POLICE.MP7    - Police stations - (contributed by Peter Osmond)
PIZZA.MP7     - selected Pizza restaurants across London
KENTUCKY.MP7  - Kentucky Fried Chicken restaurants - (contributed by
                someone, but I can't remember who!)
GOLF.MP7      - golf courses across London
MARKETS.MP7   - markets, with their optimum visiting times
                (contributed by Tony Crocker)
THEATRES.MP7  - theatres across London - (contributed by
                Tony Crocker & Gordon Drayson)
BIGMACS.MP7   - some MacDonalds restaurants - (contributed by Tony
                Crocker)

Notes:
1) Each of the .MP7 overlays listed above has an associated .MBM file of
the same name. These are the on-screen icons for overlays, and will be
used if present in the main \System\Apps\MapperLo folder. If they are not
present, a default 'star' icon will be used ....

2) Each of the overlays includes reference information for each item (for
example, telephone contact numbers), and this can be displayed by
turning 'Overlay Reference Fields' On.

3) Whether using the supplied vector overlays or contributing your own,
please note the low (100m) resolution used by Psi-Mapper/London. It's
no good expecting or drawing every last detail, as the resulting data
file size would be far too large. The Psi-Mapper vector overlays are
*approximate* representations only of the real world, and should not be
taken as gospel!


Editing your custom place database & custom item overlays
---------------------------------------------------------
Although the program allows you to add places and items, you'll need to
use the supplied CustomEd.opo utility to do editing of your custom
places file or other overlays, i.e. for deleting mistakes and unwanted
entries. This functionality *may* be added into the main program in due
course.


Printing or grabbing a current map screen
------------------------------------------
You may well want to grab a map screen as a standard picture file,
for including in WORD, printing or editing using SKETCH. Grabbing the
Series 5 screen is done with pressing 'Control', 'Shift', 'Fn' and 'S'
simultaneously (it's easier to do than it sounds) and lo and behold a
dialog will appear asking where you want the graphic file to be
created. You can then use SKETCH's 'import EPOC picture file' function
to use the graphic in other ways on your Series 5.

Small print:
------------
1) You can contact me via email at slitchfield@cix.co.uk, or
   at slitchfield@ukonline.co.uk

2) Thanks to all the testers and contributors out there, who have supplied
   ideas, support, bug-reports, data, code fragments etc. You are too
   numerous to mention! If *you* find a bug or spot an error etc in any
   of the overlays, then please get in touch!

3) Disclaimer: I accept no responsibility for anything happening to you
   as a result of inaccurate information within Psi-Mapper! I try to
   keep the overlays up-to-date as possible, but that is as far as my
   liability goes!

4) Do not use this application while on the move! If you crash your car
   while looking at your Psion, don't blame me!!

Steve Litchfield