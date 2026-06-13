WordBox version 1.2 (18Aug98)

INSTALLATION INSTRUCTIONS
-------------------------

1. Create \WordBox folder in your \System\Apps folder.
2. Unzip WordBox.zip
3. Copy all files EXCEPT spell.sis to the newly created 		\WordBox folder.
4. Install spell.opx by using the EPOC installer.  You can do this either via your windows desktop/PsiWin by double clicking the.sis file, or directly from your psion by using the Add/remove icon on the System control panel.  
5. Run WordBox from your EXTRAS bar.
6. Peace and love spread throughout the world...


FILE LISTING (14 files)
-----------------------

Click.snd
Dwop.snd
GameOver.snd
ListBox.opo
readme.txt
spell.sis
SuperTot.snd
Tot.snd
Twang.snd
UhOh.snd
WordBox.aif
WordBox.app
WordBox.hlp
WordBox.mbm


REGISTRATION
------------

WordBox is a labor of love, and as such registration is free!  Which means there is no registration required!  Enjoy!  Have an exclamation mark, on me!

You can also send requests or questions directly to the author via email: kevins@phonet.com

WordBox is Copyright © 1998 Twin Feats Software & Kevin G. Smotherman

All rights reserved.

=========================================================

KNOWN BUGS
----------

1.	Bottom-most element in bottom-right most high score
	listbox does not display unless screen is tapped or
	key is pressed.  This appears to be an OS bug.
2.	Sometimes toolbar button's text clears some of the
	pixels in a rectangle around the text.  This appears to
	be an OS bug.

----------------------------------------------------------

CHANGELOG
---------

Version 1.0 (5Jun98)

1Jun98	Pausing the game then unpausing caused a partial
		overwrite of the last word status displayed.

Version 1.1 (19Jun98)

 7Jun98 	Malformed elapsed time on Words display for
			untimed games.
 8Jun98	Before opening spell.opx, WordBox now checks the
			Spell application and mimics the user dictionary
			setting found there.
 9Jun98	Removed PenSensitivity setting control from
			NewGame dialog (it's still in prefs)
12Jun98	On the Cancel Player dialog, the
			MOVE player to last up option was not resetting
			the new current player's time correctly.
15Jun98	Words are now passed to the spell checker as all
			upper case.  This allows spell checking of words
			that are in the dictionary capitalized.

Version 1.2 (18Aug98)

18Aug98	If WordBox was powered off mid-game for more
			than 32767 seconds (9.1 hours), it would crash
			when the S5 was powered back on.
18Aug98	New spell.opx from RMR software installed that
			fixes the infamous crash if the user dictionary
			was non-existant or empty.

--------------------