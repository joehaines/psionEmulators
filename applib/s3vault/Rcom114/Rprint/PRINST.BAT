@echo off
if (%1)==() goto xhelp
if not exist %2\wdr\nul md %2\wdr
echo copying %1 files to %2\wdr
copy printdrv\%1.* %2\wdr
echo Driver installation complete
goto end
:xhelp
echo.
echo Change directory to the RCOM install directory. i.e.
echo CD C:\PSRCOM
echo .
echo Then type
echo PRINST Printer_Driver drive:
echo.
echo         Printer_Driver should be the name of the
echo         required driver without its extension
echo         eg HP2,HP2P,HP3,PS,PSAPPLE
echo.
echo         drive: should be the drive letter on the
echo                remote computer. e.g. PRINST HP2 I:
echo.
:end


