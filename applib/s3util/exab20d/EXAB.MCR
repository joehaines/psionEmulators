rem ÉÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ»
rem º    ExAb - ¸Huub Linthorst, 96-97    º
rem º    Macro to EXtend ABbreviations    º
rem º E-mail: linthors@chem.leidenuniv.nl º
rem ÈÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¼

PROC macro:
global a$(8),b$(128),b1$(128),c$(1),d$(12),e$(80),f$(25),g$(1),h$(2),i$(15),j$(30),k$(2),l$(255),m$(3),n$(128),o$(2),r$(2),s$(255),t$(8),v$(38),w$(38),q$(3),ex$(8,8),a%,f%,g%,i%,l%,m%,r%,t%,x%(8),z%,ex%
v$="ExAb (2.0d) Ä ¸Huub Linthorst, 96-98"
useapp:("active")
if info:(4)=0 or currpid%:=app2pid%:("System") or currpid%:=app2pid%:("World") or currpid%:=app2pid%:("Time") or currpid%:=app2pid%:("Calc")
	goto x::
endif
var%(4)=actpid%:
a$=upper$(pid2app$:(currpid%:))
p1:
s$=""

w$=rept$(" ",6)+"ExAb"
if exist ("m:\opd\exab.ini")
	open "m:\opd\exab.ini",C,k$,l$
	first
	w$=C.l$
	if w$<>v$
		close
		goto y::
	endif
	next
	b$=C.l$
	next
	c$=C.l$
	next
	r$=C.l$
	next
	h$=C.l$
	next
	k$=C.l$
	next
	g%=val(C.l$)
	next
	m%=val(C.l$)
	next
	var%(1)=val(C.l$)
	next
	f%=val(C.l$)
	next
	o$=C.l$
	next
	x%(7)=val(C.l$)
	next
	ex$(4)=C.l$
	next
	ex$(5)=C.l$
	next
	ex$(6)=C.l$
	next
	ex$(7)=C.l$
	next
	ex$(8)=C.l$
	next
	g$=C.l$
	close
else
y::
	dinit:("Update: "+mid$(w$,7,4)+" ÄÄ "+mid$(v$,7,4))
	dtext:("","For the latest information on "+mid$(v$,1,11)+",",0)
	dtext:("","please read EXAB.TXT, included in the ZIP file,",$200)
	dtext:("","ExAb needs to update your configuration",2)
	dbuttons:("Update,13,Quit,27")
	r%=dialog:
	if r%=2
		goto x::
	endif
	b$="LOC::M:\DAT\EXAB.EXB"
	n$="EXAB"
	c$="+"
	r$="/"
	h$=">"
	k$="~"
	g%=1
	m%=1
	var%(1)=1
	f%=1
	o$="^"
	x%(7)=1
	ex$(4)="NDcode"
	ex$(5)="Look4it"
	g$="&"
	p5:
	goto x::
endif

parse$(b$,"",x%())
n$=mid$(b$,x%(4),loc(b$,".")-x%(4))
t$=mid$(datim$,17,8)
if m%=1
	d$=gen$(day,2)+"-"+gen$(month,2)+"-"+gen$(year,4)
elseif m%=2
	d$=gen$(month,2)+"/"+gen$(day,2)+"/"+gen$(year,4)
elseif m%=3
	d$=gen$(day,2)+"."+gen$(month,2)+"."+gen$(year,4)
elseif m%=4
	d$=mid$(datim$,5,11)
elseif m%=5
	d$=month$(month)+" "+gen$(day,2)+", "+gen$(year,4)
endif

if testsrv%:=0
	if gettext$:<>""
		if a$="SHEET"
			goto b::
		endif
		l$=gettext$:
		if len(l$)>254
			beep:(5,700)
			giprint:("String too long...")
			goto x::
		elseif len(l$)>20
			q$="..."
		endif
		if x%(7)=1
			opl:
			if x%(7)=0
				goto x::
			endif
		endif
