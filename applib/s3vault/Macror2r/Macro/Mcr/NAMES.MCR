PROC macro:
	REM I have this macro asigned to CTRL-SHIFT-N
	REM Written by Richard E. Rettke and placed in the Public Domain
	REM email: rrettke@execpc.com
	REM NAMES Insert frequently used names into text (any of 11 variants)
	REM
	Global ln$(75,17),fn$(75,17),sn$(75,17),i%,j%,ix$(75,17),fmt$(11,25)
	i%=0
	assign$:("Doe","John","Jane")
	assign$:("Limbaugh","Rush","Marna")
	assign$:("Rettke","Dick","Barbara")
	
	j%=0
	Do
		j%=j%+1
		ix$(j%)=Upper$(ln$(j%)+"."+Left$(fn$(j%),1))
	Until j%=i%

	UseApp:("active")
	dInit:("Insert Name")
	dChoiceA:(1,"Name",addr(ix$()),0)
	j%=Dialog:

	fmt$(1)=ix$(j%)																REM RETTKE.R
	fmt$(2)=Upper$(ln$(j%)+"."+Left$(sn$(j%),1))	REM RETTKE.B
	fmt$(3)=ln$(j%)+" "+fn$(j%)										REM Rettke Richard
	fmt$(4)=ln$(j%)+", "+fn$(j%)										REM Rettke, Richard
	fmt$(5)=ln$(j%)+" "+sn$(j%)										REM Rettke Barbara
	fmt$(6)=ln$(j%)+" "+fn$(j%)+"/"+sn$(j%)				REM Rettke Richard/Barbara
	fmt$(7)=fn$(j%)+" "+ln$(j%)										REM Richard Rettke
	fmt$(8)=sn$(j%)+" "+ln$(j%)										REM Barbara Rettke
	fmt$(9)=fn$(j%)+"/"+sn$(j%)+" "+ln$(j%)				REM Richard/Barbara Rettke
	fmt$(10)=upper$(ln$(j%)) 											REM RETTKE
	fmt$(11)=ln$(j%)															REM Rettke

	dInit:("Select Format")
	dChoiceA:(1,"Format",addr(fmt$()),i%)
	j%=Dialog:

	if j%
		PutText:(fmt$(j%))
		Right:
	endif
ENDP
	
Proc assign$:(lname$,fname$,sname$)
	i%=i%+1+(-(i%=27)) REM This statement automatically skips item 28
	If i%>75					 REM ** Change if array dimension changes **
		Useapp:("Active")
		dInit:("Error in 'Word'")
		dText:("","Too Many Items Defined, Increase array dimension",0)
		Dialog:
		Stop
	Endif
	ln$(i%)=lname$
	fn$(i%)=fname$
	sn$(i%)=sname$
Endp	Rem incr
