PROC macro:
UseApp:( "active" )
dinit:( "Info" )
dtext:( "Application name", Pid2App$:( CurrPid%: ), 0 )
dtext:( "Used file name", Pid2Fil$:( CurrPid%: ), 0 )
dtext:( "Task name", Pid2Tsk$:( CurrPid%: ), 0 )
dtext:( "Process ID", "$" + hex$( CurrPid%: ), $200 )
dtext:( "Internal", DevName$:( "m:" ), 0 )
dtext:( "A SSD", DevName$:( "a:" ), 0 )
dtext:( "B SSD", DevName$:( "b:" ), 0 )
dialog:
ENDP
