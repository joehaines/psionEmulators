@ECHO OFF
REM Install program for Midnight on Psion 3a.
REM First set up RFM or other utility to map the desired Psion drive to a
REM DOS drive, then run using INSTALL [drive name], eg.
REM INSTALL D:
REM If your psion drive is mapped to DOS drive D:
REM
ECHO.
IF "%1"=="" GOTO ERRORM
ECHO Ignore any File/Directory already exists messages!
MKDIR %1\APP
CD %1\APP
COPY MIDNIGHT.OPA %1
MKDIR %1\APP\MIDN
CD %1\APP\MIDN 
COPY *.OPO %1
COPY *.BIN %1
COPY *.PIC %1
COPY *.FON %1
ECHO Installation complete!
GOTO END
:ERRORM
ECHO Usage: INSTALL [drive letter], see READ.ME file.
:END
ECHO.
