Welcome to Tune Pal!
====================

Thanks for downloading and trying out my program. Tune Pal is a player for .abc music files for the Psion range
of handheld computers .abc files, if you don't already know are commonly used by traditional Irish, Scotts and Breton
musicians to share tunes. You can find more information about the file format and download
more tunes from the following sources:

http://www.gre.ac.uk/~c.walshaw/abc2mtex/abc.txt - File format description
http://home.swipnet.se/%7Ew-11382/abcmus/ ABC Mus home page - A popular Windows ABC Player
http://home.swipnet.se/%7Ew-11382/index.html - Thousands of tunes in ABC format

There are hundreds of ABC resources on the Internet. The above is just a small selection of links.

About this program
==================

This program should run is an ABC player for the Psion 5, 5MX, Revo etc. It will not run on a Series 7 or NetBook.
It supports about 90% of the ABC notation. For example, it will play:

* Tunes in any major, major, minor, mixlodian, dorian key.
* Sharps flats and naturals
* Repeats
* Variations
* Rolls
* Tuplets
* Searches for tunes

That means it plays most reels, jigs, hornpipes etc. What is not supported, because of speed and time considerations
are:

* Bibpipe key
* The << >> characters
* Key changes in a tune
* Meter

I will probably add support for the above items, if there is any demand for it. If you like the program and there are particluar
features you really would like, then let me know and I'll see what I can do.

Please note, this software is completely free and without any warrenty. You install this program at 
your own risk. Back up your Psion before installing any new software!

Installation
============

The archive file this program comes in consists of 5 files:

TunePal.sis
Music.sis
SystInfo.sis
Date.sis
readme.txt

Double click on each of the .sis files to install each of the modules onto your Psion. That's it.

Using
=====

The program comes with a sample ABC file to get you started. By default the program looks in /ABC to find 
ABC files.  Start the program by clicking on the TunePal icon from the extras bar. After you dismiss the about screen,
the program will display a blank screen. I will probably put a picture there at some stage, but to get the program
to do something, press the menu key. A note on the menu choices:

1. Play a tune - This plays a tune. You can press tab on the tune choice to bring up a list. You can also press
the first letter of a tune repeatedly to find the tune. Once a tune is playing, press the up or down arrows to speed 
up and slow down. Pressing "P" pauses a tune, ank key restarts and pressing ESC stops a tune playing.

2. Load a tune file - This lets you load a different ABC tune file.

3. Find a tune - This option allows you to search through a group of files to find a tune
It recursivelly searches through a directory looking for the tune. 

4. Font Size - This changes the font which is used to display the tunes.

5. Transpose - You can transpose up and down a semitone.

Editing ABC's on a Psion
========================

TunePal is just a player, however if you want to edit .abc files on a Psion, you can use the Epoc text
editor, which you can download from http://www.symbiandevnet.com/downloads/sdks/er5sdks.htm

Known Issues
============

1. As specified above, some of the notation isn't supported yet. I havn't put much in the line of error handling into
the program, so it is possible the program will crash if it comes across something it doesn't recognise. Don't worry - 
it wont harm your Psion.

2. The programs method for playing naturals is a bit screwy. Sometimes it makes mistakes!

3. I don't take account of the tune type or meter when a tune is playing. Just use the up and down arrows to adjust the speed.

Updates
=======

If you are interested in receiving notifications about updates to TunePal, send an email to TunePal-subscribe@yahoogroups.com to 
subscribe to the mailing list. I am planning to send an email every now and again whenever I have made an update to the program.
You can unsubscribe at any time and I will not share your email address with anyone!

Version Changes
===============

Version 0.4 - 12th May 2002
1. New improved UI
2. Now stores volume setting in the config file
3. Added repeat option and stores value in the config file
4. Fixed a bug which caused program to hang on some minor key tunes
5. Can interrupt a serach by pressing esc and get a partial list of matches
6. Expanded the range of notes the program plays (was causing the program to crash with some tunes
7. Fixed a bug in roll playing where low or high rolls wouldn't play
8. Note length is now an option on the Options menu
9. Fixed a bug with lengths of high and low notes
10. Pause resolution is now much higher leading to better timing
11. Many other busg fixes

Version 0.3 - 19th September 2001

1. Added a background image and changed the way tunes are displayed.
2. Tidied up the keyboard handling.
3. Added a few more tunes to the sample tunes file.

Version 0.2 - 5th September 2001

1. Fixed some timing issues related to tuplets
2. Now ignores embedded chords correctly
3. Saves and loads configuration. It now remebers what the last file and tune you listened to was, transpose settings
   and speed.
4. Find in files now works correctly
5. Added the transpose option.

Version 0.1 - 28th August 2001

Initial Version

And finally...
==============

If you like my program, then please let me know by sending me an email to:

skooter500@yahoo.co.uk

I'd appreciate it. If you really like it and sure why wouldn't you, then feel free to send me a present of some sort. Send me an email and I'll
give you my address.

This software is completely free. If you ask me nicely, I'll even send you the source code. If you plan to make enhancements, then I'd appreciate
if you could send them back to me for incorporation into future versions.

Thanks to RMR Software for Music.opx
Thanks to David Aldred for the UI enhancements.

Slán go foill,

Bryan Duggan
12th September 2001