a::
		dinit:("Enter ExAb abbreviation for '"+left$(l$,20)+q$+"'")
		dtext:("","",0)
		dedit:(addr(s$),"Abbreviation",20)
		dtext:("","",$200)
		dtext:("","Database: "+n$,2)
		if dialog:
			s$=left$(s$,10)
			if s$=""
				goto x::
			endif
			open b$,D,k$,l$
			while not eof
				if s$<>D.k$
					next
				else
					beep:(5,700)
					giprint:("Abbreviation '"+s$+"' already exists...")
					close
					goto a::
				endif
			endwh
			if eof
				D.k$=s$
				D.l$=l$
				append
				giprint:("Definition "+num$(pos,3)+" appended...")
			endif
			close
			goto x::
		endif
		goto x::
	endif
endif

b::
if a$<>"SHEET"
	sendkey:(left%,ctrl%+shift%,1)
endif
p1:
if x%(7)=1
	ex%=1
	if left$(s$,3)=">>>" and right$(s$,3)="<<<"
		ex$(1)="NDcode"
	elseif right$(s$,1)=":"
		ex$(1)="Look4it"
	else
		goto m::
	endif 
	opl2:
	goto x::
endif
m::
if s$=""
	gettext$:
	goto x::
elseif s$=c$+c$
	delete:
	a%=1
	giprint:("ExAb will search whole text...")
	p2:
else 
	p3:
endif
x::
endp

proc opl:
ex$(1)="Upper"
ex$(2)="Lower"
ex$(3)="ExAb"
dinit:("")
dchoicea:(3,"Plug-in",addr(ex$()),0)
ex%=dialog:
if ex%=3
	return
elseif ex%=1
	delete:
	puttext:(upper$(l$))
	right:
elseif ex%=2
	delete:
	puttext:(lower$(l$))
	right:
elseif ex%>3
	opl2:
endif
x%(7)=0
endp

proc opl2:
loadm "m:\macro\mco\"+ex$(ex%)+".mco"
s$=""
v$="ExAb Ä "+ex$(ex%)
hlstart:
unloadm "m:\macro\mco\"+ex$(ex%)+".mco"
x%(7)=0
endp

proc ini:
tofront:
if exist ("m:\opd\exab.ini")
	delete "m:\opd\exab.ini"
endif
create "m:\opd\exab.ini",C,k$,l$
C.l$=v$ :c.k$="1" :append
C.l$=upper$(b$) :i2:
C.l$=c$ :i2:
C.l$=r$ :i2:
C.l$=h$ :i2:
C.l$=k$ :i2:
C.l$=gen$(g%,1) :i2:
C.l$=gen$(m%,1) :i2:
C.l$=gen$(var%(1),1) :i2:
C.l$=gen$(f%,1) :i2:
C.l$=o$ :i2:
C.l$=gen$(x%(7),1) :i2:
C.l$=ex$(4) :i2:
C.l$=ex$(5) :i2:
C.l$=ex$(6) :i2:
C.l$=ex$(7) :i2:
C.l$=ex$(8) :i2:
C.l$=g$ :i2:
r%=pos+5
do
	C.l$="" :i2:
until pos=r%
close
p6:
if t$=""
	close
	macro:
else
	return
endif
x::
endp

proc i2:
c.k$=gen$(pos+1,2) :append
endp

proc p3:
if exist (b$)
	trap open b$,D,k$,l$
	if err
		beep:(5,700)
		dinit:("Error: "+err$(err))
		dtext:("","Database "+n$+" may be in use by other program.",2)
		dtext:("","Do you want ExAb to try close the database?",2)
		dbuttons:("Yes,13,No,27")
		dtext:("","",$200)
		dtext:("","If ExAb cannot close the database, you must close",2)
		dtext:("","and reopen MACRO before you can use ExAb.",2)
		r%=dialog:
		if r%=1
			usefile:(b$)
			exit:
			useapp:(a$)
			giprint:(n$+" closed...")
			p3:
		else
			right:
		endif
	endif
	last
	p4:
else
	tomacro:
	beep 6,500
	giprint "Database "+n$+" not found...",0
	p5:
	close
endif
endp

proc getchar:(k%,n%)
var%(3)=actpid%:
hotkey:(632,8,"getchar",0)
if var%(2)<>var%(3)
	goto q::
endif
usepid:(var%(4))
tofront:
if var%(1)=2
	psion:(%j)
else
	psion:(%b)
endif
q::
usepid:(var%(3))
var%(3)=0
exit:
endp

