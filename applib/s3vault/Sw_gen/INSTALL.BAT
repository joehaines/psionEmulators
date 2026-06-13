@echo off
cls

if .%1 == . goto :def
set mpbase=%1
goto :cont2
:def
set mpbase=c:

:cont2

if exist %mpbase%\nul goto :devok1
cls
echo.
echo              The device %mpbase% was not found.  
echo.
echo Reissue the 'install' command with a valid device:
echo For example:   install d: would install Mobile pages on device d:
echo.
goto :end
:devok1

echo.
echo This process will install Mobile Pages in the directory %mpbase%\mobpages
echo.
pause

%mpbase%
IF not exist %mpbase%\mobpages\nul md %mpbase%\mobpages
if not exist %mpbase%\mobpages\pc\nul md %mpbase%\mobpages\pc
if not exist %mpbase%\mobpages\psion\nul md %mpbase%\mobpages\psion
if not exist %mpbase%\mobpages\psion\dev-m\nul md %mpbase%\mobpages\psion\dev-m
if not exist %mpbase%\mobpages\psion\dev-a\nul md %mpbase%\mobpages\psion\dev-a
if not exist %mpbase%\mobpages\psion\dev-b\nul md %mpbase%\mobpages\psion\dev-b

md %mpbase%\mobpages\psion\dev-m\app
md %mpbase%\mobpages\psion\dev-a\app
md %mpbase%\mobpages\psion\dev-b\app

md %mpbase%\mobpages\psion\dev-m\dat
md %mpbase%\mobpages\psion\dev-a\dat
md %mpbase%\mobpages\psion\dev-b\dat

md %mpbase%\mobpages\psion\dev-m\wrd
md %mpbase%\mobpages\psion\dev-a\wrd
md %mpbase%\mobpages\psion\dev-b\wrd

md %mpbase%\mobpages\psion\dev-m\spr
md %mpbase%\mobpages\psion\dev-a\spr
md %mpbase%\mobpages\psion\dev-b\spr

md %mpbase%\mobpages\psion\dev-m\app\pages
md %mpbase%\mobpages\psion\dev-a\app\pages
md %mpbase%\mobpages\psion\dev-b\app\pages

cls
rem     Always copy the pkunzip executable
copy a:\pkunzip.exe %mpbase%\mobpages\pc

if not exist %mpbase%\mobpages\pc\s3aemul.exe goto :n1
echo The Psion 3a Emulator is already installed in %mpbase%\mobpages\pc
goto :n2

:n1
if not exist a:\s3aemul.zip goto :n2
cls
Echo Copying & expanding Psion 3a Emulator to %mpbase%\mobpages\pc
copy a:\s3aemul.zip %mpbase%\mobpages\pc
cd %mpbase%\mobpages\pc
pkunzip -d s3aemul
if exist readme.txt rename readme.txt readme.s3a
if exist readme.s3a copy readme.s3a %mpbase%\mobpages
del s3aemul.zip

:n2
if not exist  a:\mpgen_pc.zip goto :n3
cls
Echo Copying and expanding Pages Generator DOS files ....
copy a:\mpgen_pc.zip %mpbase%\mobpages\pc
cd %mpbase%\mobpages\pc
pkunzip -d mpgen_pc
del mpgen_pc.zip


:n3
if not exist  a:\mpgen_3a.zip goto :n4
cls
Echo Copying and expanding Pages Generator Psion files ....
copy a:\mpgen_3a.zip %mpbase%\mobpages\psion\dev-m
cd %mpbase%\mobpages\psion\dev-m
pkunzip -d mpgen_3a
del mpgen_3a.zip

cd %mpbase%\mobpages\psion\dev-m\app\pages
copy file?.lab %mpbase%\mobpages\psion\dev-a\app\pages
copy file?.lab %mpbase%\mobpages\psion\dev-b\app\pages

cd %mpbase%\mobpages\psion
if exist a:\readme.1st copy a:\readme.1st %mpbase%\mobpages\readme.mpc

cd %mpbase%\mobpages\pc
if exist psion.bat copy psion.bat %mpbase%\psion.bat
if exist mpprep.bat copy mpprep.bat %mpbase%\mpprep.bat
if exist mpcomp.bat copy mpcomp.bat %mpbase%\mpcomp.bat
if exist mpmenu.bat copy mpmenu.bat %mpbase%\mpmenu.bat
if exist mpdrop.bat copy mpdrop.bat %mpbase%\mpdrop.bat
del pkunzip.exe
cls
echo.
echo Refer to the file 'readme.mpc' in the %mpbase%\mobpages directory,
echo for further discussion on using Mobile Pages.
echo.
echo.
echo     Press any key to view 'readme.mpc'
pause
if not exist %mpbase%\mobpages\readme.mpc goto :nompc
echo on
type %mpbase%\mobpages\readme.mpc | more
echo off
:nompc
cls
echo.
echo The file that was displayed contained additional installation 
echo instructions, and the terms and conditions of the Mobile Software
echo Software License Agreement and of the Limited Warranty.
echo.
echo By using the supplied software for the first time, you accept these
echo conditions.
echo.
ECHO To see the contents again, either view the file 'readme.1st', on the
echo supplied diskette, or the file 'readme.mpc' in the %mpbase%\mobpages directory.
echo.
echo You can use the Notepad in Windows or the TYPE command, in DOS, 
echo to view this file.
echo.
echo         ******* Installation Process Complete *******
echo.
goto :end



:end



