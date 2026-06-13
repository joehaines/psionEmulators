PROC macro:
	local d%,file$(128),field%
	open "\cix\macro\emailmac.dbf",c,name$,number$
		file$=c.name$ :field%=val(c.number$)
	close
	RunAppC:("Word","\cix\reply.txt")
	SendKey:(260,12,1)
	RunAppB:("Data",file$)
	loop::
	SendKey:(614,8,1)
	SendKey:(84,2,1)
	SendKey:(79,2,1)
	SendKey:(32,0,1)
	SendKey:(256,0,1)
	SendKey:(121,0,1)
	SendKey:(13,0,1)
	SendKey:(258,4,1)
	SendKey:(263,10,1)
	UseFile:(file$)
	RunAppC:("Data",file$)
	SendKey:(292,0,1)
	SendKey:(292,0,1)
	SendText:(left$(GetText$:,24))
	SendKey:(13,0,1)
	dia::
	dInit:("This one?")
	dButtons:("Yes,%y,No,%n,Ignore,%i,Finish,%x,Quit,%q")
	d%=Dialog:
	if d%=5
		kill:
		RunAppC:("data","\cix\reply.txt")
		kill:
		goto end
	elseif d%=4
		RunAppC:("Word","reply.txt")
		SendKey:(609,8,1)
		Sendtext:("reply.txt")
		SendKey:(256,0,1)
		SendKey:(116,0,1)
		SendKey:(13,0,1)
		SendKey:(121,0,1)
		d%=0
		while d%<2000
			d%=d%+1
		endwh
		kill:
		UseFile:(file$)
		SendKey:(632,8,1)
		goto end
	elseif d%=2
		SendKey:(13,0,1)
		goto dia
	elseif d%=3
		RunAppC:("Data","\cix\reply.txt")
		SendKey:(258,0,1)
		goto loop
	endif
SendKey:(629,8,1) rem Psion-U
SendKey:(260,12,1)
d%=1
do
	SendKey:(257,4,1) rem Control-Down
	d%=d%+1
until d%=field%
SendKey:(263,10,1) rem Shift-Psion-End
RunAppC:("Word","reply.txt")
SendKey:(8,0,1)
SendKey:(610,8,1)
SendKey:(257,0,1)
SendKey:(259,0,1)
	goto loop
	end::
ENDP
