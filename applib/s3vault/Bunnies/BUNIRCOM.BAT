@ECHO OFF
IF "%1"=="" GOTO HELP
IF "%1"=="?" GOTO HELP
 
MD %1\APP
MD %1\APP\BUNNIES
COPY BUNNIES.OPA %1\APP
COPY BUNNIES\*.PIC %1\APP\BUNNIES
COPY BUNNIES\BUNNIES.LEV %1\APP\BUNNIES
COPY *.PIC %1\APP\BUNNIES
COPY BUNNIES.LEV %1\APP\BUNNIES
GOTO THEEND

:HELP
ECHO ------------------------------------------------
ECHO Bunnies copy utility
ECHO Must be run under RCom
ECHO Use BuniRCom remoteDrive:
ECHO Example bunircom l:
ECHO Dont worry too much if you see a lot of file not found messages -
ECHO depending on how you have unzipped BUNNIES, the files could be in
ECHO different places.
ECHO -------------------------------------------------
:THEEND
@ECHO ON
