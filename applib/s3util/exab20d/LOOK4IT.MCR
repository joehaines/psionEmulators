rem ษอออออออออออออออออออออออออออออออออออออป
rem บ   Look4it - ธHuub Linthorst, 1997   บ
rem บ  MacSys macro for database linking  บ
rem บ    Plug-in for ExAb (1.9l and up)   บ
rem บ E-mail: linthors@chem.leidenuniv.nl บ
rem ศอออออออออออออออออออออออออออออออออออออผ

rem You may want to change the name of the DBF files where Look4it searches
rem the required data, by changing "Address" and/or "Notes" in the lines
rem starting with b2$ some 20 lines down into the filenames of your choice. 
rem Note: leave the quotation marks!

PROC macro:
global s$(255),v$(38)

v$="Look4it (1.1) ฤ ธHuub Linthorst, 1997"
useapp:("active")
if currpid%:<>srvpid%:
	tomacro:
	tofront:
endif
if testsrv%:=0
	if len(gettext$:)=0 and pid2app$:(currpid%:)<>"Agenda"
		sendkey:(262,14,1)
	endif
	hlstart:
endif
endp

proc hlstart:
local b2$(8)
s$=gettext$:
if loc(s$,":")>0
	b2$="Address" rem DATABASE TO SEARCH DATA ENDING WITH ":"
else
	b2$="Notes" rem DATABASE TO SEARCH OTHER DATA
	s$=s$+":"
endif
s$=left$(s$,loc(s$,":")-1)
while loc(s$," ")<>0
	s$=mid$(s$,loc(s$," ")+1,len(s$))
endwh
var%(4)=actpid%:
giprint:("Looking for '"+s$+"' in "+b2$+".dbf...")
runappc:("Data",b2$+".dbf")
hotkey:(632,8,"getchar",1)
var%(2)=actpid%:
sendtext:(s$+chr$(13))
giprint("<Psion-x> to return...")
z::
endp

proc getchar:(k%,n%)
var%(3)=actpid%:
hotkey:(632,8,"getchar",0)
if var%(2)<>var%(3)
	goto q::
endif
usepid:(var%(4))
tofront:
q::
usepid:(var%(3))
var%(3)=0
exit:
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

