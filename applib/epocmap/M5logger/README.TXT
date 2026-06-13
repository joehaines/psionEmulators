M5Logger V1.1 Freeware Readme.Txt
-------------------------------------------

M5Logger is a mileage logging program that has been developed over the last 4 years.

M5Logger allows you to record your present location and trip (speedometer) reading
to a database with a time and date stamped entry. This database records whether the 
trip is personal or business, the actual mileage completed for the journey and gives 
sub totals of each type of mileage.

The database can be exported to a tab delimeted file which can be viewed from 
within M5Logger. 

I have made this a freeware program after porting it to the S5 from my S3c. I use it
on a daily basis to record all my business mileage for expense purposes. 

New functions have now been added including viewing the exported data from within the
program, a proper help file and launching MyCar (from Ad Bosch) from within the 
program (See History).

If you do use the program please let me know - I am not a programmer and have pulled 
this together from various bits of code and a lot of trial and error (See Acknowledgements).

--------------------------------------------------------------------------------------
Know Problems with V1.1 
---------------------------------
* None
--------------------------------------------------------------------------------------


Installation.
-------------
To install, create a c:\system\apps\M5Logger directory and copy in the following files: 

M5Logger.app		- Main app
M5Logger.aif		- Application information file
M5Logger.mbm		- Bitmap file.
Export.mbm		- Icon file.
Newentry.mbm		- Icon file.
Viewexpt.mbm		- Icon file.
Viewtot.mbm		- Icon file.
M5Screen.mbm		- Graphics file.
M5Title.mbm		- Graphics file.
Exview.opo		- Export File Viewer. (Courtesy of Roger Muggleton [Teddy]).

And create a C:\Documents\Logger directory and copy in the following files: 

M5Logger.Hlp		- Help file (data format)


The following file is created on startup:

L-Base.ODB		- Database of trips 
			  (stored in C:\Documents\Logger directory)

The following file is created on export of data:
L-Base Export.Txt	- Tab delimited text file with all recorded trips 
			  (stored in C:\Documents\Logger directory)


Disclaimers.
------------
You install and use at your own risk, as with any application. 
I accept no responsiblity for any data loss, system damage or machine failure.


Thank you for trying M5Logger.

MarkR 

	
History
-------
Ver 0.1		  (S3)	- Initial code released by Les Hall for the S3 256K machine (1993)
Ver 0.2 	  (S3a)	- Adapted for improved graphics and storage capabilities.
Ver 0.3 	  (S3c)	- Further enhancements to core program.
Ver 1.01 (Beta 1) (S5)	- First public release of program after porting to the S5
Ver 1.01 (Beta 2) (S5)	- Second public release.
			- Now has a unique ID.
			- Fixed graphics on toolbar (Thanks to Al Richey and Ad Bosch for this).
			- Added menu items for User Settings (to be incorporated in next release)
			  Thanks to Hedley Wright for the suggestions.
Ver 1.02 (Beta 3) (S5)	- Fixed drop out when viewing exported data - the system now returns to
			  M5Logger when you exit the data viewer.
			- Tidied up the code to gain speed increase in display.
			- Installed screen graphics (well it looks nicer!)
			- Set up a launch to Ad Bosch's "My Car" program from within M5Logger.
			  (requires the MyCar program on your system to work)
Ver 1.03 (Beta 4) (S5)	- Menus now in 'standard' format.
			- First user requested option installed for selecting exit method. This
			  has been tested fully and works with PsiWin 2.0. Note - the program does
			  not get restarted by PsiWin on completion of the backup!
			- Implemented a proper help (Data) file. Still working on access once the
			  help file is open - the example from Psion does not appear to work!!			
			- Switched off the 'beep' on exit (by menu) 8-)
Ver 1.1           (S5)  - Help "Go Back" now working propoerly. Also the help file is closed on
                          exiting M5Logger to reduce system memory usage.
                        - I have decided to use the standard Ctrl+E to exit. This ensures that
                          PSiWin can close the program down for BackUp.
                        - As this is such a small prog (<22Kb, incl the Help file), I have 
                          I have decided to set it to run from C only. If this causes you a 
                          problem let me know.


Future Plans.
-------------
Only updated if new functionality is released by Psion, which can be utilised by this program
running on the S5, or at specific User request.


Acknowledgements.
-----------------
As I said I am not a programmer and this would not have got to a Beta stage without the help of 
the following: 

Les Hall (ML3)				- For releasing the original code on which this is based.
Adrian Harper (Livewire Total 5)	- For persuading me to put out a beta and highlighting 
					  the problems with the alpha code.
Roger Muggleton	(Teddy)			- For allowing me to incorporate the core of Teddy into
					  my programme as the Export Data File Viewer.
Alan Richey (S5Bank et al)		- For producing EventDemo. Without this code I would not 
					  have got the toolbars etc working at all.
Jason Keen (Backlite)			- For his support.
Steve Lichfield (PsiMapper)		- For running a great site, incl programming guides and
					  suggestions.
Paul Law (Easynote)			- For advice on toolbar graphics,screen refresh and Help
                                          Files.
Ad Bosch (MyCar)			- For assistance with screen graphics.
