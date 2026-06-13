ChaDis7 v1.10
=============

Battery Monitor for the Psion Series 7 / Netbook
Freeware by Kevin Millican

Disclaimer
==========
The author can accept no responsibility for any damage to equipment or loss of data resulting from using ChaDis7. Use of this program is solely at the user's risk and by using ChaDis7 you, as the user, imply your acceptance of these conditions.

Installation
============
Install ChaDis7 by double-clicking on the ChaDis7.SIS file in Windows whilst your Netbook/S7 is connected to the PC under PsiWin, or copy ChaDis7.SIS to the Netbook/S7 and tap it twice from the System screen.

Using ChaDis7
=============

Click on the ChaDis icon from the Extras menu. This will display the main ChaDis graph and status. Press the <Enter> or <Escape> key to hide the program, or tap the screen with the pen.

The Menu button gives access to the General Settings dialog and a couple of other options.

Any other keypress will redraw the screen.

When charging, ChaDis7 will note the last time that the maximum voltage  was achieved. 'dT' is the time since this last point - effectively the  time since last full charge. It will also sense an unaccounted 0.5V+ increase that results in a battery level over 12V when the machine is not being charged (ie. if the machine is fully charged while switched off)

An interesting feature of the Netbook charging system is that there is some benefit in leaving the machine on charge for a while after it hits the apparent ceiling of 12.6V. You can test this by removing the power at various intervals after this point is reached, and making a note of the battery level after about 30 seconds. Longer charge periods do increase this level, though as yet I don't know the optimum figure.

Important
=========

For ChaDis7 to be effective, it has to remain running in the background. When performing a backup or software installation using PsiWin, the system will attempt to close ChaDis7 along with other programs. You can disable this by unticking the appropriate option in the General Settings dialog.

I haven't yet included any predictions of remaining battery life but will probably do so for a future version. Ideas for the best implementation are welcome and I would appreciate screenshots showing the characteristics of other machines with different ages, memory levels etc. to help me decide on the algorithm for this.

HISTORY
=======

v1.1
Altered the background routines so that the program takes up even less resources when in the background (it was 3%, now it's virtually undetectable, and certainly under 1%).
Changed the battery image to reflect a real Psion battery pack instead of an AA cell.
Changed the display of external/battery power to two graphic indicators, lessening the need for any alternative language resources.
Added a second way of sensing that a full charge has taken place, ie. an unexplained 0.5V+ voltage increase leading to a battery voltage over 12V
Added a menu to allow settings to be altered and program information displayed.

v1.0 Initial Release


Queries and comments should be sent by email to :-
ChaDis@millican.info

See also - http://www.kevin.millican.net/Home





