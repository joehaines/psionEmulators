=======================================
Speaking Clock - For the Psion Series 5
Version 2.00
=======================================

By Stephan Nicholls: stephan.nicholls@cableinet.co.uk
Web Site:            www.wkweb5.cableinet.co.uk/stephan.nicholls

This program started off as a little OPL program from David Pollard
(see 'Notes' section for more information). I started playing with it
to start learning OPL and ended up with Speaking Clock.

Features
========

* Tells you the time when you switch the Psion on or when you press
  the external 'play' button (dead handy at night). 
* You can specify whether to tell the time in 12 or 24-hour clock 
  format. 
* Volume is selectable (but not a lot of difference). 
* You can record your own sounds (numbers) and even have multiple
  sets in different languages and specify where Speaking Clock should
  look for the sounds. 
* Best of all it's totally free. All I ask is that you take the time
  to sign my guestbook and e-mail me a smile :-).

Known Issues
============

* At the moment the external play button works because it switches the Psion on.
  If the Record application is not running then that is started and Speaking
  Clock is put into the background. To combat this Speaking Clock puts itself
  back into the foreground. I would prefer Speaking Clock to return to the
  start-screen if put into background by the user rather than record, but I can
  only do this if I can detect that the Psion was switched on by using the play
  button - any ideas how I do this?
* All this doesn't make Speaking Clock as multi-tasking 'friendly' as I would
  like - any suggestions?
* I don't know whether this will work on the GeoFox, anyone who has tried
  Speaking Clock out on one please let me know how it goes.

I am a novice OPL programmer so suggestions on how to resolve the above are
welcome by e-mail. I'm also interested in any suggestions for improvements or
anyone who can give me a comprehensible explanation of GETEVENTA32 and exactly
how events shoot around on a Series 5.

Disclaimer
==========

I've had no problems with Speaking Clock on my Series 5, but I make no
promises, i.e. don't blame me for lost files, data, high-score tables
etc!

Installation
============

(1) Unzip the files


(2) On the Series 5 disk you wish to install on, make sure viewing of the
    \System folder is turned on and then go into \System\Apps and make a
    new folder \SpeakClk. Then you should now have a folder whose path reads
    something like \System\Apps\SpeakClk

(3) Use PsiWin to COPY the SpeakClk.app, SpeakClk.AIF and SpeakClk.INI files
    into this new folder, by dragging and dropping them using the *right* mouse
    button. Using this button brings up a mini-menu from which you can select
    "Copy". (Note that you DON'T want to CONVERT any of these files, they're
    all already in Series 5 format!)

(4) Copy all the number files to a directory, I use C:\System\Apps\SpeakClk\Wves.

(5) If all went well, you should now have a new "SpeakClk" icon on your
    Extras bar.

To Use
======

To run just select SpeakClk from the Extras bar.

When you start Speaking Clock you get a start-screen giving you a choice
of cancelling, starting, displaying help screens or setting preferences.
If you haven't put your wave files in my suggested directory you will have
to specify the location. You can also change the volume and whether the
time is told in 12-hour or 24-hour format.

When you select start the Psion switches itself off, every time you switch
it on (by pressing 'Esc' etc) you will be told the time and then the Psion
will switch itself off again. You can use the external play button as well
(you can use the record button, but you'll get an extra beep). Note my
comments under 'Known Issues' if you use the play button.

To cancel Speaking Clock, press any KEY whilst you are being told the time.

Notes
=====

There's no reason why you can't record your own sound files so that Speaking
Clock tells you the time in your choice of language. Use the preferences menu
to point to the location of your files. If you do I'd love a copy e-mailed to
me, especially if you're happy for me to make them  available on my web site.

The original OPL which started me off was by David Pollard:

E-mail:	DavidP@mtc.sund.ac.uk
FTP:	mtcnt2.sund.ac.uk (login as anonymous, password your email address)

Thanks to him for making it available.


