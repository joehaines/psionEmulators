PROC macro:
REM I have this macro assigned to CTRL-SHIFT-DASH
REM Written by Richard E. Rettke and placed in the Public Domain
REM email: rrettke@execpc.com
REM Insert frequently used text

	Global word$(50,30),i%,j%,k%,dd$(2),mmm$(3),yy$(2),hh$(2),mm$(2),mo$(2)
	i%=0
	dd$=Right$("0"+Num$(day,2),2)
	mmm$=month$(month)
	mo$=Right$("0"+Num$(month,2),2)
	yy$=Right$(Num$(year,4),2)
	assign$:(dd$+mmm$+yy$)
	hh$=Right$("0"+Num$(Hour,2),2)
	mm$=Right$("0"+Num$(Minute,2),2)
	assign$:(hh$+":"+mm$)
	assign$:(dd$+mmm$+yy$+" "+hh$+":"+mm$+" ")
	assign$:(yy$+mo$+dd$)
	assign$:("MB 400E")
	assign$:("BJR RER")
	assign$:("CS: ")
	assign$:("Call: ")
	assign$:("Diamonte")
	assign$:("DD Meeting")
	assign$:("Family Matters:")
	assign$:("FUP: ")
	assign$:("LV _ _m UA flt _, AR _ _m")
	assign$:("MaM: ")	REM Meeting after the Meeting
	assign$:("N&B: ")
	assign$:("Perkins on 47")
	assign$:("Perkins ATW")

	assign$:("Parking")
	assign$:("PM:")
	assign$:("RER")
	assign$:("STP PP: ")
	assign$:("STP HP: ")
	assign$:("STP VP: ")
	assign$:("SUP: ")
	assign$:("Travel (record mileage)")
	assign$:("Travel")
	assign$:("TENTATIVE")
	assign$:("=Date("+yy$+","+num$(month,2)+","+num$(day,2)+")")
	assign$:(left$(datim$,6)+mid$(datim$,8,3)+mid$(datim$,14,8))
	assign$:(left$(datim$,6)+mid$(datim$,8,3)+mid$(datim$,14,2))

	UseApp:("active")
	dInit:("Insert Word ("+num$(i%,3)+" items to select)")
	dChoiceA:(1,"Word",addr(word$(1)),i%)
	j%=Dialog:
	if j%
		PutText:(word$(j%))
		Right:
	endif
ENDP
	
Proc assign$:(xyzzy$)
	i%=i%+1+(-(i%=27)) REM This statement automatically skips item 28
	If i%>50						REM *** Change if WORD$ dimension changes
		Useapp:("Active")
		dInit:("Error in 'Word'")
		dText:("","Too Many Items Defined, Increase array dimension",0)
		Dialog:
		Stop
	Endif
	word$(i%)=xyzzy$	
Endp	Rem assign
