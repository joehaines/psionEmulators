@echo off
yesno Do you want to install S3C?
if errorlevel 1 goto exit
mclink @install.txt

yesno Do you want to install the EPOC Libraries?
if errorlevel 1 goto nolib
mclink @instepoc.txt
:nolib

:exit
echo Finished.

