PROC macro:
	global w$(25), f$(128), txt$(50,25), ct%,h%,r%,a%
	RunAppC:("Spell","")
	f$="\txt\wordlist.txt"
	GetList:
	w$=""
	GIPRINT: ("Psion WordPro ¸1995 PROTRONICS USA 404-351-1055")
	UseApp:("Spell")
	Esc:
	dINIT: ("Word ")
	DCHOICEA: (ct%, "", addr(txt$()), ct%)
	h%=dialog:
	if h% = 0 
		Psion:(%x)
	else
	if h% = ct%
		dINIT: ("New Word")
		dedit: (addr(w$),"", 15)
		dialog:
	else
		w$=txt$(h%)
		endif
	if w$>""
		SendText:(w$)
		Tab:
		if h%>ct%-2 :PutList: :endif
	endif :endif
ENDP

proc PutList:
	if exist (f$)
		IOOPEN(h%,f$,291)
	else
		IOOPEN(h%,f$,289)
		endif
	iowrite(h%,addr(w$)+1,len(w$))
	ioclose(h%)
endp

proc GetList:
	a%=addr(w$)
	r%=IOOPEN(h%,f$,$20)
	if r%=0
	ct%=0
	do
		ct%=ct%+1
		r%=ioread(h%,a%+1,25)
		if r%<0 :break :endif
		pokeb a%,r%
		txt$(ct%)=w$
		until ct%=51
	ioclose(h%)
	else
	ct%=1
	endif
	txt$(ct%)=left$(gettext$:,25)
	if txt$(ct%)>"" :ct%=ct%+1 :endif
	txt$(ct%)="New"
endp

