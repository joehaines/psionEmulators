		Global Conquest version 1.03
		===========================
		
		(c) 1999 Thomas Wright
		--------------------------

Note: turn the status window off when viewing this file.  This file is
meant to be viewed on the Psion.

Contents:
~~~~~~~~~
1. Introduction.
2. Installation.
3. Troubleshooting/FAQs
4. Contact info.


1. Introduction:
~~~~~~~~~~~~~~~
Global Conquest is a two player game (each player needs a
Psion 3a/3c/3mx).  I have not tested it with the Psion 3mx processor
but I can't foresee any problems if it uses EPOC 16 (which I think
it does).  I'm sorry, but you can't use it with the S3a emulator.

It is a war simulation which puts the might of an army at your
fingertips, with large maps, animated cursors, intelligent units
and two-player capability (by a serial link cable or by infrared).

2. Installation:
~~~~~~~~~~~~~~
If you unzipped the ZIP file with the `recreate directory'
option switched on, simply copy the files, in their directories,
to a drive on your Psion.  \APP\CONQUEST.OPA can be on any
drive.  ALL OTHER FILES must all be on the same drive as each
other, but not necessarily the same drive as 
\APP\CONQUEST.OPA is on.

I do not recommend installing most of the game to a flash SSD
because it writes the game settings to disk every time you play,
but the file \APP\CONQUEST.OPA can be installed to a flash SSD
without any problems.  If you are limited on disk space, look at
the list of files later in this document to see which files are
essential and which are optional.

Install the game from the Psion system screen as normal:
press Psion-I and select `CONQUEST.OPA' from whichever
drive you installed it to, then press Enter.

Have fun!!


More detailed installation instructions (for people who cannot
`recreate directory' when unzipping):
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following is a list of files, along with which directories
they should be installed to.

All files marked with a `#' must be installed to the same disk.
Files marked with a `?' are optional.

	 File									Directory						Notes
	 ~~~~								~~~~~~~~~						~~~~~~
	 CONQUEST.OPA				\APP\							Program file
?	#CONQUEST.RSC			\CONQUEST\					Help file
?	#INTRO.VID					\CONQUEST\					Intro. video
	#All MOD files (*.MOD)	\CONQUEST\					Game data
	#All PIC files (*.PIC)		\CONQUEST\PICS\		Game data
?	#All WVE files (*.WVE)	\CONQUEST\SOUNDS\	Sound FX

Note: although the help file is optional, I strongly recommend that you
install it, at least until you get an idea of the game.

A full install (all the files) takes up 165k of disk space, and a minimum
install (only the essential files) takes up 63k.

3. Troubleshooting/FAQs:
~~~~~~~~~~~~~~~~~~~~~~~~
This section is also in the help file, which is included with the game,
but I thought a shortened version would also be useful here, in case
you can't get into the game to view the help file.

Q: I want to change the drive Global Conquest is installed to.  What
     do I need to do?
A: Step 1: Exit Global Conquest.
     Step 2: Move the directory "\CONQUEST\", and all the files in it to
          whichever drive you want the files on.
     Step 3: Delete the file \OPD\CONQUEST.DRV from the Internal
          drive.
     Step 4: run the game and use #027#026 to choose the disk the files are
          now on, and press Enter to start the game.

     Note: all the files for the game must be on the same drive, except:
          \APP\CONQUEST.OPA - Can be on any drive.
          \OPD\CONQUEST.DRV - Must be on the Internal drive.  Note:
          this file is made by the game and should not be moved to a
          different drive or directory.

Q: Can I install Global Conquest to a Flash SSD?
A: Yes, you can, but read this:
     As with any other file, when you delete a saved mission file from a
     Flash SSD, the space is not reclaimed until you either format it or
     compress it (with Psion Manager).  There are two solutions to 
     this:
     1. Don't save any games, or only save a few that you will use
          frequently.
     2. Use a different disk for saving games to, by changing the drive
          selector in the `Save mission' dialog box.

Q: Can I install Global Conquest to a remote drive (eg. one on  my
     PC)?
A: No.  Global Conquest can only be installed to a local drive (A, B, C
     and Internal) because it needs the communications port to be
     free during the game.

4. Contact info:
~~~~~~~~~~~~~~
Email me on tom_131@hotmail.com with comments, bug reports,
suggestions, requests, etc. or visit my website:
	www.geocities.com/siliconvalley/horizon/6565/
Please note that this address must be all lower case.

-------------------------------------------
Global Conquest ¸ Thomas Wright, 1999
