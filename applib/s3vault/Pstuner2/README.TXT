PSTuner v2.0, © 1995, bruce linde/really desperate records


OVERVIEW

     PSTuner is a Psion 3a 'strobetuner' that determines pitch by
grabbing quarter-second samples and counting cycles/second, using
a couple of creative algorithms tailored to the Psion 3a's
sampling environment.
     It is extremely accurate (at a cost of some speed) and will
yield best results when fed clean, consistent notes (a note struck
too hard will ring sharp before settling in). Or, you can use
harmonics.


FILES

PSTuner requires three files, which MUST exist and be kept in
these locations:
        \APP\PSTuner.opa
        \OPD\Tuner.pic
        \Sys$samp.ldd   (must be at the root level)
In addition, Psion has authorized distribution of Sys$samp.ldd on
the condition that the docs and sample files (originally uploaded
by Psion as TSMP10.ZIP) are kept together. They are included with
PSTuner.


THE DRIVER - Sys$samp.ldd"

     PSTuner makes use of a driver provided by Psion. The docs
that come with the driver include the following disclaimers, which
are not as dire as they might sound at first read:

    "Psion will not be held responsible for any loss of data as a
result of use of Sys$samp.ldd, which is still to some extent in an
experimental phase. It has not been exhaustively tested, and can
not be relied upon, i.e., not to reset your 3a occasionally. It is
not an official Psion product."

	The worst thing that I've experienced is that it took a little
trial and error to ensure that PSTuner wouldn't leave the driver
loaded in memory. I was extremely careful about making sure the
driver cleans up after itself, but if this happens to you, you can
easily recover the 6k of memory the driver grabs for itself by
running the program and simply quitting before going into LISTEN
mode.
     As far as stability goes, I've actually left the thing
running for days with no problems. If you DO experience
weirdnesses, please let me know a.s.a.p.


SHAREWARE NOTIFICATION

     PSTuner came about as I was getting more and more into my 3a,
trying to grok OPL, and playing more guitar (more about music in a
minute). It occured to me that while I didn't always carry a
tuner, I almost always had my 3a with me.
     I wrote PSTuner to exploit the digital sampling and
programming features of the 3a, and because it was something I
could use. It is my hope that you, too, will find it useful. If
you DO continue using PSTuner, it is shareware...


SHAREWARE DETAILS

     All proceeds from PSTuner go to The Documentation of AIDS
Issues and Research Foundation. D.A.I.R. is a 501(c)3 non-profit
information archive used by Persons With AIDS and medical
professionals world-wide.
     Please send a tax-deductible donation of ANY size to:
          D.A.I.R.
          c/o bruce linde
          2840 60th av
          oakland, ca 94605


STUFF

     The 'Esc' key will get you out of most PSTuner screens. Help
is not available while in LISTEN mode. If you don't back up your
3a, either to a RAM card or another CPU, please re-read the help
file called THE DRIVER (above), and give backups some thought...
unless, of course, you have nothing to lose.
     I am not responsible for out-of-tune performances or loss of
data resulting from use or misuse of PSTuner or the Sys$samp.ldd
driver. Oh, and don't forget that PSTuner is shareware. Thanks!


HARMONICS

     Every note consists of a fundamental, and harmonic overtones
(thirds, fifths, octaves, etc.). These 'harmonics' can be isolated
on a guitar (or bass) by partially muting a string at the twelfth,
fifth, or seventh frets, as you pluck the string. This dampens the
grosser vibrations of the string, letting the higher-pitched
components of the note (harmonics) ring through. 12th and 5th fret
harmonics produce higher octaves of the fundamental; 7th fret
harmonics yield fifths.


MUSIC STUFF

Write or e-mail for more information about:
"bruce linde: not like other men", an independent release on
really desperate records, and "Beggars and Saints" and "Monkey",
the most recent releases by Jai Uttal and the Pagan Love Orchestra,
on Triloka Records (w/ bruce on bass):

     really desperate records
     2840 60th av
     oakland, ca 94605
     
     www.songwriter.com
     brucelinde@aol.com

----------------------------
Credit Where Credit Is Due

PSTuner is my first program. It was written with considerable
support and guidance from Master Programmer/Lode Runner Ron
Hunsinger. Every novice diving into the deep end of the
programming pool should be lucky enough to have a Ron as their
lifeguard... thanks also to: Jamie Pyper@Psion S.F., for turning
me into a 3a junkie, Jack McCrae, for kick-starting me by sharing
his Blackjack source code, David Wood@Psion U.K., for uploading
the Sys$samp.ldd driver and documentation, and, whoever wrote the
3a OPL Programming Manual!

bruce linde
july 15, 1995

----------------------------
History
2.0			thanks to John Woodthorpe i found out what was
			causing soft resets on some 3a's. somewhere along
			the way the driver got corrupted in the v1.1 upload,
			going from 6080 bytes (correct) to 6095 bytes (wrong!).

			apologies to anyone who got the corrupted driver!

			i also made the main window a little more user friendly 
			and fixed some minor bugs/dumbnesses.
			
			
1.1			minor bug fix to allow running app from any drive 
			(all files must still be on the same drive)
			
1.0			6 months ago i couldn't even spell 'programmer',
			and now i r one!
