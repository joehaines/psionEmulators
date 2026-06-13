rem -- Anchor.mcr (from Bracket.mcr)

rem -- 1995/11/21 Release version 1.1
rem -- 1995/05/23 Test version 1.0

PROC Macro:
	
	local x$(255)
	
	UseApp: ("Active")

	SendKey: (right%, shift%, 1)	
	if SrvPid%: <> CurrPid%:
		ToMacro:
		ToFront:
	endif
	SendKey: (left%, shift%, 1)
	
	x$ = GetText$:
	
	if x$<>""
		Delete:
	endif
	
	PutText: ("<a name = """"></a>")
	Right:
	nLeft:(4)
	
	if x$<>""
		Psion:(%i)
	endif

	SendKey:(left%,psion%,0)
		
	Psion:(%f)
		SendKey:(%",0,2)
		Down:
		SendKey:(%B,0,0)
		Down:
		SendKey:(%N,0,0)
	Enter:
	Right:
	Left:

ENDP
