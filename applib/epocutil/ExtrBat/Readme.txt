ExtraBatt v1.01 - User Documentation

INSTALLATION
------------
You can use either your C: or D: drives (both directories, mentioned below, must be on the same drive), though it is recommended that you use the C: drive as ExtraBatt really comes into force when the main batteries are low and accessing the D: drive when batteries are low can be risky and somewhat unreliable.

Whichever drive you choose, you need to create two directories in the \SYSTEM\APPS area, these are \SYSTEM\APPS\ExtraBatt and \SYSTEM\APPS\1-TSpoon.

In  \SYSTEM\APPS\ExtraBat, copy;
   Back.mbm
   ExtraBat.aif
   Extrabat.app

In \SYSTEM\APPS\1-TSpoon, copy;
   All files with extension .baa and .baf
   1-TSpoon.aif
   1-TSpoon.app

RUNNING
--------
Once all of these are installed, you should see a battery with a 'No Entry' sign over it on the Extras Bar. Select this icon then, once the main window opens, press Extras again and watch the icon change in a few seconds.

As far as the menu is concerned, only the Scan Period needs any explanation (I feel). This indicates the rate at which ExtraBatt will scan for the battery's status. Under normal circumstances, you should be able to set this to Fast without loss in performance with other applications. If, however, you are experiencing loss of performance, set the scan to Medium (Default) or Slow.

Under my pre-release notes, I mentioned that there was voice notification but, doh! This only worked when there was enough battery (or external) power to run the speaker. This made the whole thing a stupid waste of memory so I removed it.

CHANGES IN BATTERY STATUS
-------------------------
When the battery status goes down, there will be two beeps, one of a slightly lower pitch than the other. I had intended to use samples here but the whole system was made more unreliable with the task of playing a sound.

When new batteries are inserted or, after the level going down, you haven't used the it for a while, the sound will be the reverse, that is, two beeps with the latter being of a higher pitch than the former.

KNOWN BUGS
----------
Maybe only my machine suffers from this problem but I've noticed that the battery status (even in the System), reads 'Good' until it suddenly flops like a deflated dinghy. This is still of some use though.

If the Psion is too busy during a battery's transition, the icon can sometimes become that crappy question mark. If this happens,simply exit ExtraBatt (using the Exit option in the menu bar), and restart.

If you have shut down ExtraBatt and the icon is still set to display the battery status (you really should only use the menu for this version), you will have to run \SYSTEM\APPS\EXTRABAT\ExtraBat.app and shut down through using the Exit option in the menu.

You can control the volume of the beeps through the Control Panel in System (I really should do something about this).

I haven't tried this one but I guess that if you're checking the status of the backup battery (by selecting the battery status icon when ExtraBatt is running) and the batteries change status there could be some trouble!!!!!

USERS OF PURPLE SOFTWARE'S NAVIGATOR
------------------------------------
You will need to create a shortcut to the \SYSTEM\APPS\1-TSpoon\1-TSpoon.app file, put it in your workspace and there you have it.

HOW THIS WORKS
--------------
There are basically two applications involved. One application comes in five versions each with a different icon which represents different states for the battery. These files are stored as .baa and .baf files.

The default application (the battery with a no entry sign), is different from the others in that when you select that, it launches another application, the ExtraBatt application, which has been set to not show its icon on the Extras bar.

As the ExtraBatt application polls the battery status, it changes the current 'icon' application as appropriate, as necessary. You can click on this icon for a report on the status of the backup battery.

DISCLAIMER
----------
It's not likely that these applications will damage your Psion. If they do, or you lose files because of them, it's not my fault. Let me know by all means, but I accept no responsibility for any damage, none at all. Zilch.