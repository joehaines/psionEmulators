Title:       SafeOPL (Epoc) V1.2
Description: Protect your OPL programs against RevTran
Author:      Richard Smedley <rsmedley@cix.co.uk>
Home page:   http://www.cix.co.uk/~rsmedley/s5/


INTRODUCTION
============

SafeOPL provides you with a quick & easy way of protecting your OPL
programs against RevTran. RevTran is a well-known program which makes
it very easy for anybody to reverse-translate OPL programs, allowing
them to modify your program or find out how it works.

SafeOPL consists of two parts; an OPL template that you need to use
for writing your OPL programs, and the main SafeOPL program which
patches the code in your finished program to protect it against
RevTran.

Using SafeOPL doesn't guarantee that your program won't be hacked by
a professional hacker. Sadly nothing can guarantee this. But it will
stop "normal" people from using RevTran to hack your program.

SafeOPL runs entirely on your Psion, so you don't need to have a
desktop machine (PC, Amiga) to use it.

Full usage instructions are included in the help file.


INSTALLATION
============

This program should be fully compatible with all EPOC machines,
including Psion S5/5mx, Revo, S7, Oregon Scientific Osaris, etc.

Select SAFEOPL.SIS to install it on your Psion, and everything will
be installed for you automatically. You will find the OPL template
(SafeOPL.opl) in your System\Apps\SafeOPL folder.

SafeOPL uses the following additional modules, which are all supplied
with the program as SIS files:

    PERCENT.SIS (Percent.opm V2.20, by Francois Langinieux)
    SYSRAM1.SIS (Sysram1.opx V1.0)

If you haven't installed these on your system yet, or if you are using
earlier versions of these modules, then you will need to install these
before you can use SafeOPL. Again all you need to do is select their
SIS files and they will be installed automatically.


CONDITIONS OF USE
=================

If you want to release any programs that have been protected using
SafeOPL, then please read the following conditions of use:

FREEWARE PROGRAMS: Freeware programs which have been protected using
SafeOPL can be freely distributed. It would be nice if you could
credit me somewhere in the docs for your program, but this isn't
essential.

SHAREWARE PROGRAMS: If you want to release a shareware program which
has been protected using SafeOPL, then I would like you to give me a
free registration code for your program. Again, it would also be nice
if you could credit me in the docs for your program, but this isn't
essential.

COMMERCIAL PROGRAMS: SafeOPL can be used to protect commercial products
but you must send me a free copy of the finished product that you are
releasing. You must also mention my name somewhere in the credits,
either in the manual or in the program itself. Note that you don't have
to mention SafeOPL, simply mentioning my name is enough.


KNOWN BUGS
==========

Problem: SafeOPL crashes with Kern-Exec error 3 if you try to change
folders by pressing the 'Tab' key.

Solution: This is caused by a system bug in Epoc. Pressing the 'Tab'
key while you are anywhere in the 'System' folder will crash any OPL
program. Always change folders by typing the name of your folder on
the keyboard, not by using the folder list.


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

The latest version of SafeOPL will always be available for downloading
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

SafeOPL is Copyright (C) Richard Smedley 1997-1999.

This program may be freely distributed for non-commercial purposes
providing that all the original files remain fully intact and
unmodified. Commercial use of this program, or any part thereof,
requires the written permission of the author.

Standard disclaimer applies; you use this program at your own risk.

Percent.opm is Copyright (C) Francois Langinieux, 1999.


VERSION HISTORY
===============

V1.0 (08.08.98)

  - The first public release.

V1.1 (03.04.99)

  - The file selector now defaults to "System/Apps" instead of the
    root folder.

  - Numerous internal improvements to the code.

  - Gave the docs a much needed rewrite.

  - Now requires Sysram1.opx

V1.2 (11.12.99)

  - Completely redesigned the GUI. SafeOPL & ScrambleOPL now have
    the same "look and feel" to each other.

  - Now displays a progress bar, so you can instantly see how much
    of your program has been protected.

  - You can now press Esc at any time to abort without making any
    changes to your program.

  - Numerous improvements to the docs.