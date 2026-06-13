HTMLEdit: the HTML editor for Psion Series 5 and compatibles
------------------------------------------------------------
v2.40, January 27th 2002

Open Source freeware 
What's new for v2.40?
-----------------------
The program have been turned into open source freeware, and is now maintained
by Trygve Henriksen
Bug fixed: The memory loss problem(It allocated a new block of memory every
time you loaded a file, but never released it)
Bug fixed: Crappy handling of other screensizes than the S5(640x240) and the
Geofox One (640x320)
Bug fixed: Now no longer tries to open index.html in the
\system\apps\htmledit\ folder when it can't find the last opened file during
application startup. It now loads index.[extn] from your selected default
folder. This needs more work, though....
Bug fixed: Can now be installed on E: on the S7/netBook computers.

Numerous bugs still remain, though....

Registration system removed. 
Sysram1.sis now inside the main htmledit.sis file.

Bugs to be targeted:
EVLINELEN crashing...
Browsing only works with the old MSGSuite 1.5. Must be patched for WEB 2.0
and Opera 5.13

----------------------
v2.31, March 4th 1999

Shareware by Steve Litchfield

What's new for v2.31 (since v2.2)?
---------------------------------
Bug fixed: "OPL IO System error 2" now sorted out on "File open";
Bug fixed: Control-left/right near start and end of file now handled properly;
Bug fixed: pressing Escape while text selected now works properly;
Help and examples added to enable custom clip-text to also be used within the
Tags system;
New registration code system in place

Installation
------------
If you unzipped HTMLEdit OK, you should have ended up with
2 separate files. One will be this text file 8-)

The other one is a SIS file.

(Experts)
If you know what a .SIS file is, just double-click on it in Win95 Explorer
or tap on it on the Series 5 and you'll be installed in no time.

(Beginners)
If you *don't* know what a .SIS file is, this must be one of the very first
third party applications you've ever installed. Don't worry, it's all quite 
painless:

  IF you have PsiWin 2.1 or above, just double-click on the "htmledit.sis"
  file to launch EPOC Install on the PC and to complete the installation.

  IF you have the 'older' PsiWin 2.01 or if you've downloaded HTMLEdit
directly
  onto the Psion 5, the best way to proceed is to copy the "htmledit.sis"
file into a
  suitable place on your Psion. If the file appears on the system screen
  with a question mark beside it, this just means you haven't yet upgraded
your
  Series 5 to accept EPOC install files. Grab a copy of the special
INSTEXE.EXE
  program from the same place as you obtained HTMLEdit and tap on it on your
Psion.
  You should now see a nice little icon beside the "htmledit.sis" file and
can tap
  on it to complete your installation.

If all went well, you should now have a new "HTMLEdit" icon on your
Extras bar.


Expert troubleshooting
----------------------
[For the technically minded, after installation:]
1) the HTMLEdit program files should exist in \system\apps\htmledit
2) the sysram1.opx file should exist in \system\opx
3) the vwin.opo and colorpick.opo files should exist in \system\opl
4) the tag libraries should live in \system\apps\htmledit\libs



Registration
------------
No longer necessary. This program is freeware

Thanks, credits and notes:
==========================

1) Thanks to Richard Panton for his virtual window library vwin.opo and for
various 
   users, including George Cooke, for their feedback and help. And thanks, of
course, 
   to Russ Spooner for starting the whole project off in such fine style.

2) OPL32 was never really intended to use ascii as its document base... 
   We have found this out the hard way. Occasionally you will find that Html
files
   imported from other editors may contain stray characters (usually
invisible to
   the naked eye) - these can cause HTMLEdit to misbehave... you have been
warned. 

3) Don't try to use really tiny files (less than 16 characters) 
 
4) Very occasionally you might lose the cursor, or it will appear that
HTMLEdit is
   editing the wrong line. To fix this, first try hitting "End" then "Home";
   failing this, try Control-Home. This will reset the cursor position.
