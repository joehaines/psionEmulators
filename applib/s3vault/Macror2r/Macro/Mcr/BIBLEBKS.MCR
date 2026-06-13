PROC macro:
REM I have this macro assigned to CTRL-SHIFT-B
REM Insert Books of the Bible (fullname or Abbreviation)
REM Written 28Jul95 by Richard E. Rettke
REM Copr. 1995 Rettke Resources, all commercial rights reserved

	Global ix$(67,24),book$(67,16),bk$(67,5),fmt$(2,16),i%,j%
	i%=0

REM old testament
	assign$:("Genesis ","Ge ")
	assign$:("Exodus ","Ex ")
	assign$:("Leviticus ","Lev ")
	assign$:("Numbers ","Nu ")
	assign$:("Deuteronomy ","Dt ")
	assign$:("Joshua ","Jos ")
	assign$:("Judges ","Jdg ")
	assign$:("Ruth ","Ru ")
	assign$:("1 Samuel ","1Sa ")
	assign$:("2 Samuel ","2Sa ")
	assign$:("1 Kings ","1Ki ")
	assign$:("2 Kings ","2Ki ")

	assign$:("1 Chronicals ","1Ch ")
	assign$:("2 Chronicals ","2Ch ")
	assign$:("Ezra ","Ezr ")
	assign$:("Nehemiah ","Ne ")
	assign$:("Esther ","Est ")
	assign$:("Job ","Job ")
	assign$:("Psalms ","Ps ")
	assign$:("Proverbs ","Pr ")
	assign$:("Ecclesiastes ","Ecc ")
	assign$:("Song of Songs ","SS ")
	assign$:("Isaiah ","Isa ")
	assign$:("Jerimiah ","Jer ")
	assign$:("Lamentations ","La")
	assign$:("Ezekiel ","Eze ")
	assign$:("Daniel ","Da ")
	assign$:("Hosea ","Hos ")
	assign$:("Joel ","Joel ")
	assign$:("Amos ","Am ")
	assign$:("Obadiah ","Ob ")
	assign$:("Jonah ","Jnh ")
	assign$:("Micah ","Mic ")
	assign$:("Nahum ","Na ")
	assign$:("Habakkuk ","Hab ")
	assign$:("Zephaniah ","Zep ")
	assign$:("Haggai ","Hag ")
	assign$:("Zechariah ","Zec ")
	assign$:("Malachi ","Mal ")

REM new testament
	assign$:("Matthew ","Mt ")
	assign$:("Mark ","Mk ")
	assign$:("Luke ","Lk ")
	assign$:("John ","Jn ")
	assign$:("Acts ","Ac ")
	assign$:("Romans ","Ro ")
	assign$:("1 Corinthians ","1Co ")
	assign$:("2 Corinthians ","2Co ")
	assign$:("Galations ","Gal ")
	assign$:("Ephesians ","Eph ")
	assign$:("Philippians ","Php ")
	assign$:("Colossians ","Col ")
	assign$:("1 Thessalonians ","1Th ")
	assign$:("2 Thessalonians ","2Th ")
	assign$:("1 Timothy ","1Ti ")
	assign$:("2 Timothy ","2Ti ")
	assign$:("Titus ","Tit ")
	assign$:("Philemon ","Phm ")
	assign$:("Hebrews ","Heb ")
	assign$:("James ","Jas ")
	assign$:("1 Peter ","1Pe ")
	assign$:("2 Peter ","2Pe ")
	assign$:("1 John ","1Jn ")
	assign$:("2 John ","2Jn ")
	assign$:("3 John ","3Jn ")
	assign$:("Jude ","Jude ")
	assign$:("Revelations ","Rev ")

	j%=0
	Do
		j%=j%+1
		ix$(j%)=book$(j%)+" ("+bk$(j%)+")"
	Until j%=i%

	UseApp:("active")
	dInit:("Insert Name")
	dChoiceA:(1,"Books of the Bible",addr(ix$()),i%)
	j%=Dialog:

	fmt$(1)=book$(j%)
	fmt$(2)=bk$(j%)

	dInit:( "Select Format" )
	dButtons:("Fullname,32,Abbreviation,13") 
	j% = Dialog:
	if j%
		PutText:(fmt$(j%))
		Right:

	endif
ENDP
	
Proc assign$:(a$,b$)
	i%=i%+1+(-(i%=27)) REM This statement automatically skips item 28
	If i%>67
		Useapp:("Active")
		dInit:("Error in 'Word'")

		dText:("","Too Many Items Defined, Increase array dimension",0)
		Dialog:
		Stop
	Endif
	book$(i%)=a$
	bk$(i%)=b$
Endp	Rem assign
