rem -- This macro demonstrates various kinds of remote dialogs

PROC macro:
local d%
UseApp:( "active" )
while 1
	dInit:( "dText dialog example" )
	dText:( "", "dChoice example", $400 )
	dText:( "", "dChoiceA example", $400 )
	dText:( "", "dButtons example", $400 )
	dText:( "", "dEdit example", $400 )
	d% = Dialog:
	if d% = 0
		return
	endif
	@( "dialog" + gen$( d%, 1 ) ):
endwh
ENDP

PROC dialog2:
local d%
dInit:( "dChoice dialog example" )
dChoice:( 1, "Choose a number", "One,Two,Three,Four,Five,Six,Seven,Eight,Nine" )
d% = Dialog:
if d%
	GiPrint:( "Your choice was " + gen$( d%, 4 ) )
endif
ENDP

PROC dialog3:
local d%, color$(3,5)
color$(1) = "Red"
color$(2) = "Green"
color$(3) = "Blue"
dInit:( "dChoiceA dialog example" )
dChoiceA:( 1, "Choose a color", addr( color$() ), 0 )
d% = Dialog:
if d%
	GiPrint:( "Your choice was " + color$( d% ) )
endif
ENDP

PROC dialog4:
local d%
dInit:( "dButtons dialog example" )
dButtons:( "A,%a,B,%b,C,%c,D,%d,E,%e,F,%f" )
d% = Dialog:
if d%
	GiPrint:( "Your choice was " + chr$( 64 + d% ) )
endif
ENDP

PROC dialog5:
local str$(40)
str$ = "<your name here>"
dInit:( "dEdit dialog example" )
dText:( "", "Please enter your name below", 2 )
dEdit:( addr( str$ ), "Name", 15 )
if Dialog:
	GiPrint:( "Hi, " + str$ + "!" )
endif
dENDP
