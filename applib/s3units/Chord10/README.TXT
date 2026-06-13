CHORD v1.0
----------

This is a Psion 3a chord-finding application, for stringed instruments.

This package should contain the following files:

   chord10.opa    The chord finder application.
   chdbook.odb    The database of 'book' chords.
   chdguess.odb   The database of chord spellings.
   chdhelp.rsc    The on-line help file.
   chdhelp.txt    Plain text version of help.
   readme.txt     This file.


Installation
------------

Copy:

   chord10.opa to any \app\ directory.
   chdbook.odb, chdguess.odb and chdhelp.rsc to any \app\chord\ directory.

Install the program by pressing Psion-I at the Psion system screen and
then selecting Chord10.opa.

If you do not want on-line help you do not need to copy chdhelp.rsc.

If you do not play a guitar with standard EBGDAE tuning, then you
don't need to copy chdbook.odb (the program will run without access
to the 'book' mode - see later).

The program creates two files on the internal drive of your Psion:

   \opd\chord.ini      Contains setup details.
   \chd\chord.chd      The default file for saving your chords. Any new
                       files you create are saved, by default, in this
                       \chd\ directory.

NOTE: it would be unwise to save chd files to a flash SSD, as the
      program appends changes to the file (like Data and Agenda). On
      a flash SSD this space would not be recovered, and the files
      would eat up disk space. On a RAM pack, or on the internal drive,
      the files are compressed automatically when they are saved.


How Does It Work?
-----------------

At its simplest, press the space bar, then type in the chord name,
e.g. Cm7. The chord will be displayed with relevant details. Then
press the space bar again to find another inversion, or press the
enter key to store the chord in one of the ten chord boxes.

See the file chdhelp.txt for a full manual. This is also available
from within the program via the help key.


Registration
------------

Chord is shareware, not free software. If you continue to use it then
send ten UK pounds to:

Graham Brand, 1 Staveley Gardens, London W4 2SA, UK.

I will send you a code which will disable the registration screen
which pops up to remind you that programs like this take a long time
to write!

If you are registering from outside the UK, then please send either
10 UK pounds in cash, or round up to the nearest amount to 12 UK
pounds (to cover bank charges) in paper notes in your own currency,

Your support will enable me to develop the program further - feel
free to offer suggestions, ideally by email. If you have an email
number, quote it when registering, as this is the quickest way
for me to send you your code.


Thanks
------

Many thanks to the beta-testers:

Martin Aldred, Neil Brewitt, Roger Burton-West, Phil Camp, Nicolas Pike,
Steve Rumsby, Graham Tappenden, and Simon John Wallace.


Copyright (and standard disclaimer)
-----------------------------------
 
The Copyright for Chord v1.0 is reserved by Graham Brand, (c) 1995.
All rights reserved. The unregistered shareware version may be
distributed freely, unmodified, as a package containing all of the
files mentioned above. The program is provided without warranty of any
kind. In no event will I be liable for damages arising from its use.

However, any bug reports can be sent to me, preferably by email, and I
will do my utmost to fix them.


Graham Brand
gbrand@sagar.win-uk.net
22 July 1995

