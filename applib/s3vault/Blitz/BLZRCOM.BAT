@ECHO OFF
IF "%1"=="" GOTO HELP
IF "%1"=="?" GOTO HELP
 
MD %1\APP
MD %1\APP\BLITZ
COPY BLITZ.OPA %1\APP
COPY BLITZ\TITLE.PIC %1\APP\BLITZ
COPY BLITZ\BUILDING.PIC %1\APP\BLITZ
COPY TITLE.PIC %1\APP\BLITZ
COPY BUILDING.PIC %1\APP\BLITZ

IF "%2"=="" GOTO NORMAL

COPY BLITZ\ANDBCKD*.PIC %1\APP\BLITZ\BACKGND*.PIC
COPY ANDBCKD*.PIC %1\APP\BLITZ\BACKGND*.PIC
GOTO THEEND

:NORMAL
COPY BLITZ\BACKGND*.PIC %1\APP\BLITZ
COPY BACKGND*.PIC %1\APP\BLITZ
GOTO THEEND

:HELP
ECHO ------------------------------------------------
ECHO Blitz copy utility
ECHO Must be run under RCom
ECHO Use blzrcom remoteDrive: [X]
ECHO Example blzrcom l:
ECHO Or blzrcom l: X to use alternative backgrounds
ECHO Dont worry too much if you see a lot of file not found messages -
ECHO depending on how you have unzipped blitz, the files could be in
ECHO different places.
ECHO -------------------------------------------------
:THEEND
@ECHO ON
