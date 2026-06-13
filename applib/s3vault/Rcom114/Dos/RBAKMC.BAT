@echo off
xwait link
if errorlevel 1 goto failed
echo Stopping L:
xstop l:
if errorlevel 1 goto failed
echo Backup L: C:\PSION\L
backup l: c:\psion\l
if errorlevel 1 goto failed
echo Stopping M:
xstop m:
if errorlevel 1 goto failed
echo Backup M: C:\PSION\M
backup m: c:\psion\m
if errorlevel 1 goto failed
echo Stopping R:
xstop r:
if errorlevel 1 goto failed
echo Backup R: C:\PSION\R
backup r: c:\psion\r
if errorlevel 1 goto failed
echo Stopping S:
xstop s:
if errorlevel 1 goto failed
echo Backup S: C:\PSION\S
backup s: c:\psion\s
if errorlevel 1 goto failed
echo Stopping I:
xstop i:
if errorlevel 1 goto failed
echo Backup I: C:\PSION\I
backup i: c:\psion\i
if errorlevel 1 goto failed
echo BACKUP COMPLETED OK
pause
quit
:failed
echo ****** BACKUP FAILED ******
pause
quit
