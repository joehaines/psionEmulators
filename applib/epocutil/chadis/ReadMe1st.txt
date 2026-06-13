ChaDis v1.04
============
Battery Monitor for the Psion Revo / Revo Plus
Freeware by Kevin Millican

Disclaimer
==========
The author can accept no responsibility for any damage to equipment or loss of data resulting from using ChaDis. Use of this program is solely at the user's risk and by using ChaDis you, as the user, imply your acceptance of these conditions.

Welcome
=======
Notwithstanding the above disclaimer, I hope that ChaDis will help you to protect your data and get the very best out of your Revo batteries !

Installation
============
Install ChaDis by double-clicking on the ChaDis.SIS file in Windows whilst your Revo is connected to the PC under PsiWin, or copy ChaDis.SIS to the Revo and tap it twice from the System screen.

Using ChaDis
============
Click on the ChaDis icon from the Extras menu. This will display the main ChaDis graph and status. Press the <Enter> or <Escape> key to hide the program.

You can use the <Menu> key to select a few other options.
The graph is initially based on default figures for realistic use. The Revo is capable of running about 12.5 hours, provided that you leave it on and don't press any keys (!) but 9hrs is more typical.
The light grey line shows the average current and is preset to 42mA.
The sloping diagonal plot shows a dark grey 3 pixel line for the region exceeding your last peak charge volts.

The plot changes to black for the monitored zone since your last charge.

The plot switches to a dark grey, 1 pixel thick line for the predicted remaining time.

Over time, the values used will accurately reflect your most recent usage characteristics and will allow you to assess the best time to recharge your Revo based on the way you use it and the real condition of your batteries.

Important
For ChaDis to be effective, it has to remain running in the background so it has been designed to stay running even if it receives a 'Close Program' message from the system screen or when you backup or install other programs using PsiWin. It is therefore recommended that you Backup your Revo once while ChaDis is not running. On subsequent backups when ChaDis is running, you will see PsiWin making three unsuccessful attempts to close ChaDis before continuing as normal.
To close ChaDis you can select 'Close' from its own menu, type <Ctrl-E> as usual while it is displaying its graph, or in an emergency, 'Kill' it from the System list of running programs (press a shift key while tapping the 'Close File' button).

It is also important that your Revo is switched on whilst it is in the docking station. This allows ChaDis to check for overcharging conditions and also ensures that the graph functions correctly, detecting your 'last charge' position.

HISTORY
=======

v1.04 Two new options have been added to the preferences:-
1. an alarm delay time. This prevents the alarm from sounding for a few minutes after 100% is achieved on fast-charge (most Revos switch down to trickle within this time)
2. the daily assumed loss due when the machine is switched off can be changed. 3% is probably sufficient for the original Revo but 5% is more realistic for the Revo Plus. If you want to set this figure more accurately, it is better to measure the real loss from around 50% charge than to do it from 100%.
SystInfo.OPX is now embedded in its own installer within ChaDis.SIS

v1.03 Added a separate time interval for the overcharge warning alarm. This can now be set to sound from the same frequency as the check interval up to ten minutes maximum. This can be used to prevent the alarm from sounding continuously if you are backing up and the Revo was already at 100% battery level.
Tidied up the graph drawing a bit. 

v1.02 Modified the menus by removing the 'Continue' item (just 'escape' the menu instead) and added keyboard shortcuts.

v1.01 Fixed minor bug that prevented ChaDis from starting if the Revo was being charged and at 100% the first time it was run.

v1.0 Initial Release



Queries and comments should be sent by email to :-
kevin.millican@altavista.net

See also - http://www.kevin.millican.net/Home





