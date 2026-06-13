rem ษอออออออออออออออออออออออออออออออออออออป
rem บ    NDcode - ธHuub Linthorst, 1997   บ
rem บ   MacSys macro for text encryption  บ
rem บ           Plug-in for ExAb          บ
rem บ E-mail: linthors@chem.leidenuniv.nl บ
rem ศอออออออออออออออออออออออออออออออออออออผ

PROC macro:
global l$(255),f$(8),s$(255),v$(38),x%(8),f%,m%,g%,i%,l%,r%,z%
v$="NDcode (1.3) ฤ ธHuub Linthorst, 1997"
useapp:("active")
if srvpid%:<>currpid%:
	tomacro:
	tofront:
endif
if testsrv%:=0
	hlstart:
else
	sendkey:(right%,shift%,1)
	gettext$:
endif
endp

proc hlstart:
z%=0
if len(gettext$:)=0
	q6::
	sendkey:(257,6,1)
	if loc(gettext$:,">>>")=0
		if z%=1
			goto x::
		endif
		z%=1
		left:
		psion:(%f)
		sendkey:(62,2,3)
		enter:
		left:
		goto q6::
	endif
	l$=mid$(gettext$:,loc(gettext$:,">>>"),(loc(gettext$:,"<<<")+3)-loc(gettext$:,">>>"))
	if len(l$)>66
		beep:(8,800)
		dinit:("String too long to show in window")
		dtext:("","Select string manually",2)
		dtext:("","and hot-key on selection",2)
		dialog:
		goto x::
	endif
	q1:
	if len(f$)=0
		goto x::
	endif
		q0:
	if l%=1
		goto x::
	endif
	dinit:("")
	dtext:("",s$,2)
	dposit:(-1,1)
	dialog:
	goto x::
elseif len(gettext$:)>249
	beep:(6,1000)
	giprint:("String too long...")
	goto x::
elseif len(gettext$:)>0
	q1:
	if len(f$)>0
		l$=gettext$:
		q0:
		if g%=0 and l%<>1
			delete:
			puttext:(" >>>"+s$+chr$(i%+len(s$))+"<<<")
			goto x::
		elseif g%=1
			if l%=1
				goto x::
			endif
			sendkey:(8,0,2)
			puttext:(s$)
			sendkey:(21,4,2)
			dinit:("")
			dchoice:(1,"","ENcode,DEcode,Delete")
			dposit:(1,1)
			r%=dialog:
			if r%=3
				sendkey:(14,4,2)
				delete:
				return
			elseif r%<>2
				tomacro:
				tofront:
				giprint:("One moment please...")
				l$=gettext$:
				s$=""
				q0:
				delete:
				puttext:(" >>>"+s$+chr$(i%+len(s$))+"<<<")
			endif
			sendkey:(14,4,2)
			goto x::
		endif
	endif
	return
endif
x::
right:
endp

proc q1:
tomacro:
f$=""
busy off
dinit v$
dxinput f$,"Encryption key"
dialog
if len(f$)>0
	tofront:
	do
		f$=left$(f$+f$,8)
	until len(f$)=8
	f%=1
	while f%<9
		x%(f%)=asc(mid$(f$,f%,1))
		i%=i%+x%(f%)
		f%=f%+1
	endwh
endif
tofront:
endp

proc q0:
f%=8
m%=1
if left$(l$,3)=">>>" and right$(l$,3)<>"<<<"
	goto x::
elseif left$(l$,3)<>">>>" and right$(l$,3)="<<<"
	x::
	beep:(6,800)
	l%=1
	giprint:("Not correctly selected...")
	return
elseif left$(l$,3)=">>>" and right$(l$,3)="<<<"
	if chr$(i%+len(l$)-7)<>mid$(l$,len(l$)-3,1)
		l%=1
		beep:(6,800)
		giprint:("Wrong password...")
		return
	endif
	l$=mid$(l$,4,len(l$)-7)
	g%=1
else
	g%=0
endif
while m%<=len(l$)
	if g%=1
		s$=s$+chr$((asc(mid$(l$,m%,1)))-x%(f%))
	elseif g%=0
		s$=s$+chr$((asc(mid$(l$,m%,1)))+x%(f%))
	endif
	m%=m%+1
	f%=f%-1
	if f%=0
		f%=8
	endif
endwh
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

