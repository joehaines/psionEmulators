@echo off
REM
REM DPBank Install script for RCOM
REM
REM %1 should be I: , L: or R:
if %1.==I:. goto ok
if %1.==L:. goto ok
if %1.==R:. goto ok
if %1.==i:. goto ok
if %1.==l:. goto ok
if %1.==r:. goto ok
echo This is a script for RCOM (which also comes with PsiWin)
echo.
echo Usage:
echo       start RCOM and in RCOM type:
echo       INSTALL I: (I: for Internal / L: for SSD-A / R: for SSD-B)
echo.
echo Note:
echo       this will install the english files only! If you want
echo       to install another language, refer to the documentation.
echo.
echo PsiWin users:
echo               copy the files WITHOUT conversion to the
echo               relevant directories. See the "installation"
echo               section in the documentation as to where the
echo               files should be copied.
goto exit

:ok
echo.
echo Installing DPBank to %1
echo -----------------------
echo OK? (If not, press Ctrl-C now)
echo.
pause
if not exist %1\app md %1\app
if not exist %1\app\dpbank md %1\app\dpbank
cd app
copy dpbank.opa %1\app\
cd dpbank
copy langs01.opo %1\app\dpbank\
copy help01.rsc  %1\app\dpbank\
cd ..

echo.
echo DPBank installed
:exit
