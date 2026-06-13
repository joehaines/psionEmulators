rem JUMPWEEK
rem macro for jumping to specific week
rem in agenda. Please swap the month
rem and day string when using date
rem format mm/dd/yy !
rem Richard Schmidt rschmidt@worldonline.nl
rem Advise : assign to Control-j or w

PROC Macro:
	LOCAL wk$(25),w&,seconds&
	LOCAL y%,m%,d%,x%,day$(2),mon$(2)

	UseApp:( "active" )

	rem Query week number
	dInit:("Jump week") 
	dEdit:(ADDR(wk$), "Week #", 2)
	IF Dialog:

		rem calculate new date
		w&=VAL(wk$)-1
		IF WEEK(1,1,YEAR)=52
			w&=w&+1
		ENDIF
		seconds&=DATETOSECS(YEAR,1,1,0,0,0)+(w&*604800)
		SECSTODATE seconds&, y%,m%,d%,x%,x%,x%,x%

		day$=GEN$(d%,2)
		IF d%<10
			day$="0"+day$
		ENDIF

		mon$=GEN$(m%,2)
		IF m%<10
			mon$="0"+mon$
		ENDIF

		rem Jump to according date
		rem Swap day$ and mon$ when using
		rem date format mm/dd/yy
		Psion:( %j )
		SendText:( day$ )
		SendText:( mon$ )
		Enter:

	ENDIF
ENDP