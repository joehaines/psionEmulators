@echo off
if (%1)==() goto xhelp
if not exist %1\APP\BIKE md %1\APP\BIKE
echo copying the CycleCalc application to %1\app
copy APP\Cycle.opa %1\app\Cycle.opa
copy APP\BIKE\CYCLE.RSC %1\APP\BIKE\CYCLE.RSC
echo CycleCalc installation complete
echo Choose Install from the System Screen
goto end
:xhelp
echo.
echo Change directory to the CycleCalc install directory. i.e.
echo CD C:\CYCLE
echo .
echo Then type
echo BIKEINST drive:
echo.
echo         drive: should be the drive letter on the
echo                remote computer. e.g. BIKEINST I:
echo.
:end
