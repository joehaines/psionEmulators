rem -- This macro is a sample System Screen installer
rem -- Provided ONLY as an example
rem -- Do NOT use without making modifications

PROC macro:

rem -- Use the System Screen as the current app
UseApp:( "System" )

rem -- Decrease the zoom level
Psion:( %Z )

rem -- Show the memory bar on the bottom
Diamond:

rem -- Make status window narrow
CtrlDn:
nMenu:(2)
CtrlUp:

rem -- Set preferences: Use Enter to open multiple files
Psion:( %q )
Down:
Right:
Enter:

rem -- Auto switch-off if no external power
Psion:( %o )
Key:( %i )
Enter:

rem -- Set sound options
SetSound:( 1, 1, 1, 0 )

rem -- Jump to far left
Home:

rem -- Replace icons for Agenda, Time, World and Calc
rem -- with group "Main"
nRight:(2)
remove:(4)
install:( "main", "current" )

rem -- Replace icons for Record and Program
rem -- with group "Opl"
nRight:(2)
remove:(2)
install:( "opl", "current" )

rem -- Assign Control-Data to the leftmost icon inside the group "Opl"
Space:
Home:
hkey:( "D", "C" )
Space:

rem -- Install groups "Tools", "Tools2" and "Games"
install:( "games", "last" )
install:( "tools2", "current" )
install:( "tools", "current" )

rem -- Assign Control-Sheet to the rightmost icon inside the group "Tools"
Space:
End:
hkey:( "S", "C" )
Space:

rem -- Run standard applications
if Fil2Pid%:( "macro.mdb" ) = 0
	RunApp:( "macro", "macro" )
endif
if App2Pid%:( "calc" ) = 0
	RunAppB:( "calc", "" )
endif
if Fil2Pid%:( "agenda.agn" ) = 0
	RunAppB:( "agenda", "agenda" )
endif
if App2Pid%:( "finder" ) = 0
	RunApp:( "finder", "" )
endif
ENDP

rem -- This procedure removes the selected icon
rem -- Repeats n% times
PROC remove:( n% )
local i%
i% = n%
while i%
	i% = i% - 1
	Psion:( %/ )
	Yes:
endwh
ENDP

rem -- This procedure installs an application app$ from the
rem -- default drive. Position ("first", "last" or "current") is
rem -- specified in pos$
PROC install:( app$, pos$ )
Psion:( %i )
SendText:( app$ )
Up:
Key:( asc( pos$ ) )
Enter:
ENDP

rem -- This procedure assigns an application button
rem -- k$ specifies which button to assign
rem -- c$ specifies modifier (control = "C" or none = "N")
PROC hkey:( k$, c$ )
local pos%
Psion:( %a )
pos% = loc( "DWATOCS", k$ )
if pos% > 1
	nRight:( pos% - 1 )
endif
Down:
Key:( asc( c$ ) )
Enter:
ENDP
