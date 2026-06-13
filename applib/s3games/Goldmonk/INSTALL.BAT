@ECHO OFF
CLS
ECHO.
ECHO GOLD MONKEY INSTALLER
ECHO ---------------------
ECHO.
ECHO This installer works only if you have the PSION RCOM program and RCOM.EXE
ECHO is in your path. This means that you can start RCOM from wherever you are
ECHO on your hard-disk(s).
ECHO.
ECHO If you are not quite sure if you have RCOM on your path, answer N to the
ECHO following question and, when back to the DOS, try to write RCOM and then
ECHO press the enter key. If you succeed in starting RCOM this way, you can
ECHO then restart this batch file and answer Y to the following question.
ECHO.
ECHO If you do not have RCOM but MCLINK, please anwser no to the following
ECHO question and read the GOLDMONK.DOC file for the instructions of where to
ECHO copy the Gold Monkey files.
ECHO.
ECHO If you have RCOM but it is out of your path, normally start RCOM, then
ECHO start the INSTALL.SCR batch file from inside RCOM.
ECHO.
CHOICE.COM /C:YN "Are you quite sure that RCOM is in your path "
IF ERRORLEVEL 2 GOTO END

CLS
ECHO.
ECHO GOLD MONKEY INSTALLER
ECHO ---------------------
ECHO.
ECHO Please press the letter of the drive you wish to install Gold Monkey to.
ECHO.
ECHO Available choices:
ECHO                     A     To install to SSD A: (the LEFT one)
ECHO                     B     To install to SSD A: (the RIGHT one)
ECHO                     M     To install to Internal RAM Disk
ECHO.
ECHO                     Q     TO QUIT INSTALLER
ECHO.
CHOICE.COM /C:ABMQ /N "Selected drive: "
CLS
IF ERRORLEVEL 4 GOTO END
IF ERRORLEVEL 3 RCOM /C INSTALL.SCR M:
IF ERRORLEVEL 2 RCOM /C INSTALL.SCR B:
IF ERRORLEVEL 1 RCOM /C INSTALL.SCR A:

:END
