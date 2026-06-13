PROC macro:
REM I have this macro assigned to CTRL-SHIFT-J
REM Written by Richard E. Rettke and placed in the Public Domain
REM email: rrettke@execpc.com
REM Display JULIAN Date
Local s&,yr%,mo%,dy%,hr%,mn%,sc%,yrday%
s&=DATETOSECS(year,month,day,hour,minute,second)
SECSTODATE s&,yr%,mo%,dy%,hr%,mn%,sc%,yrday%
Useapp:("Active")
dInit:("")
dText:("Today is",Mid$(datim$,1,16),0)
dText:("Julian day",Gen$(yrday%,3),0)
Dialog:
ENDP