proc p4:
if asc(s$)=46 or lower$(right$(s$,4))=".dbf" or lower$(right$(s$,4))=".wrd" or lower$(right$(s$,4))=".agn" or lower$(right$(s$,4))=".spr"
	if z%<>1
		delete:
	endif
	if s$=".p"
		p101:
		goto z::
	elseif s$=".c"
		if exist ("loc::m:\app\charm.opa") or exist ("loc::a:\app\charm.opa") or exist ("loc::b:\app\charm.opa")
			l$="Charm"
			goto a::
		else
				beep:(5,700)
				giprint:("CHARM program not found...")
				goto z::
		endif
	elseif s$=".?"
		p10:
		if r%=0
			goto z::
		else
			z%=1
		endif
		p4:
	elseif s$=".d"
		puttext:(d$)
		goto x::
	elseif s$=".t"
		puttext:(t$)
		goto x::
	else
		if loc(s$,":")>0
			e$=left$(s$,loc(s$,":")-1)
			dinit:("Remove link?")
			dtext:("",s$,2)
			dbuttons:("Keep,13,Remove,8")
			r%=dialog:
			if r%=0
				goto z::
			elseif r%=1
				puttext:(s$)
				right:
			endif
			s$=mid$(s$,loc(s$,":")+1,len(s$))
		endif
		l$=lower$(right$(s$,3))
		if l$="dbf" or s$=".b"
			l$="Data" :f$="dat" :i$="dbf"
		elseif l$="wrd" or s$=".w"
			l$="Word" :f$="wrd" :i$="wrd"
		elseif l$="agn" or s$=".a"
			l$="Agenda" :f$="agn" :i$="agn"
		elseif l$="spr" or s$=".s"
			l$="Sheet" :f$="spr" :i$="spr"
		else
			l$=""
			goto c::
		endif
		if left$(s$,1)="."
			busy off
			tomacro:
			r%=8
			j$="Select"
b::
			s$="m:\"+f$+"\*."+i$
			dinit "Use "+l$
			dfile s$,"",r%
			if r%=19
				dbuttons j$,13,"Quit",27,"Select existing",290
				goto d::
			elseif r%=8
				dedit e$,"Find text",15
				dbuttons j$,13,"Quit",27,"Create new",290
d::
				r%=dialog
				if r%=13
					goto a::
				elseif r%=290
					if j$="Select"
						r%=19
						j$="Create"
					elseif j$="Create"
						r%=8
						j$="Select"
					endif
					goto b::
				endif
				goto z::
			endif
		else
			if e$="" and f%=1
				dinit:("Find text in "+l$+" file "+upper$(s$))
				dedit:(addr(e$),"Find",20)
				if dialog:
				else
					goto z::
				endif
			endif
		endif
a::
		tofront:
		giprint:("Using "+upper$(l$)+"...")
		close
		runappc:(l$,s$)
		hotkey:(632,8,"getchar",1)
		var%(2)=actpid%:
		if e$<>"" and j$<>"Create"
			if l$<>"Data"
				sendkey:(260,12,1)
				psion:(%f)
			endif
			sendtext:(e$+chr$(13))
		endif
	endif
elseif s$="exab"
	if z%<>1
		delete:
	endif
	exab:
	goto x::
endif
c::
if len(s$)>10
	i%=1
	s$=left$(s$,10)
endif
position 1
while (not eof) and (s$<>D.k$)
	next
endwh
if s$=D.k$
	if z%<>1
		delete:
	endif
	p7:
elseif (eof) and s$<>D.k$
	if pos>201
		beep:(5,700)
		giprint:("Maximum number reached...")
		goto z::
	endif
	f$="1"
	p8:
	if r%<>0 and r%<>pos
		p4:
		goto z::
	elseif r%=0
		if a%<>0
			goto x::
		endif
		goto z::
	endif
	dinit:("Enter full text for '"+s$+"'")
	dtext:("","",0)
	dedit:(addr(l$),s$,18)
	dtext:("","",$200)
	if i%=1
		beep:(5,700)
		giprint:("Abbreviations 10 characters max...")
	endif
	dtext:("","LF="+r$+"³Tab="+o$+"³item="+g$+"³cursor="+k$+"³date=.d"+h$+"³time=.t"+h$,$202)
	dtext:("","Database: "+n$,2)
	r%=dialog:
	if r%=1
		if l$=""
			if a%=0
				delete:
			endif
			goto x::
		else
			D.l$=l$
			D.k$=s$
			append
			giprint:("Record no. "+num$(pos,3)+" appended...")
			tofront:
			delete:
			p7:
		endif
	elseif a%<>0
		goto x::
	else
		goto z::
	endif
