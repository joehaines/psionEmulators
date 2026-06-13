Title:       ScrambleOPL (Epoc) V1.1
Description: Scramble proc & variable names in OPL source code
Author:      Richard Smedley <rsmedley@cix.co.uk>
Home page:   http://www.cix.co.uk/~rsmedley/s5/


INTRODUCTION
============

One of the major problems when writing programs for the Psion is that
translated OPL programs still contain the names of all your procedures
and variables. This makes things very easy for hackers, because if they
find a way of decompiling your program they will be able to restore the
names of all your procs and variables to see exactly how the program
works.

ScrambleOPL makes things more difficult for hackers. It modifies the
OPL source code for your program, replacing the name of every proc &
variable with a randomly-generated nonsense name. This means that even
if somebody does succeed in reverse-translating your program, most of
the program will look like gibberish to them - making it a lot more
difficult for people to work out how your program works and modify it.

Using ScrambleOPL doesn't guarantee that your program won't be hacked
by a professional hacker. Sadly nothing can guarantee this. But it will
make things a lot more difficult for hackers, to help protect against
this happening.

ScrambleOPL runs entirely on your Psion, so you don't need to have a
desktop machine (PC, Amiga) to use it.

Full usage instructions are included in the help file.


INSTALLATION
============

This program should be fully compatible with all EPOC machines,
including Psion S5/5mx, Revo, S7, Oregon Scientific Osaris, etc.

Select SCRAMBLEOPL.SIS to install it on your Psion, and everything
will be installed for you automatically.

ScrambleOPL uses the following additional modules, which are supplied
with the program as SIS files:

    PERCENT.SIS (Percent.opm V2.20, by Francois Langinieux)
    SYSRAM1.SIS (Sysram1.opx V1.0)

If you haven't installed these on your system yet, or if you are using
earlier versions of these modules, then you will need to install these
before you can use ScrambleOPL. Again all you need to do is select their
SIS files and they will be installed automatically.


CONDITIONS OF USE
=================

If you want to release any programs that have been protected using
ScrambleOPL, then please read the following conditions of use:

FREEWARE PROGRAMS: Freeware programs which have been protected using
ScrambleOPL can be freely distributed. It would be nice if you could
credit me somewhere in the docs for your program, but this isn't
essential.

SHAREWARE PROGRAMS: If you want to release a shareware program which
has been protected using ScrambleOPL, then I would like you to give me
a free registration code for your program. Again, it would also be nice
if you could credit me in the docs for your program, but this isn't
essential.

COMMERCIAL PROGRAMS: ScrambleOPL can be used to protect commercial
products but you must send me a free copy of the finished product that
you are releasing. You must also mention my name somewhere in the
credits, either in the manual or in the program itself. Note that you
don't have to mention ScrambleOPL, simply mentioning my name is enough.


CONTACTING THE AUTHOR
=====================

If you have any comments about this program, or if you want to contact
me for any reason, then here is my address:

    Richard Smedley
    PO Box 59
    Sutton-in-Ashfield
    Notts
    NG17 3HP
    England

    rsmedley@cix.co.uk

The latest version of ScrambleOPL will always be available to download
from my home page:

    http://www.cix.co.uk/~rsmedley/s5/


CREDITS
=======

I would like to thank the following people for their invaluable help
and support while developing this program:

    Mike Rudin - Author of RevTran
    Francois Langinieux - Author of Percent.opm


COPYRIGHT & DISTRIBUTION
========================

ScrambleOPL is Copyright (C) Richard Smedley 1999.

This program may be freely distributed for non-commercial purposes
providing that all the original files remain fully intact and
unmodified. Commercial use of this program, or any part thereof,
requires the written permission of the author.

Standard disclaimer applies; you use this program at your own risk.

Percent.opm is Copyright (C) Francois Langinieux, 1999.


VERSION HISTORY
===============

V1.0 (27.02.99)

  - The first public release.

V1.1 (11.12.99)

  - Completely redesigned the GUI. ScrambleOPL & SafeOPL now have
    the same "look and feel" to each other.

  - Now displays a progress bar, so you can instantly see how much
    of your program has been protected.

  - You can now press Esc at any time to abort without making any
    changes to your program.

  - Numerous improvements to the docs.
