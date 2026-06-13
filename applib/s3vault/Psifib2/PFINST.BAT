@echo off
if (%1)==() goto xhelp
if (%2)==() goto xhelp
if not exist %1\app\nul md %1\app
if not exist %1\app\psibg\nul md %1\app\psibg
echo copying PsiFibs to %1\app
copy app\psifibs.opa %1\app
echo copying PsiFibs files to %1\app\psibg
copy app\psibg\*.pic %1\app\psibg
copy app\psibg\psifibs.rsc %1\app\psibg
if not exist %2\bgm\nul md %2\bgm
echo copying bgm files to %2\bgm
copy bgm\*.bgm %2\bgm
echo copying txt files to %2\bgm
copy bgm\*.txt %2\bgm
echo PsiFibs installation complete
goto end
:xhelp
echo.
echo Type
echo PFINST drive1: drive2:
echo .
echo 			drive1: should be the remote drive to install
echo                            program and bitmaps on
echo                    drive2: should be the remote drive to install
echo                            bgm-files (and txt-files) on
echo			eg.	PFINST i: i:
echo                    or      PFINST a: i:
echo.
:end


