PROC macro:
global c$(1),d$(30),e$(1),l$(255),n$(36),r$(2),s$(255),t$(3),v$(3),a%,i%,r%,t

rem ллллллллллллллллллллллллллллллллллллллл
rem л ExAb - ИHuub Linthorst, 96-97       л
rem л Macro to EXtend ABbreviations       л
rem л E-mail: linthors@chem.leidenuniv.nl л
rem ллллллллллллллллллллллллллллллллллллллл

c$="+" rem whole text indicator
r$="/" rem linefeed indicator
e$="m" rem JBMACFIX.OPO drive (if present)

n$="ExAb v.1.7c - ИHuub Linthorst, 96-97"
v$="17c"

useapp:("active")
if Pid2App$:(CurrPid%:)= "Program"
	d$="ExabP"
else
	d$="Exab"
endif

sendkey:(left%,ctrl%+shift%,1)
p1:
if s$=""
	gettext$:
	goto x::
endif
if s$=c$+c$
	delete:
	a%=1
	giprint:("ExAb will search whole text...")
	p2:
else 
	p3:
endif
x::
endp

proc p3:
if exist ("loc::m:\dat\"+d$+".DBF")
	open "loc::m:\dat\"+d$+".DBF",D,k$,l$
	last
	p4:
else
	p6:
endif
endp

proc p4:
if s$=".?"
	delete:
	p10:
	if r%=0
		goto x::
	endif
	p4:
elseif s$=".d"
	delete:
	puttext:(mid$(datim$,5,11))
	goto x::
elseif s$=".t"
	delete:
	puttext:(mid$(datim$,17,8))
	goto x::
else
	l$=lower$(mid$(s$,loc(s$,".")+1,3))
	if l$="dbf"
		l$="data"
	elseif l$="wrd"
		l$="word"
	elseif l$="agn"
		l$="agenda"
	elseif l$="spr"
		l$="sheet"
	else
		l$=""
		goto c::
	endif
	delete:
	if exist ("loc::"+e$+":\app\jbmacfix.opo")
		runjb:(l$,s$,actpid%:)
	else 
		runappc:(l$,s$)
	endif
endif
c::
if len(s$)>10
	i%=1
	s$=mid$(s$,1,10)
endif

position 1
while (not eof) and (s$<>D.k$)
	next
endwh
if s$=D.k$
	delete:
	p7:
elseif (eof) and s$<>D.k$
	if pos>100
		beep:(8,600)
		giprint:("Maximum number reached...")
		goto z::
	endif
	dinit:(n$)
	dtext:("","Enter full text for '"+s$+"'    (<й = "+r$+")",2)
	dedit:(addr(l$),s$,24)
	if i%=1
		beep:(8,600)
		giprint:("Abbreviations 10 characters max.")
	endif
	if a%<>0
		beep:(8,600)
		dtext:("","<Enter> continue '"+c$+"' search, <Esc> quit",2)
	endif
	r%=Dialog:
	if r%=1
		if l$=""
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
	else
		goto z::
	endif
endif
t=t+1
x::
i%=0
if a%<>0
	right:
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
right:
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
	sendkey:(259,6,1)
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
beep:(8,600)
giprint:("ExAb finished extending "+num$(t,4)+" abbreviations")
endp

proc p1:
if currpid%:<>srvpid%:
	tomacro:
	tofront:
endif
if testsrv%:=0
s$=gettext$:
return
else
return
endif
endp

proc p5:
append
p7:
esc:
sendkey:(259,6,1)
delete:
return
endp

proc p6:
giprint:("Welcome to ExAb, one moment please...")
create "loc::m:\dat\"+d$+".DBF",D,k$,l$

rem ---This line and the lines down to the next rem-line contain on-line info and may be removed
if exist ("loc::m:\wrd\exab"+v$+".wrd")
	goto x::
endif
s$=""
runappc:("word","loc::m:\wrd\exab"+v$+".wrd")
sendkey:(260,12,1)
enter:
enter:
D.k$="exab"
D.l$=n$+r$+r$+"A Macro to EXtend ABbreviations"+r$+r$+"E-Mail me for comments, suggestions and improvements at linthors@chem.leidenuniv.nl//With the cursor behind the word help, hit the hot-key again to get help"
p5:
sendkey:(259,6,13)
delete:
D.k$="help"
D.l$="ExAb HELP"+r$+r$+"This is how ExAb works. Type an abbreviation, e.g. 'sy'. With the cursor behind 'sy', hit ExAb's hot-key (I use <Psion-Enter>). The macro will search database EXAB for 'sy'. hot-key..1"
p5:
D.k$="hot-key..1"
D.l$="If found, ExAb will replace 'sy' in your text with the corresponding full text. If not found, ExAb will present an input screen to enable you to add 'Sincerely Yours,' or 'Since Yesterday' to the database. hot-key..2"
p5:
D.k$="hot-key..2"
D.l$="Alternatively, ExAb can search and replace all abbreviations of a text in one go. Therefore, type a '"+c$+"' at the end of each abbreviation. After finishing, type '"+c$+c$+"' at the start of the text followed by the hot-key. hot-key..3"
p5:
D.k$="hot-key..3"
D.l$=r$+"'Exabbing' on special codes '.t' and '.d' will stamp the time and date, respectively, while hot-keying on '.?' opens ExAb's definitions screen for quick reference. hot-key..4"
p5:
D.k$="hot-key..4"
D.l$=r$+r$+"Database EXAB, is automatically created during installation. It contains these help lines and the abbreviations and extensions that you add during - a hopefully enjoyable - use. Using DATA, you can modify EXAB.DBF. hot-key..5"
p5:
enter:
D.k$="hot-key..5"
D.l$=r$+r$+"Now try ExAb yourself with sy"
append
close
sendkey:(260,12,1)
rem ---This line and the lines up to the previous rem-line may be removed
x::
p4:
return
endp

proc p7:
l$=D.l$
while loc(l$,r$)<>0
	l$=mid$(l$,1,(loc(l$,r$)-1))+chr$(000)+mid$(l$,(loc(l$,r$)+1),len(l$))
endwh
puttext:(l$)
return
endp

proc p8:
local m1$(100,10),m$(100,43),q$(3)
giprint:("ExAb is loading definitions...")
position 1
while not eof
	m1$(pos)=D.k$
	if len(D.l$)<21
		q$=""
	else
		q$="..."
	endif
	m$(pos)=m1$(pos)+"  :  "+mid$(D.l$,1,20)+q$
	next
endwh
dinit:("ExAb definitions ("+num$(pos-1,3)+")")
dchoicea:(1,"",addr(m$()),pos-1)
dtext:("","<Enter> = paste extension",2)
r%=dialog: 
if r%<>0
	puttext:(" "+m1$(r%))
	s$=m1$(r%)
endif
endp

proc p10:
dinit:("ExAb Database ("+d$+".dbf)")
dbuttons:("List,%l,Open,%o,Sort,%s")
r%=dialog:
if r%=1
	p8:
	return
elseif r%=2
	if exist ("loc::"+e$+":\app\jbmacfix.opo")
		runjb:("Data","m:\dat\"+d$+".dbf",actpid%:)
	else 
		runappc:("data","m:\dat\"+d$+".dbf")
	endif
elseif r%=3
	p9:
else
	s$=""
endif
r%=0
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

proc runjb:(a$,f$,act%)
rem ИJBSoft 1996
local buf%(128),name$(128),p%(6)
close
name$="JBMSRS"
pokew addr(buf%()),act%
poke$ uadd(addr(buf%()),2),a$
poke$ uadd(addr(buf%()),3+len(a$)),f$
call($228b,0,255,6,addr(buf%()),addr(name$)+1)
name$=parse$("jbmacfix.opo",cmd$(1),p%())
runapp:(name$,"")
endp

proc p9:
local z%,g$(10),g%,lp%,n$(128),c%
last
z%=pos
if count>7
	tomacro:
	busy "Sorting database...",0
	while z%>0
		position z% 
		g%=pos
		g$=upper$(D.k$)
		do
			if upper$(D.k$)<g$
				g$=upper$(D.k$)
				g%=pos
			endif
			lp%=pos
			back
		until pos=1 and lp%=1
		position g%
		print g$
		update
		z%=z%-1
	endwh
endif
close
tofront:
busy off
beep:(6,500)
giprint:("Database sorted. Thanks for waiting...")
endp