endif
t%=t%+1
if right$(l$,len(h$))=h$
	right:
	delete:
	sendkey:(left%,shift%+ctrl%,1)
	p1:
	p4:
endif
x::
i%=0
right:
if a%<>0
	if r%=1 and l$=""
		right:
		r%=0
		goto y::
	endif
	sendkey:(127,2,1)
y::
	a%=2
	p2:
endif
z::
close
tofront:
endp

proc p2:
if a%=2
	psion:(%g)
	goto y::
endif
psion:(%f)
sendtext:(c$)
enter:
y::
p1:
if s$=c$
	sendkey:(left%,ctrl%+shift%,1)
else
	a%=0
	goto x::
endif
p1:
l$=""
if a%=1
	p3:
elseif a%=2
	p4:
endif
x::
close
beep:(5,700)
beep:(5,500)
giprint:("ExAb finished extending "+num$(t%,4)+" abbreviations")
endp

proc p1:
if srvpid%:<>currpid%:
	tomacro:
	tofront:
endif
if testsrv%:=0
	if g%=1
		s$=gettext$:
	else
		s$=lower$(gettext$:)
	endif
endif
endp

proc p6:
giprint:("EXAB.INI updated...")
if not exist (b$)
	create b$,D,k$,l$
else
	open b$,D,k$,l$
endif
s$=""
endp

proc exab:
use D
d.l$=r$+"ÄÄÄÄÄ "+v$+" ÄÄÄÄÄ"+r$+r$+"A Macro to EXtend ABbreviations"+r$+r$+"E-Mail me for comments and suggestions at linthors@chem.leidenuniv.nl"+r$+r$
p7:
d.l$="ExAb HELP"+r$+r$+"This is how ExAb works. Type an abbreviation, e.g. 'sy'. With the cursor behind 'sy', hit ExAb's hot-key (I use <Psion-Enter>). The macro will search your EXAB database for 'sy'. "
p7:
d.l$="If found, ExAb will replace 'sy' in your text with the corresponding full text. If not found, ExAb will present an input screen to enable you to add 'Sincerely Yours,' or 'Since Yesterday' to the database. "
p7:
d.l$="Alternatively, you may enter an abbreviation for a highlited string of text after exabbing on it."+r$+r$+"ExAb can search and replace all abbreviations of a text in one go. Therefore, type a '"+c$+"' at the end of each "
p7:
d.l$="abbreviation. After finishing, type '"+c$+c$+"' at the start of the text followed by the hot-key."+r$+r$
p7:
d.l$="'Exabbing' on special codes '.t' and '.d' will stamp the time ( .t"+h$+" ) and date ( .d"+h$+" ), respectively, while hot-keying on '.c' opens Tom Dolbilin's Charm, enabling you to insert special characters in your document. "
p7:
d.l$="ExAb can also open files in Psion's programs Data (.b), Word (.w), Agenda (.a), and Sheet (.s) and bring selected text from these files into your document. Special code '.?' opens ExAb's Info menu for quick reference "
p7:
d.l$="and access to ExAb's options."+r$+r$+"The ExAb databases (extension .EXB) are automatically created. The data that you'll add during, a hopefully enjoyable, use can be modified using the options in the Info menu (.?)."+r$+r$
p7:
d.l$="Now, try ExAb yourself with sy"+k$+r$+r$+"ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ"+r$+r$
p7:
close
endp

proc p7:
local n%,m$(20,60)
l$=d.l$
if loc(l$,g$)>0
	l$=mid$(l$,loc(l$,g$)+1,255)
	r%=loc(l$,g$)
	n%=1
	while r%<>0
		m$(n%)=mid$(l$,1,r%-1)
		l$=mid$(l$,r%+1,255)
		r%=loc(l$,g$)
		n%=n%+1
	endwh
	m$(n%)=l$
	dinit:("Items '"+s$+"'")
	dchoicea:(1,"Paste",addr(m$()),n%)
	r%=dialog:
	if r%
		l$=m$(r%)
	else
		l$=""
		s$=""
		return
	endif
