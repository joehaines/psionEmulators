rem -- Use this macro to get help on an OPL keyword or a macro
rem -- function. It assumes you have two Data files installed:
rem -- OPL.DBF with OPL keywords and MACRO.DBF with macro
rem -- functions. Run from the Program editor after positioning
rem -- the cursor on the keyword in question.


PROC macro:
local func$(255), file$(15), i%

UseApp:( "Active" )

rem -- Quit if not run from OPL or macro editor
if Pid2App$:( CurrPid%: ) <> "Program"
	GiPrint:( "Help is available only from OPL or macro editor" )
	return
endif

rem -- Select word under cursor
SendKey:( 262, 14, 1 ) rem Control-Shift-Home

rem -- Make Word the bring server if it already isn't
if CurrPid%: <> SrvPid%:
	ToBack:
	ToFront:
endif

rem -- Read the highlighted keyword
func$ = GetText$:

rem -- Test to see if it's an OPL keyword or a macro function (unlike
rem -- macro functions, OPL keywords do not contain a colon - ":")
i% = loc( func$, ":" )
if i%
	file$ = "macro.dbf"

	rem -- remove trailing colon
	func$ = left$( func$, i% - 1 )
else
	file$ = "opl.dbf"

	rem -- Remove trailing "(" if any
	i% = loc( func$, "(" )
	if i%
		func$ = left$( func$, i% - 1 )
	endif
endif

rem -- Run Data (if not already running) and switch to it
if Fil2Pid%:( file$ ) = 0
	RunApp:( "data", file$ )
endif
UseFile:( file$ )
ToFront:

rem -- Search for keyword in Data
SendText:( "." + func$ )
Enter:
ENDP
