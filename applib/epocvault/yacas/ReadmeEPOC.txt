Yacas (Yet Another Computer Algebra System)
*******************************************

YACAS (http://yacas.sourceforge.net) is an easy to use, general purpose
Computer Algebra System, a program for symbolic manipulation of
mathematical expressions. It uses its own programming language designed
for symbolic as well as arbitrary-precision numerical computations. The
system has a library of scripts that implement many of the symbolic
algebra operations; new algorithms can be easily added to the library.
YACAS comes with extensive documentation (320+ pages) covering the
scripting language, the functionality that is already implemented in the
system, and the algorithms we used.

The entire site (including the documentation) can also be found in the
source code distribution, http://yacas.sourceforge.net/downloads.html.

Yacas for EPOC Machines
=======================

Read the original "EpocYacas.txt" file, written by Ayal Pinkus, the main
author of Yacas. Then read the generic "Readme.txt" text and the more
specific "ReadmeEpoc.txt" (almost a copy of this web page). It is
important to install the "Stdlib.SIS" before "YacasEpoc.SIS".

The most complete distribution of Yacas runs on Linux, the EPOC version
currently lacks some features, mainly concerning input and output :

* "copy & paste" are partially implemented by the user interface
* "\" character at the end of line is not recognized to continue the input
  using the next "In>" line
* "ToFile(filename) text expression" works, but the file should be
  created before (an empty text is enough). At least the user can export
  any result as text files, for example: ToFile("C:\TaylorSin.txt")
  PrettyForm(Taylor(x,0,9) Sin(x)). You can replace "PrettyForm" by "Echo"
  to avoid long outputs
* the RC file is fixed at "C:\System\Apps\Yacas\YacasRC.txt" and has its
  commands read every time Yacas is started
* the history file is stored as "C:\System\Apps\Yacas\YacasHistory.txt"
* "?function", "??", "Help", "quit", "restart", "Ctrl+C" (stop the
  currently running calculation), command-line options, etc., are not
  implemented. To read the documentation, browse the HTML, TXT of PDF
  files outside the Yacas session
* graphical output ("Plot2D" and "Plot3DS") is not implemented

The Revo+ screenshot (www.RobertoColistete.net/Yacas/Yacas1_0_53.gif)
shows a Yacas 1.0.53rev2 session taking 3.2 MB of RAM (using the thin
"YacasServer.exe"), while the "\System\Apps\Yacas\" files use 380 KB.
After automatically loading the scripts needed for "N()", "D()",
"Integrate()", etc., each input/output is faster and takes memory only
to stored results in variables. For example, the script loaded by the
first calling of "Integrate(...)" takes 680 KBytes of RAM, but any
further integration does not use additional RAM (unless you store the
results in variables).

So even a Psion Revo with 8 MB can comfortably run Yacas if there are
not a lot of applications installed. In this case, trying to read Yacas
HTML docs will take a lot of memory using Web or Opera for EPOC, so the
TXT docs are more recommended.

For more details about Yacas releases, read the "Changes.html" document.


Yacas 1.0.43
============

The Yacas SIS file shows version 1.43 because the installation process
cannot display 1.0.43.

This version is old if compared to the current one (1.0.55), but it uses
only 0.6 MB of RAM when started, and 1MB to 2MB of RAM is usually enough
for many calculations.

Some limitations of Yacas 1.0.43 for EPOC :

* the installation process is somewhat slow because the "scripts"
  directory has approx. 100 files
* it does not have an icon
* "Integrate(x)" is not defined, but "Integrate(x,a,b)" is
* "PrettyForm" and "Simplify" functions sometimes close Yacas


Yacas 1.0.53rev2
================

The SIS file shows version 1.53 because the installation proccess cannot
display 1.0.53.

This version is the last one compiled to EPOC (version 1.0.55 is
available on Linux, Windows, etc), but it uses 2.1 MB of RAM when
started, and 3MB to 4MB of RAM is usually enough for many calculations.

The ZIP file contains the directory "ThinServer-400" with a version of
"YacasServer.exe" that uses less 400 KB of RAM (1.7 MB of RAM when
started), just copy it to the "/System/Apps/Yacas/" folder on your EPOC
machine (rename the other "YacasServer.exe" if you want to come back).
This thin "YacasServer" is limited to smaller calculation sessions
due to a smaller stack size.

Yacas 1.0.53 solves all the above limitations about the/and is faster
than the release 1.0.43 for EPOC, but it also has a newer limitation (the
world is not perfect...) due to the client/server implementation :

* output is limited to 5KB characters, more than this will close Yacas

There are also some improvements on the client user interface, version
1.20, when compared to Yacas 1.0.43 for EPOC :

* "copy & paste" now work but just inside Yacas, the selection can use
  "Shift" + "Left", "Right", "Ctrl+Left" and "Ctrl+Right"
* new cursor navigation keys: "Fn+Left", "Fn+Right", "Ctrl+Left" and
  "Ctrl+Right"
* "Shift+Del" keypress work as a delete key
* the scroll bars are turned on by default and their vertical and
  horizontal history size are increased
* "Up" and "Down" keypresses also show the current line and the total
  lines of the history in a message window

Read the "ChangesYacasEPOC.txt" for more details.

Future of Yacas for EPOC/Symbian (wish list)
============================================

There are many possible improvements regarding Yacas on EPOC machines :

* extend "copy & paste" to export & import to the EPOC32 clipboard,
  implement "\" character at the end of line and other features on the
  client for EPOC
* fix the command "ToFile" to create output text files, implement
  "FindFile", "SystemCall", etc., on the server for EPOC
* fix the releases 1.0.54, 1.0.55 and further ones to compile again
  targeting EPOC machines
* fix the limitation of maximum characters of the outputs
* try to optimize the memory used by the server and the scripts 
* create an Yacas OPX so that an OPL program could talk to the Yacas
  server, sending inputs and receiving outputs
* based on this Yacas OPX, I would like to create an open source
  frontend written in OPL with "In(n)"/"Out(n)" browseable cells, edit
  menu, open/save of notebooks, menu pane listing the main functions,
  documentation in EPOC help format, completing of input commands,
  graphics output, etc. As I work better and faster in OPL than in C++, I
  think I would be able to make most of the features of this frontend
  project for Yacas based on my experience programming Smuggers and
  OPLWizard

About Symbian machines, due to memory constraints (2.6 to 4 MB of
operating RAM) to run Yacas 1.0.53, I am not sure whether Series 80
(Nokia 92xx Communicator) and/or Series 60 (Nokia 7650/ 3650/ 6600/
N-Gage, Siemens SX1, Sendo X, etc) could run an Yacas release. But the
UIQ machines (SonyEricsson P800/900, Motorola A920, etc) have 7-8 MB of
operating RAM, and the newer Nokia Communicator and Series 90 machines
(to be release on the 2nd quarter of 2004) are expected to have more RAM
available, so it would be nice to :

* build a Symbian OS 6.0 version of Yacas to run on Nokia 92xx
  Communicators, even on Windows SDK emulator and on constrained memory
  Nokia Communicators, so it would be a preparation to release Yacas for
  new machinies running Series 80 and 90
* build an UIQ release of Yacas, with a frontend written in OPL
  depending on the status of the OPL SDK currently being developed to UIQ

The above lists are expected to attract attention of C++ programmers
interested to contribute in developing Yacas on EPOC/Symbian devices. My
experience in C++ for EPOC/Symbian is limited (but improving as I
develop Yacas!), so many of the above items could not be completed by me
alone.

Resuming my thoughts, as the EPOC and Symbian machines approaches the
power of Windows/Linux/Mac notebooks, an Yacas release targeted to
EPOC/Symbian would be ideal because Yacas is memory efficient and an
open source project, contrary to many other Computer Algebra Systems
(CAS), and Symbian is the best OS for smartphones IMHO. I support
the future vision that PDA & smartphones running CAS will be used as
educational tools by millions of students and as common calculation
tools by many other professionals around the world.

Yacas SIS and Documentation Files for EPOC
==========================================

You can read the HTML documentation in the Yacas Web page,

http://yacas.sourceforge.net

Almost all of this documentation has been converted to simple and
smaller text (ASCII) files, more suitable to be read on EPOC machines.
Download a zip file of these text files from

http://www.RobertoColistete.net/Yacas

The same address hosts the SIS files of Yacas for EPOC machines.

Authors
=======

The Yacas for EPOC web page, the "ReadmeEPOC.txt", the SIS files of
Yacas for EPOC and some C++ modifications on Yacas 1.0.53 for EPOC were
done by Roberto Colistete Jr. Thanks to Daniel Rigby for developing and
compiling the C++ source code for EPOC, as well as sending the files of
Yacas 1.0.53 to me. Last but not least, thanks to all the Yacas
community developers and specially to Ayal Pinkus for supporting Yacas
on EPOC, Linux, Windows, etc.

Visit the PocketIQ site (www.PocketIQ.com) and enjoy my arcade game
Smuggers and many other softwares for EPOC and Symbian machines.

Email : Roberto@PocketIQ.com

Roberto Colistete Jr. - November, 2003
