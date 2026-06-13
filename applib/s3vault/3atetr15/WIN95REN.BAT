@ECHO OFF
CLS

ECHO This batch file will rename the Tetris Documentation Files from 8 characters
ECHO MS-DOS standard to Windows 95 250 characters standard.
ECHO.
ECHO This will give you more detailed information of what every file contains.
ECHO.
ECHO You can do this operation only if you have Windows 95 (or Windows NT)
ECHO installed and running.
ECHO.

CHOICE.COM /C:YN "Are you sure you are running Windows 95 or Windows NT NOW "
IF ERRORLEVEL 2 GOTO END

REN TETRIS "Tetris"
REN TOURNAME.TXT "First World Tetris Tournament Instructions.txt"
REN HISTORY.TXT "History.txt"
REN INSTALL.BAT "Install.bat"
REN INSTALL.SCR "Install.scr"
REN TETRIS.TXT "Tetris - Game Documentation.txt"

:END