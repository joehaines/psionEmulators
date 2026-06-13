@echo off
if (%1)==() goto xhelp
if not exist %1\wdr\nul md %1\wdr
echo copying RTF files to %1\wdr
copy rtf\wl$rtf.dyl %1\wdr
copy rtf\ws$rtf.dyl %1\wdr
echo RTF installation complete
goto end
:xhelp
echo.
echo Change directory to the RCOM install directory. i.e.
echo CD C:\PSRCOM
echo .
echo Then type
echo RTFINST drive:
echo.
echo         drive: should be the drive letter on the
echo                remote computer. e.g. RTFINST I:
echo.
:end
