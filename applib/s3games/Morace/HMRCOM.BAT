@ECHO OFF
IF "%1"=="" GOTO HELP
IF "%1"=="?" GOTO HELP
 
MD %1\APP
MD %1\APP\MORACE
COPY MORACE.OPA %1\APP
COPY MORACE\*.PIC %1\APP\MORACE
COPY MORACE\HMLEVELS.LEV %1\APP\MORACE
COPY *.PIC %1\APP\MORACE
COPY HMLEVELS.LEV %1\APP\MORACE
GOTO THEEND

:HELP
ECHO ------------------------------------------------
ECHO Morace copy utility
ECHO Must be run under RCom
ECHO Use hmrcom remoteDrive:
ECHO Example hmrcom l:
ECHO Dont worry too much if you see a lot of file not found messages -
ECHO depending on how you have unzipped MORACE, the files could be in
ECHO different places.
ECHO -------------------------------------------------
:THEEND
@ECHO ON
