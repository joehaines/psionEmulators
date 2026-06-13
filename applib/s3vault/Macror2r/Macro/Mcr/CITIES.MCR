PROC macro:
	REM I have this macro assigned to CTRL-SHIFT-DIAMOND
	REM Written by Richard E. Rettke and placed in the Public Domain
	REM email: rrettke@execpc.com
	REM Text insert of frequently referenced cities
	REM To add cities Increase the 30 dimension also change the IF test
	REM in the ASSIGN$ proc
	Global city$(30,20),i%,j%
	i%=0
	assign$:("Appleton")
	assign$:("Beaver Dam")
	assign$:("Chicago")
	assign$:("Fond du Lac")
	assign$:("Fort Atkinson")
	assign$:("Grafton")
	assign$:("Green Bay")
	assign$:("Janesville")
	assign$:("Kaukauna")
	assign$:("Kenosha")
	assign$:("Madison")
	assign$:("Marinette")
	assign$:("Marshfield")
	assign$:("McFarland")
	assign$:("Menasha")
	assign$:("Milwaukee")
	assign$:("Minneapolis/St. Paul")
	assign$:("Mukwanago")
	assign$:("Neenah")
	assign$:("Oshkosh")
	assign$:("Racine")
	assign$:("Seymour")
	assign$:("Stevens Point")
	assign$:("Sun Prarie")
	assign$:("Watertown")
	assign$:("Wausau")

	UseApp:("active")
	dInit:("Insert City")
	dChoiceA:(1,"City",addr(city$(1)),i%)
	j%=Dialog:
	if j%
		PutText:(city$(j%))
		Right:
	endif
ENDP
	
Proc assign$:(xyzzy$)
	i%=i%+1+(-(i%=27)) REM This statement automatically skips item 28
	If i%>30 REM change if city$ array dimension changes
		Useapp:("Active")
		dInit:("Error in 'Word'")
		dText:("","Too Many Items Defined, Increase array dimension",0)
		Dialog:
		Stop
	Endif
	city$(i%)=xyzzy$	
Endp	Rem assign
