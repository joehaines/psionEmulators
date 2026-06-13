rem -- Image.mcr (from Bracket.mcr)

rem -- 1995/11/27 Release version 1.1
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

	PutText: ("<img src = """" align = """" alt """">")
	SendKey:(left%,ctrl%,0)
		
	Psion:(%f)
		SendKey:(%",0,2)
		Down:
		SendKey:(%F,0,0)
		Down:
		SendKey:(%N,0,0)
	Enter:
	Right:
	Left:
	
ENDP