endif

i$=r$
j$=chr$(10)
p71:
i$=".d"+h$
j$=d$
p71:
i$=".t"+h$
j$=t$
p71:
i$=o$
j$=chr$(9)
p71:
if loc(l$,k$)<>0
	p72:
endif
puttext:(l$)
return
endp

proc p71:
while loc(l$,i$)<>0
	l$=left$(l$,(loc(l$,i$)-1))+j$+right$(l$,len(l$)-loc(l$,i$)-len(i$)+1)
endwh
endp

proc p72:
if loc(l$,k$)<>0
	puttext:(right$(l$,len(l$)-loc(l$,k$)-len(k$)+1))
left:
	l$=(left$(l$,(loc(l$,k$)-1)))
endif
endp


proc p8:
local m1$(201,10),m$(201,40)
giprint:("ExAb loading definitions...")
position 1
while not eof
	m1$(pos)=D.k$
	q$=""
	if len(d.l$)>20
		q$="..."
	endif
	m$(pos)=m1$(pos)+"     "+left$(d.l$,20)+q$
	next
endwh
if len(f$)>0
	m$(pos)="<New>"
	if a%<>0
		beep:(5,700)
	endif
	dinit:("")
	dtext:("","Abbreviation '"+s$+"' not found",2)
	dtext:("","Paste existing or input new extension",$202)
	dtext:("","",0)
	dchoicea:(pos,"",addr(m$()),pos)
	dtext:("","",$202)
else 
	dinit:("Paste ExAb definition ("+num$(pos-1,3)+")")
	dtext:("","",0)
	dchoicea:(1,"",addr(m$()),pos-1)
	dtext:("","",$202)
endif
dtext:("","Database: "+n$,2)
r%=dialog: 
if r%<>0 and r%<>pos
	s$=m1$(r%)
endif
endp

proc p10:
dinit:(v$)
dtext:("","Database: "+n$,2)
dbuttons:("List,%l,Codes,%c,Open DB,%o,Sort DB,%s,Settings,%q,Info,%i")
r%=dialog:
if r%=1
	p8:
	return
elseif r%=3
	close
	giprint:("Launching Data...")
	runappc:("Data",b$)
	hotkey:(632,8,"getchar",1)
	var%(2)=actpid%:
elseif r%=4
	p9:
elseif r%=2
c::
	dinit:("ExAb special codes")
	dtext:(".?","Info menu",$400)
	dtext:(".c","Insert character (Charm)",$400)
	dtext:(".d","Stamp date ("+d$+")",$400)
	dtext:(".t","Stamp time ("+t$+")",$400)
	dtext:(".p","Use program Ä",$600)
	dtext:("exab","ExAb introduction",$400)
	r%=dialog:
	if r%=3
		s$=".c"
	elseif r%=4
		s$=".d"
	elseif r%=5
		s$=".t"
	elseif r%=2
		s$=".?"
	elseif r%=6
		s$=".p"
	elseif r%=7
		s$="exab"
	endif
	return
elseif r%=5
	close
	p5:
elseif r%=6
	dinit:(v$)
	dtext:("","ExAb uses code of Tom Dolbilin and supports CHARM (¸TD).",202)
	dtext:("","When you like ExAb and wish to receive free updates",2)
	dtext:("","by e-mail, have your name added to the ExAb user list.",$202)
	dtext:("","ExAb is also available for the Psion S5",2)
	dtext:("","Email me at linthors@chem.leidenuniv.nl",2)
	dialog:
endif
close
tofront:
s$=""
r%=0
endp

proc p101:
dinit:("Use program")
dtext:(".b","Data",$400)
dtext:(".w","Word",$400)
dtext:(".a","Agenda",$400)
dtext:(".s","Sheet",$400)
r%=dialog:
if r%=2
	s$=".b"
elseif r%=3
	s$=".w"
elseif r%=4
	s$=".a"
elseif r%=5
	s$=".s"
else
	goto x::
endif
z%=1
p4:
x::
endp

proc p5:
busy off
tomacro:
a::
r%=1
if l%=2
	goto c::
