ECHO OFF
IF %1.==. GOTO ERROR
xwait link
mkdir %1\app
mkdir %1\opd
mkdir i:\dat
mkdir %1\wrd
mkdir %1\d2s
copy datana.opa %1\app
copy datana.rsc %1\opd
copy datana.dbf i:\dat
copy datana.wrd %1\wrd
copy tutor?.d2s %1\d2s
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
