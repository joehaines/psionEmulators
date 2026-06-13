S3EXPORT.APP - S5BIMP.APP
-------------------------

CONVERTING S3ABANK DATA FILES TO S5BANK FORMAT

This self-extracting Archive should contain the following files:

S3EXPORT.APP	This should be installed on the S3A/S3C
S5BIMP.APP	This should be loaded on the S5
README.TXT	This file.

The basic logic of this conversion is that S3AEXP.APP exports all of the Data files in a text-delimited format and then S5BIMP.APP uses these files to build new S5-compatible data files.  I have to do it that way as that is the only common format shared by the programs.

First some important points to note:

1.	This will work only on data files produced by S3ABANK V1.9, so if you do not have that version you need to upgrade first.   Copies can be downloaded from my Web Page at:

	http://members.aol.com/alanrichey

Basically, it was much too complex to try and support previous versions.

2.	It will convert all of the files except the Memopad files, which have a WRD extension.   The memopad function in S5BANK is completely different, so the best solution is to simply transfer them using PSIWIN 2, as you would for any other Word files, load them into S5 WORD and then cut & paste them into S5BANK.

3.	S3EXPORT.APP is common to all my programs, so you will initially be asked which program you wish to export.   However, on the S5 I have to have different headers for each program so you will need the specific conversion program for S5BANK.

4.	Psion have changed the code for the pound sign from that used on the S3A/S3C.    In the S5 currency screen it will therefore appear as the wrong symbol.   You simply need to update it there.   If like me, you have a US S5, with no pound sign, then simply select 'Insert special character' in WORD and do a 'cut & paste'.  Any pound signs you have in Archive header lines will need to be changed before you import to the S5.  If you want to change these see the PC section below for details on how to go about it.

I have carried out this operation on my own data successfully, but it took me 5 attempts, and there are pitfalls.  I have therefore written these instructions as step-by-step process as a real 'idiots' guide, based on my experience.   It may look simple, and some of the steps are obvious,  but you ignore them at your peril.  I strongly recommend you follow the same steps.

SERIES 3A/3C
------------

1.	Load S3EXPORT.APP into an \APP\ directory and install in the usual way using <Psion-I>.

2.	Highlight 'S3AExp' and press <Enter>.

3.	It defaults to "S3ABANK V1.9", so just press <Enter>.

4.	It will now export each of the files and keep you informed of progress.  If the program crashes it means it has found a file which is not V1.9 compatible.    Note which file it is that is causing the crash, then go into S3ABANK and force an update on that area to resave the file in V1.9 format.   Normally that is obvious, for example if it is the main Group file that is causing the problem, then opening up the Accounts screen and updating one of the accounts will force the program to save the Group file in the V1.9 format.   If you get stuck, just E-mail me the details and we will sort it.

5.	After a few goes, or even first time if you are lucky, the program will run right through.  

PC
--

1.	Link the S3A/S3C to the PC and run up PSIWIN 2.    Select the M: directory and double click.   You should see a directory called \XFRBANK\, which is where the text files have been placed.

2.	Drag that directory and drop it on the WIN95 desktop.   If you are prompted for 'Conversion' make sure you select 'MSDOS-Text'.

3.	Now shut down PSIWIN2 and remove the S3A/S3C.  (I have found from bitter experience that turning off the link on the Psion before shutting down PSIWIN tends to crash my PC)

4.      As stated, the  £ (pound) symbol does not have the same ASCII code in the Series 5 as it does on the S3A/S3C.  If you are a UK user, or operate Groups in Sterling, or have  Account transactions or Expenses in sterling as your Foreign transactions, you will have the pound sign used in various files.  If you wish to correct the symbol code, edit the files now before transferring to the S5.

5.      Open the Win95 Wordpad (usually in Start menu : Programs : Accessories), navigate to the \XFRBANK\ directory on the Desktop and open each file in turn  (Transaction Archives normally have the extension .ARC and Expenses Archives have .OLD).  Do a 'Replace All' using Alt+0156  in the 'Find what' field and the normal keyboard £ symbol (or Alt+0163) in the 'Replace with' field.  Save the file and repeat for all the other archive files.  Then do the same also on the CURRENCY.DAT file.

6.      Now link up the Series 5 and run PSIWIN 2 again.

7.	Select the C:\ drive in the window, and drag and drop the \XFRBANK\ directory from the WIN95 desktop to the C:\ directory.   Again, If you are prompted for 'Conversion' make sure you select 'MSDOS-Text'.   

8.	The window should now show a C:\XFRBANK\ directory containing all the text files.

9.	Now transfer the program S5BIMP.APP to the Series 5.  It can go anywhere, although the C:\ root directory is as good a place as any, as you will be removing it after this procedure.

SERIES 5
--------

1.	Start S5BANK in the usual way.  If it the very first time then make sure the Group title you use is the same as one of those in S3ABANK.

2.	If you have the Fuel Module loaded then select that using <Ctrl+R>,<Ctrl+F> or via the View menu.   Now use <Ctrl+N> to enter your vehicles, specifying the EXACT same name as in the S3ABANK version.   You do not need to enter any data, the program just needs to know they exist.

3.	If you have the Expenses module loaded then select that using <Ctrl+R>,<Ctrl+E> or the View menu.  Now use <Ctrl+N> to enter the correct number of trips that you have in S3ABANK.  For now just call them 'One', 'Two' etc and leave all the rest of the details blank.  Again, you do not need to enter any data, the program just needs to know they exist.

4.	Go back to the main account screen and select (or create) a Group with the same name as one from your S3ABANK data files.   Finally press <Ctrl+E> to exit.  You must shutdown S5BANK as it cannot run at the same time as S5BIMP.APP.

5.	Now select the S5BIMP.APP file name and tap, or press <Enter>, to run it.    Note that it does not install itself on the 'Extras' bar.  This is intentional, as for obvious reasons it uses the same unique ID as S5BANK and if the system saw 2 programs with the same number it would get very confused !!

6.	If you are reading these instructions then just press Continue to clear the opening screen.  Now you should have a Menu where each of the main core modules and the optional modules are selectable.   The reason I did it like this is so you can do just some of them if required.   However, for the first time, select each of the Core modules in turn and then whichever optional modules are appropriate.   In each case the program will inform you of progress.   Once you have transferred all of the required modules just Quit with <Ctrl+E>.

7.	Now run up S5BANK.  You will need to go into the main Archive screen (and, if appropriate, the Expenses Archive screen) and select the Archive file to view.  If appropriate you will also need to go into the Expenses module and rename your trips to the correct titles.   From the main screen press <Ctrl+E> to save it, and that will generate a new Accounts.ini file with all the new settings.  In my system it all works.   Hope it does in yours too :-))

8.	Note that the conversion doesn't transfer your particular S3aBank program configuration and preferences so you will need to set these up again for S5Bank.

9.	Delete the S5BIMP.APP file.   As I said before, having 2 programs with the same ID might cause havoc !!

10.	Delete the C:\XFRBANK\ directory when you are sure all the data files have been successfully imported.

Any problems, or suggestions on how to improve these instructions, E-mail me at  alanrichey@compuserve.com
