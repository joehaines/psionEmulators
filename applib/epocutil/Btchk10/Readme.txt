Author          : Paul Bielski
Program Name    : BatCheck
Version Number  : 1.0
Released        : 16 August 1997
For Computer    : Psion Series 5
Memory Used     : 6K for files, ~64K while running
Application UID : &1BACEC10
Cost            : Freeware

Purpose:
--------
The Series 5 is lacking an adequate battery status indicator. This program can be configured to notify the user of the battery status whenever the program is accessed, the battery state changes, or the Series 5 is powered on. In addition, the display at startup can be configured to avoid interfering with other programs that may run when the Series 5 is powered on.

Installation:
-------------
1) Create a "BatCheck" folder in the \System\Apps\ directory on the C: or D: drive of your Series 5.
2) Copy all files except the Readme.txt file there.

(Please note that I have not tested the application from a D: drive, since I don't have a CompactFlash card yet. I believe it will work; I have logic in there to check C: first and then D: for the BatCheck.mbm file. The program works with the file on C: and if the file is missing altogether, so it should work from D: also. Please let me know if it doesn't work.)

Usage:
------
Select the BatCheck icon from the Extras bar. When run, a dialog box appears that presents the user with the following options:

"Notify on coming to foreground" - When this option is enabled, BatCheck will display a notice giving the current main battery condition whenever the program is brought to the foreground using the task list.

"Notify on battery change of state" - When this option is enabled, BatCheck will come to the foreground and notify the user of a main or backup battery change of state. For example, if the main battery state changes to "Low" from "Good", BatCheck will notify the user automatically.

"Notify on Series 5 switch on" - When enabled, this option forces the Series 5 to notify the user of the battery state when the power is turned on. On power on, BatCheck will come to the foreground, display a message, and then automatically return to the background. There are two sub-options associated with this:

"Delay after switch on" - This is the number of seconds that the Series 5 will wait after switch on before displaying the battery warning message. I added this option since there are so many programs that run on startup, and the order they come up in appears to be arbitrary (which is bad, since the BatCheck message can be quickly overwritten by another application). If the user makes the delay long enough (like 1 or 2 seconds), all other programs will start before BatCheck displays its message.

"Message Duration after switch on" - This option allows the user to set the length of time that BatCheck will spend in the foreground after the message is displayed. Once this duration has elapsed, BatCheck will return to the background and the program that was up before it will return to the foreground.

Whenever the program is in the foreground, the user can alter the preferences, send the program to the background, or close it using the toolbar, menu, or menu hot keys. In addition, pressing the Escape key will send the program to the background.

Things to be aware of:
----------------------
Preferences are not saved. You must reset them whenever the program restarts.

PsiWin will shut down BatCheck during a backup, so make sure you restart it after each backup!

I have not received a reply from Psion on my UID request, so I made one up. I will release an update when I get my official UID.

Disclaimer:
-----------
I have tested this application by itself and in conjunction with several other "power on" applications without trouble. However, I cannot guarantee that anyone else will not have problems with it. Use it at your own risk!

Contacting me:
--------------
My name is Paul Bielski, e-mail is pbielski@sprynet.com . Please feel free to contact me with problems, suggestions, or compliments ;-} I have been an avid Series 3a user for almost 4 years, and felt like it was time to give a little something back (that is hopefully useful!)

History:
--------

v1.0 - First release