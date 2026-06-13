rem Written by Tom Dolbilin (tdolby@livetext.com) 10/28/96
rem This macro allows one to insert a new label to a database
rem and adjust all records accordingly
rem Run from within Data
rem Make sure to BACKUP your data file before usage!!!

PROC macro:
local num%, pos%, label$(20), i%

num% = 225  rem Number of fields in database
pos% = 5  rem New label position (starting with 1 from top)
label$ = "URL:"   rem Name for new label

UseApp:("active")
if Pid2App$:(CurrPid%:) <> "Data"
	if Info%:(4) = 1
		GiPrint:("Run NewLabel from Data")
	endif
endif

rem Update labels
Psion:(%l)
nDown:( pos% - 1 )
PutText:( label$ )
Enter:
Tab:

rem Adjust every record in the file
while i% < num%
	i% = i% + 1
	Esc:
	Psion:(%u)   rem Update record
	SendKey:( 257, 4, pos% - 1 )   rem Skip to label p%
	Enter:
	Tab:
endwh
ENDP
