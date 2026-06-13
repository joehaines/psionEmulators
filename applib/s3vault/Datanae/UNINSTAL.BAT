ECHO OFF
ECHO .
IF %1.==. GOTO ERROR
ECHO THIS PROCEDURE DELETES DATANA AND ALL
ECHO FILES WITH IT.
ECHO .
PAUSE
xwait link
del %1\app\datana.opa
del %1\opd\datana.rsc
del i:\dat\datana.dbf
del %1\wrd\datana.wrd
del %1\d2s\*.d2s
rmdir %1\d2s
GOTO EXIT
:ERROR
ECHO .
ECHO .
ECHO Call %0 after start of RCOM with:
ECHO .
ECHO	%0 i: or r: or l: 
ECHO .
ECHO .
GOTO EXIT
:EXIT
