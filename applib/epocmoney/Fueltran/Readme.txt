S3FUELEX.APP - S5FIMP.APP
-----------------------

CONVERTING S3AFUEL DATA FILES TO S5FUEL FORMAT

This Archive should contain the following files:

S3FUELEX.APP   	This should be installed on the S3A/S3C
S5FIMP.APP	This should be loaded on the S5
README.TXT	This file.

The basic logic of this conversion is that S3FUELEX.APP exports all of the History files in a text-delimited format and then S5FIMP.APP uses these files to build new S5-compatible data files.  I have to do it that way as that is the only common format shared by the programs.

I have carried out this operation on my own data successfully, but it took me a couple of goes, and there are pitfalls.  I have therefore written these instructions as step-by-step process as a real 'idiots' guide, based on my experience.   It may look simple, and some of the steps are obvious,  but you ignore them at your peril.  I strongly recommend you follow the same steps.

SERIES 3A/3C
------------

1.	Load S3FUELEX.APP into an \APP\ directory and install in the usual way using <Psion-I>.

2.	Highlight the first Vehicle and press <Enter>.

3.	It will now export the history file and keep you informed of progress.

4.	Now repeat the export operation for the other vehicles you may have defined.


PC
--

1.	Link the S3A/S3C to the PC and run up PSIWIN 2.    Select the M: directory and double click.   You should see a directory called \XFRFUEL\ , which is where the text files have been placed.

2.	Drag that directory and drop it on the WIN95 desktop.   If you are prompted for 'Conversion' make sure you select 'MSDOS-Text'.

3.	Now shut down PSIWIN2, remove the S3A/S3C and link up the Series 5.  Run up PSIWIN 2 again.  (I have found from bitter experience that turning off the link on the Psion before shutting down PSIWIN tends to crash my PC)

4.	Select the C:\ drive in the window, and drag and drop the \XFRFUEL\ directory from the WIN95 desktop to the C:\ directory.   Again, If you are prompted for 'Conversion' make sure you select 'MSDOS-Text'.   

5.	The window should now show a C:\XFRFUEL\ directory containing all the text files.

6.	Now transfer the program S5FIMP.APP to the Series 5.  It can go anywhere, although the C:\ root directory is as good a place as any, as you will be removing it after this procedure.

SERIES 5
--------

1.	Start up S5FUEL and create all the vehicles first with the EXACT SAME names as the filenames in the \XFRFUEL\directory, ie. the presence of a file called Capri.hss means you call the car Capri  (You can always change it later).  Then shut the program down.

2.	Now tap the S5FIMP.APP file on the system screen (it will not appear on the Extras bar).

2.	If you are reading these instructions then just press Continue to clear the opening screen.  The program should then import the files and inform you of progress.

3.	Now run up S5FUEL again.   You should find the details now match the history on the S3A.  Note that I don't have separate screens now, everything is in one.  Note also that the configuration settings and the reminders are not transferred, you will need to reset those.

4.	Delete the S5FIMP.APP file, not needed anymore.

5.	Delete the C:\XFRFUEL\ directory when you are sure all the data files have been successfully imported.

Any problems, or suggestions on how to improve these instructions, E-mail me at  alanrichey@compuserve.com
