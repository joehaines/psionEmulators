PsiWin add-on translators
-------------------------
Nick Healey
Psion PLC
25/8/95

The Psion Manager application in PsiWin uses a custom version of the
"Conversion Plus" library of file converters - a product of Dataviz,
Inc. This version of the library has been designed to allow "add-on"
file converters to be developed and used with Psion Manager with no
change to the Psion Manager software being required.

PWADDON.DOC is a Word for Windows v6 file that should provide 
adequate information for implementing such file converters.

The converters should be tested and used simply by placing them in
the CONVERT subdirectory of an installed version of the PsiWin
application.  

PWADDON.TXT, a plain text version of the file which you can for
example view in the Windows Notepad, is also included in the ZIP.

Two of the PsiWin file converters - WAV-WVE and BMP-PIC - were
developed in-house by Psion, using the instructions in PWADDON.DOC. 
The source code for these, which may prove instructive, is also
included, as WAVPXL.ZIP and BMPPXL.ZIP.

