@echo off
if (%1)==() goto xhelp
if not exist %1\app\nul md %1\app
if not exist %1\app\backgamm\nul md %1\app\backgamm
if not exist %1\wve\nul md %1\wve
echo copying Backgamm to %1\app
copy backgamm.opa %1\app
echo copying Backgamm files to %1\app\backgamm
copy *.pic %1\app\backgamm
copy backgamm.rsc %1\app\backgamm
copy *.plr %1\app\backgamm
echo copying sounds to %1\wve
copy *.wve %1\wve
echo Backgamm installation complete
goto end
:xhelp
echo.
echo Type
echo BGINST drive:
echo .
echo 			drive: should be the remote drive to install on
echo			eg.	BGINST i:
echo.
:end


