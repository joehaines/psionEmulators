ECHO OFF
ECHO .
IF %1.==. GOTO SYNTAX
mkdir %1\app\banker
mkdir %1\bank
mkdir %1\app\banker\plug
mkdir %1\app\banker\report
copy 3bank2_a.opo %1\app\banker
copy 3bank3_a.opo %1\app\banker
copy 3bank4_a.opo %1\app\banker
copy maineng.opo %1\app\banker
copy b3help_a.opo %1\app\banker
copy *.rpt %1\app\banker\report
copy *.opa %1\app
copy 3bquick.opo %1\app\banker
copy 3bank5_a.opo %1\app\banker
copy logo.pic %1\app\banker
copy pel.pic %1\app\banker
copy help.rsc %1\app\banker
copy *.fon %1\app\banker
copy *.plg %1\app\banker\plug
GOTO EXIT
:SYNTAX
ECHO .
ECHO .
ECHO The correct syntax for %0 is:
ECHO .
ECHO    %0 r: or l: (using RCOM)
ECHO .
ECHO Where r: is the drive to install Banker to.
ECHO .
GOTO EXIT
:EXIT
