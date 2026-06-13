rem -- Ttype.mcr (from Bracket.mcr)

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
	
	x$ = GetText$:
	
	if x$<>""
		Delete:
	endif
	
	PutText: ("<tt></tt>")
	Right:
	nLeft:(5)
	
	if x$<>""
		Psion:(%i)
		nRight:(6)
	endif

ENDP
