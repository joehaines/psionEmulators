PROC macro:
local symbol$(255,1), i%

i% = 1
while i% < 256
	symbol$( i% ) = chr$( i% )
	i% = i% + 1
endwh

UseApp:( "active" )
dInit:( "Insert symbol" )
dChoiceA:( 1, "Symbol", addr( symbol$(1) ), 255 )
i% = Dialog:
if i%
	PutText:( symbol$( i% ) )
endif
ENDP
