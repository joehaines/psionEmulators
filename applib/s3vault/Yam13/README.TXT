YAM, Version 1.30, November 20, 1995
Copyright (c) 1995 Bermuda Software Publishing
Written by Bruno Essmann (bessmann@iiic.ethz.ch)

New in Release 1.30
===================

Due to a tip I received concerning helpkey handling I decided to release
version 1.30. There have been two minor changes

- Helpkey is now handled correctly under all circumstances, thanks for 
  this tip go to ALasdair Manson.
- Whenever possible procedures are caches. This will increase the runtime
  memory requirements by about 15KB (only if available, it's still playable
  with about 14KB of free RAM) and result in greater speed. The only thing
  that's still 'slow' is the game initialisation.

New in Release 1.20
===================

Well, it took about three month to receive at least one bug report.
Either YAM is mostly free of bugs or nobody is using it, the latter being
more probable as statistics and likelihood tell us. What shall I say at
least I got word from somebody out there ...
Once again there's only a set of minor changes

- there was an awkward bug in the handling of flags. Marking all fields
  (even those which didn't contain mines) caused YAM to rate the game as
  won as soon as all fields containing mines had been flagged.
- the help has been rewritten, it's shorter and clearer now, at least I
  hope so.
- the setup option "startup gametype" has been removed, a game of the 
  type last played is automatically prepared at startup.
- the size of the YAM package has been reduced by over 1KB again, a 
  complete installation (including help file) uses now about 18KB.

New in Release 1.10
===================

There are only some minor changes, but I think it's worth releasing a new
version even if the last release was only a fortnight ago.

- the most important change is the removal of recursion in the procedure
  which does the field check, this drastically reduces the memory required
  to run YAM (and the error "out of system memory" should not appear 
  anymore unless you're really short of memory)
- the size of the executable and the memory needed to run YAM has been
  reduced by about 1KB by a utility called 'aka' by David Palmer (thanks)
  in addition the help file is slightly smaller (about 1KB) so the total
  size reduction of the files on disk is about 2KB
- online help works in the dialogs "Set preferences", "Setup sounds" and
  "Custom game" (the help key is captured in most cases, just press the
  help key long enough and the online help will appear)
- the "Set preferences" dialog has been split up into two dialogs "Set
  preferences" and "Setup sounds".

Introduction
============

YAM is Yet Another Minesweeper for the Psion Series 3a. It will not run
on a Series 3 but feel free to port it, the code is available separately.

There have been two reasons for me to write yet another minesweeper.
Firstly I didn't like the various other minesweepers available (no offence
meant to the authors of those programs, I just didn't like them) and
secondly I wanted to find out if OPL is suited to write 'real' applications
or if it's a mere tool to enhance the built-in calculator. The conclusion
is that I don't like OPL as an programming language. Of course it's better
than nothing, but it lacks all the enhancements of modern programming
languages like Modula-2 or Oberon-2. Fortunately there exist preprocessors
('s3atrans' for MS-DOS computers and 'opl_pp' for the Psion Series 3a) else
I wouldn't have had the nerve to finish this minesweeper.

Principles of Operation
=======================

YAM looks and behaves like the built-in Psion applications. Care has been
taken to use the standard keyboard accelerators (if you don't like them get
a copy of the source code and change them to your suits).
To learn how to play (and about the object of the game if you don't know
it already) press Help, all instructions are included in the online help.

Hardware Requirements
=====================

1) Psion Series 3a
2) at least 18 KB of free memory (or 14 KB without the helpfile)

Installation
============

The installation kit consists of the following files:

name         contents

README.TXT   this text
INSTALL.TXT  installation script for use with MCLINK
YAM.OPA      minesweeper application (required)
YAM.PIC      minesweeper pictures (required)
YAM.HLP      minesweeper online help (optional)

to load

1) make shure the computer can connect to the Series 3a
2) execute MCLINK.EXE @INSTALL.TXT

remarks:
1) to copy yam to a drive other than M: edit INSTALL.TXT accordingly.
2) if you're short of free ram remove the line containing YAM.HLP from
   INSTALL.TXT, the help file is not required but recommended.
3) if you are using RCom modify the INSTALL.TXT accordingly and save
   it as INSTALL.BAT or execute the commands manually.

to install

1) move to the desired location on the system screen
2) press Psion-I and enter YAM.OPA in the installation dialogue

Files and Folders
=================

For YAM to function properly the files have to be stored at the following
locations:

name         location

YAM.OPA      wherever you want, default is \APP\ (on any drive)
YAM.PIC      \APP\YAM\ (on any drive)
YAM.HLP      \APP\YAM\ (on any drive)
YAM.INI      M:\APP\YAM\ (will be created automagically, see below)

When YAM is started the first time it will create a file called YAM.INI
which holds the configuration data. Since this file will be modified
whenever you change the settings or if a new highscore is achieved this
file will be stored on the internal drive in the folder \APP\YAM\. If
you deinstall YAM don't forget to delete this file as well.

Contact Address
===============

YAM, Yet Another Minesweeper
Bermuda Software Publishing
Attn Bruno Essmann
Duebendorfstrasse 184/02
CH-8051 Zuerich

E-mail: bessmann@iiic.ethz.ch

Freeware
========

YAM and its source code are freeware.

I decided to release YAM as freeware because there is a nasty trend that
everybody who writes even a small application thinks that he'll get money
for it. The result is that there are a lot of crippleware applications.
I've spent about 15 hours writing YAM and I did it because programming is
one of my hobbies. I don't have spare time either, I take the time for
programming since it's fun (at least sometimes) and as a basic rule I
release all applications that took less than, say 100 hours to write as
freeware.

Disclaimer
==========

Permission to use, copy, modify or distribute this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and 
that the name of Bermuda Software Publishing not be used in advertising or
publicity pertaining to distribution of the software without specific,
written prior permission.

Bermuda Software Publishing disclaims all warranties with regard to this
software, including all implied special, indirect or consequential damages
or any damages whatsoever resulting from loss of use, data or profits,
whether in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of this software.

