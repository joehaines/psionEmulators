TimeSheet- V1.05<beta> 
FreeWare for the Psion 3a, May 1996
by Andrew Burger

* Please notify me of any bugs, suggestions, comments, etc.
	at-    102116,610@compuserve.com
---------------------------------------------------------------------------
Contents:
	- readme.txt     - This file
	- TSheet.opa     - The application

Description/Overview:
	TimeSheet was written as a quick and easy way to manage timesheet
	entries.  This program is not a project planning/management program,
	and is not intended to be one.

	TimeSheet was and is being developed as a timesheet program ONLY.  
	The application was written as to how I manage my time and hourly 
	rates.

Changes since ver 1.00<alpha>, 1.04<beta> dated May 3, 1996:

	- Added support for "Bi-Weekly, and Monthly" timesheet types.
	- Added additional error level routines 
	- More compact/efficient code, removed load module in v1.04        
	- When browsing time and note entries for timesheets Bi-Weekly 
	  and Monthly, and arrow will appear next to the TimeSheet name
	  in the top right of the screen to show next and previous screens.

Limitations:
	- Help is not available in this version.
	- Only supports 5 seperate clients (max.)
	- No current print functions

Proposed Changes and Additions:
	The following are changes and additions that I am currently working
	on.  If there is anything that I am missing, please send me 
	suggestions via email as addressed above.

		- 5 Client name restriction lifted
		- Client Project Name/Codes, Cost Center, and other fields.
		- All printing functions enabled
		- Edit Sheet name (after creation time)
		- others ? suggestions?

Install TimeSheet:
	- Copy "tsheet.opa" to the \APP\ directory on any 
	  drive on the 3a
	- Install "TSHEET.opa" using Psion-I from the System screen.

Some Help:
	When TimeSheet first starts, it is basically a blank sheet:
	
	*First   -you should set your preferences, currency symbol, username
	*Second  -add all of your client names and billing rates 
	*Third   -Create a new file (.DBF) with appropriate information
	*Forth   -Open the file you just created.  Enter time as desired 
		
	TimeSheet was written to speed up access.  The application will 
	retain the last file you had opened and automatically open that 
	file when re-started.  Time sheet will create the directory
	M:\TSHEET and appropriate .ODB and .DBF files. 
	
	Psi-H - Enter new hours
	Psi-F - Create new timesheet file
	Psi-O - Open existing timesheet file
	Psi etc., etc., etc.

Registration: 
	- None Required.  TimeSheet is FreeWare, Distribute freely.

Disclaimer:
	- No Warrantees or guarantees of this appication is implied...
	- Use it at your own risk.
