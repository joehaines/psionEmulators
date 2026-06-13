ECHO OFF
ECHO .
IF %1.==. GOTO SYNTAX
mkdir %1\app\logit
mkdir %1\dyl
copy *.lib %1\app\logit
copy *.opa %1\app
copy *.dyl %1\dyl
copy *.rzc %1\app\logit
GOTO EXIT
:SYNTAX
ECHO .
ECHO .
ECHO The correct syntax for %0 is:
ECHO .
ECHO    %0 r: or l: (using RCOM)
ECHO .
ECHO Where r: is the drive to install Log-IT! to.
ECHO .
GOTO EXIT
:EXIT
