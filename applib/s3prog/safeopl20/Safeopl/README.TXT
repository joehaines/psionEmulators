Short: SafeOPL 2.0 - Protect OPL progs against RevTran
Author: Richard Smedley <rsmedley@cix.co.uk>
Type: Util (S3a, S3c, Sienna)


                                 SafeOPL 2.0

                           (C) Richard Smedley 1997
                              rsmedley@cix.co.uk


SafeOPL is a freeware program for the Psion S3a/S3c/Sienna which protects
your OPL programs against being decompiled by programs like RevTran.

It consists of two parts: An OPL template that you need to use for writing
your OPL programs, and the main SafeOPL program which patches the code in
the template to protect your OPO/OPA files against RevTran. It doesn't
require you to use OPP or anything similar, and the software runs entirely
on your Psion so you *don't* have to transfer files to a desktop computer
(PC, Amiga, etc) in order to patch them.


INSTALLATION
------------

Copy the following files into these directories on your Psion:

    SAFEOPL.OPA   -> \APP\   (on any drive)

    SAFEOPLT.OPL  -> \OPD\   (this needs to be on the internal drive)

Next, select any of the files under your OPL icon to run the OPL editor,
and then press Psion-O to load a new file. Use the file selector to select
the \OPD\SAFEOPLT.OPL file, and then load it. Now, press Psion-D to save
this as your default OPL template. You can now press Psion-X to exit the
OPL editor.

Finally, press Psion-I to install the SAFEOPL.OPA file onto the system
screen.

Et voila, everything is installed!


PROTECTING YOUR OPL PROGRAMS
----------------------------

Once everything has been installed, just write your OPL programs as you
would normally.

Use the supplied MAIN proc as a template for each procedure that you write
in your program. You'll need to change the PROC1:: label to PROC2:: in the
second proc, PROC3:: in the third proc, and so on. The compiler will warn
you (and refuse to compile your program) if you forget to rename the label
in any of your procedures.

Once you've compiled your finished program into an OPO/OPA file, run the
SafeOPL program and select your file in the file selector. It will then
create a second file called P<filename> (eg. "pmyfile.opa"), in which part
of the code in every procedure of your program has been patched with a
randomly-generated code. If RevTran tries to decompile this nonsense-code,
it will almost certainly crash (often quite spectacularly).

However, here's the downside: Because it's patching your program with a
completely random code, you may occasionally find that the code it
generates *can* be decompiled (into something nonsensical) by RevTran. So
it's worth having a copy of RevTran handy so that you can confirm for
yourself that your p<file>.opa file genuinely is RevTran protected. If it
turns out that it isn't protected, then all you have to do us run SafeOPL
again on your original OPO/OPA file to produce another p<file>.opa file.

Once you've protected your file, you can erase the original OPO/OPA file
and rename the p<file>.opa to remove the "p" from the beginning of the
filename.


COPYRIGHT & DISTRIBUTION
------------------------

This program is freeware, but remains Copyright (C) Richard Smedley 1997.
It may be freely distributed for non-commercial purposes providing all the
original files remain fully intact and unmodified. Standard disclaimer
applies; you use this program at your own risk.

Freeware/shareware programs which have been protected with SafeOPL can be
freely distributed without contacting me first, but it would be nice if you
could give me a credit somewhere in your docs. :-)

If you want to use SafeOPL to protect a commercial program, then you need
to contact me to obtain permission. This will probably involve nothing more
than you mentioning me in the manual and sending me a free copy of the
finished product.


CONTACTING THE AUTHOR
---------------------

If you have any comments or if you want to contact me for any reason, then
here is my address:

    Richard Smedley
    PO Box 59
    Sutton-In-Ashfield
    Notts
    NG17 3HP
    England

    rsmedley@cix.co.uk
    http://www.cix.co.uk/~rsmedley/psion.html

If you find this program *really* useful then postcards, keyrings, five
pound notes, naughty photos of your wife :-) etc can also be sent to me at
that address...

You'll find a complete collection of all my Psion programs on my home page
at the above URL.


VERSION HISTORY
---------------

V1.0 (27.10.97)

  - The first release.

V2.0 (14.11.97)

  - Now individually protects *all* the procedures in your program, not
    just the first procedure.