endif
e::
dinit "Exab configuration - 1"
dchoice r%,"Change database (now "+n$+")","No,Yes"
dchoice l%,"Change indicators ("+c$+" "+r$+" "+o$+" "+k$+" "+h$+" "+g$+")","No,Yes"
dchoice m%,"Date format","dd-mm-yyyy,mm/dd/yyyy,dd.mm.yyyy,dd Mon yyyy,Mon dd yyyy"
dchoice g%,"Case sensitivity","On,Off"
dbuttons "Update",13,"More",290,"Quit",27
a%=dialog
if a%=13
	if r%=2
		f$="Select"
		a%=8
		goto b::
	else
		if l%=2
c::
			dinit
			dedit c$,"Whole text search",1
			dedit r$,"Linefeed",2
			dedit o$,"Tab",2
			dedit k$,"Cursor positioning",2
			dedit h$,"Extension chaining",2
			dedit g$,"Items separator",1
			dbuttons "Confirm",13,"Quit",27
			dialog
			if c$=h$ or c$=r$ or c$=o$ or c$=k$ or r$=o$ or r$=k$ or r$=h$ or o$=k$ or o$=h$ or k$=h$ or c$=g$ or r$=g$ or o$=g$ or k$=g$ or h$=g$
				beep 6,700
				giprint "ExAb indicators must be different...",0
				goto c::
			endif
			l%=1
			goto a::
		endif
		ini:
		return
	endif
elseif a%=290
	dinit "Exab configuration - 2"
	dchoice f%,"Show Find dialog","Yes,No"
	dchoice var%(1),"This Psion speaks","English,German,French,Dutch,Swahili"
	if x%(7)<>1
		dchoice x%(7),"Plug-in support","Yes,No"
	else
		dchoice x%(8),"Plug-in","Support,Change plug-ins,Skip support"
	endif
	dbuttons "Continue",13,"Quit",27
	if dialog
		if x%(8)=3
			x%(7)=2
		elseif x%(8)=2 or x%(8)=0 and x%(7)=1
			dinit "Plug-ins"
			dedit ex$(4),"1",8
			dedit ex$(5),"2",8
			dedit ex$(6),"3",8
			dedit ex$(7),"4",8
			dedit ex$(8),"5",8
			dbuttons "Continue",13,"Quit",27
			dialog
		endif
	endif
	x%(8)=0
	goto e:: 
else
	tofront:
	return
endif
b::
b1$="m:\dat\*.exb"
dinit f$+" ExAb database"
dtext "","Present: "+n$,2
dfile b1$,"",a%
if a%=19
	dbuttons f$,13,"Quit",27
	goto d::
elseif a%=8
	dbuttons f$,13,"Create new",290,"Quit",27
d::
	a%=dialog
	if a%=13
		if lower$(right$(b1$,4))<>".exb"
			beep 5,500
			giprint "Sorry, no ExAb database...",0
			goto b::
		endif
		b$=b1$
		n$=parse$(b$,"",x%())
		n$=mid$(b$,x%(4),loc(b$,".")-x%(4))
	elseif a%=290
		a%=19
		f$="Create"
		goto b::
	endif
endif
goto a::
endp

proc testsrv%:
local wsrv$(15),in%(6),fmt&,pid%,pfmt%
wsrv$="sys$wsrv.*"+chr$(0)
in%(1)=$100
in%(2)=uadd(addr(wsrv$),1)
if os($88,addr(in%(1))) and 1
	return(in%(1) and $ff)-$100
else
	pid%=in%(1)
endif
pfmt%=addr(fmt&)
if call($683,pid%,4,0,addr(pfmt%),0)<=0
	return -33
endif
if (fmt& and $16)=0
	return -36
endif
endp

proc p9:
last
i%=pos
if count>7
	tomacro:
	busy "Sorting database...",0
	while i%>0
		position i%
		a%=pos
		d$=upper$(D.k$)
		do
			if upper$(D.k$)<d$
				d$=upper$(D.k$)
				a%=pos
			endif
			l%=pos
			back
		until pos=1 and l%=1
		position a%
		print d$
		update
		i%=i%-1
	endwh
endif
close
tofront:
busy off
beep:(5,700)
beep:(5,500)
giprint:("Database sorted. Thanks for waiting...")
a%=0
endp
