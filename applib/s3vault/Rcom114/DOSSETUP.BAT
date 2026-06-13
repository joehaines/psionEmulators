@echo off
if not exist \dos\sys$rpcs.img goto disk_bad
if (%1)==() goto xhelp
if (%1)==(?) goto xhelp
if (%1)==(-h) goto xhelp
if (%1)==(/h) goto xhelp
if (%1)==(/help) goto xhelp
if (%1)==(help) goto xhelp

set _xpth=%1
if not %_xpth%==%1 goto env_full
\dos\rmkdir %_xpth%
\dos\rmkdir %_xpth%\rtf
\dos\rmkdir %_xpth%\printdrv
\dos\rmkdir %_xpth%\scr
\dos\rmkdir %_xpth%\opl
set _xpsi=s3a
if not %_xpsi%==s3a goto env_full
set _xprt=com1
if not %_xprt%==com1 goto env_full
goto parm_top

:set_mach
set _xpsi=%1
goto parm_top

:set_com
set _xprt=%1

:parm_top
shift
if "%1"=="" goto copy_it
if "%1"=="s3" goto set_mach
if "%1"=="s3a" goto set_mach
if "%1"=="hc" goto set_mach
if "%1"=="mc" goto set_mach
if "%1"=="S3" goto set_mach
if "%1"=="S3A" goto set_mach
if "%1"=="HC" goto set_mach
if "%1"=="MC" goto set_mach
if "%1"=="com1" goto set_com
if "%1"=="com2" goto set_com
if "%1"=="com3" goto set_com
if "%1"=="com4" goto set_com
if "%1"=="COM1" goto set_com
if "%1"=="COM2" goto set_com
if "%1"=="COM3" goto set_com
if "%1"=="COM4" goto set_com
echo DOSSETUP does not understand this parameter:  %1
echo.
goto xhelp

:copy_it
echo.
echo Installing files to:  %_xpth%
echo Psion machine:  %_xpsi%
echo Serial port:  %_xprt%
echo.
echo Press Control-C if any of these settings are incorrect.
echo Press any other key to continue with the installation.
pause>NUL

copy \dos\rcom.exe %_xpth%>NUL
copy \dos\rcom.rsc %_xpth%>NUL
copy \dos\sys$rpcs.img %_xpth%>NUL
copy \dos\sys$tycl.pdd %_xpth%>NUL
copy \dos\autorcom.bat %_xpth%>NUL
copy \dos\rbackup.bat %_xpth%>NUL
copy \dos\rtfinst.bat %_xpth%>NUL
copy \rprint\rprint.exe %_xpth%>NUL
copy \rprint\prinst.bat %_xpth%>NUL
copy \rtf\*.* %_xpth%\rtf>NUL
copy \printdrv\*.* %_xpth%\printdrv>NUL
copy \scr\*.* %_xpth%\scr>NUL
copy \opl\*.* %_xpth%\opl>NUL

rem some temporary protection against the root directory
if %_xpth%==c:\ set _xpth=c:
if %_xpth%==d:\ set _xpth=d:
if %_xpth%==e:\ set _xpth=e:

copy \dos\rcom%_xpsi%.ini %_xpth%\rcom.ini>NUL
copy \dos\rbak%_xpsi%.bat %_xpth%\bak.bat>NUL
echo Physical=%_xprt%>>%_xpth%\rcom.ini
echo.
echo RCOM installed.
goto end

:env_full
echo Your MS-DOS "environment" is full.
echo Remove one or more environment variables, then try again.
echo Check your MS-DOS manual for more details of how to do this.
goto end

:disk_bad
echo You cannot run DOSSETUP from a different drive or directory.
echo Change to the disk and directory containing DOSSETUP - eg type:
echo     A:
echo     CD\
echo then:

:xhelp
echo Type DOSSETUP followed by the directory on your hard disk to
echo install the software into - for example:
echo     DOSSETUP C:\PSRCOM
echo This will install RCOM ready to run at a speed of 19,200 Baud (which
echo is suitable for the Series 3a) and using the serial port COM1.
echo.
echo You can, if you wish, change these settings when you run DOSSETUP,
echo by typing:
echo     *) the name of your Psion computer ("S3A", "S3", "HC" or "MC");
echo AND/OR
echo     *) the PC serial port your Psion computer is attached to
echo        ("COM1", "COM2", "COM3" or "COM4").
echo For example, you might type:
echo     DOSSETUP C:\PSRCOM S3 COM2
echo or
echo     DOSSETUP C:\RCOM COM3
:end
set _xpth=
set _xpsi=
set _xprt=

