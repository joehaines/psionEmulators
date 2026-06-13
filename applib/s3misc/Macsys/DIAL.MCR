rem -- This macro allows to use the standard Free-form dialing
rem -- dialog to dial alphanumeric phone numbers. Try assigning
rem -- it to Control-Dial (Control-Psion-Help)

PROC macro:
cache on
UseApp:( "active" )
if Info:(4) = 0
	UseApp:( "time" )
	ToFront:
endif
SendKey:( 291, 12, 1 ) rem Control-Dial
keys:(1)
var%(3) = CurrPid%:
ENDP

PROC keys:( state% )
local i%
rem Capture all alphabetical keys and Esc
while i% < 26
	Hotkey:( %a + i%, 0, "convert", state% )
	i% = i% + 1
endwh
Hotkey:( esc%, 0, "convert", state% )
ENDP

PROC convert:( k%, m% )
local pos%
UsePid:( var%(3) )
if k% = esc%
	Esc:
	keys:(0)
else
	pos% = loc( "ABCDEFGHIJKLMNOPRSTUVWXY", chr$( k% ) )
	if pos%
		Key:( %1 + ( pos% + 2 ) / 3 )
	endif
endif
ENDP
