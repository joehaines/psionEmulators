@echo off
if (%1)==() goto xhelp
if not exist %1\app\nul md %1\app
echo Copying Dialog Designer to %1\app
copy dialog.app %1\app
echo.
echo Dialog Designer installed successfully.
echo Install the application using Psion-I
echo at the System Screen and choosing DIALOG.APP
echo before pressing Enter.
echo.
goto end
:xhelp
echo.
echo Change directory to the RCOM install directory. i.e.
echo CD C:\PSRCOM
echo.
echo Then type
echo.
echo   INSTALL drive:
echo.
echo drive: should be the drive letter on the
echo        remote computer. e.g. INSTALL I:
echo.
:end
