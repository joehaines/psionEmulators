proc runjb:(a$,f$,act%)
rem DO NOT ALTER THIS PROCEDURE AT ALL!!!!!
rem ¸JBSoft 1996
local buf%(128),name$(128),p%(6)
name$="JBMSRS"
pokew addr(buf%()),act%
poke$ uadd(addr(buf%()),2),a$
poke$ uadd(addr(buf%()),3+len(a$)),f$
call($228b,0,255,6,addr(buf%()),addr(name$)+1)
name$=parse$("jbmacfix.opo",cmd$(1),p%())
runapp:(name$,"")
endp

