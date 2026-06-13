This zip contains a couple of potentially handy macros, for use with
the KMAC macro facility on the S3a:

1. A macro to insert the time-date string at the current cursor
position of the foreground application

2. A macro to count the number of words in a highlighted block of
text in the Word Processor.

Each of these macros is customisable, so that users can improve their
performance.

Files in this zip
-----------------
README.TXT      This documentation
CWORD.DYL       Used by the word counting macro
CWORDSRC.TXT    Discussion of source code of CWORD.DYL

Background
----------
I assume you already have KMAC installed and running on your S3a.
If not, see eg message 286 on the psion/files conference on CIX.

Additions to your top-level KMA file
------------------------------------

MACRO DateStamp
KEY Z
QUIET
CONTENTS
oM:\OPO\datestmp.opo

MACRO Count Words
KEY K
QUIET
CONTENTS
HA:\DYL\CWORD.DYL

Comments
--------
You can of course choose different key assignments, and/or alter the
exact locations where the .OPO and .DYL files are expected to be
found.

The first of these works by invoking an LPC-aware opl program (source
given below), and then second works by getting the foreground process
to hook the specified DYL.

Code for DATESTMP.OPL
---------------------

  PROC datestmp:
    global lpcHand%,lpoHand%
    loadm "m:\opo\kmaclib"
    call($198d,100,0)
    lpOpen:
    lpWrite:(%f,"")
    lpWrite:(%S,datim$)
  ENDP

The CALL locks the running OPO into background, to leave the previous
application in foreground.

The GLOBAL declarations are standard for an LPC-aware OPL program, as
are the LOADM and lpOpen: lines.

The lpWrite:(%f,"") attaches to the foreground process.

The lpWrite:(%S,str$) then sends the string str$ to the attached
process, character by character as separate keypresses.

If you don't like the particular format provided by DATIM$, change
this bit of the program.

Where you have to put files
---------------------------
The above macro definitions get edited into your top level .KMA file.

You need to translate the above .OPL program and then put the
resulting .OPO wherever the "o" command expects to find it.

You need to put the file CWORD.DYL where the "H" command expects to
find it.

How to invoke the macros
------------------------
It's the same as for any other KMAC macro.
