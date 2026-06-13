CaseTime version 2.0 for the Psion Siena, 3a, and 3c.  Copyright
(c) 1999 Dr Michael Brewer


1. Overview
-- --------

CaseTime is a multiple timer application, providing timers for use
in keeping track of time spent on multiple tasks.

It was written to be as compact as possible, without much of the
baggage that may come with other similar applications --- it
doesn't have fancy time-management functions for example.  It is
assumed that you keep track of all that by some other
already-established method (paper?).  As a result, the opa file
won't be a very big memory hog on your Psion (which is undoubtedly
straining under the load of all those games you've crammed onto
it).  In addition, it is extremely cpu-friendly.

It does, however, have the capability of handling multiple timer
"files", which can be created, saved, opened or deleted in a
similar fashion to other Psion applications, and more than one
timer file can be open at once.  This gives added flexibility to
the ways in which CaseTime can be used --- for example, tasks
could be grouped according to type, and a separate timer file
could be maintained for each group.

It can be used to be used to keep track of the time spent on
active cases which you might be working on, until such a time as
you are ready to enter the time spent on these cases into your
external case management system.

CaseTime is designed mainly (but not exclusively) for those of you
who are in a job where clients are charged by the minute.  So it
may be of use to solicitors, patent agents, and the like.  Of
course, you may find other uses for CaseTime... for example timing
how long each of your daughters is spending on the phone... or...


2. Installation
-- ------------

(a) Unzip casetime.zip.  It contains application file casetime.opa and
    this file readme.txt.
(b) Copy casetime.opa to the app\ directory on your Psion.
(c) Install CaseTime on your Psion using the "Install" menu item
    under the "Apps" menu in the system screen (Psion-I key).
(d) Move to the CaseTime icon, and press Enter.  If the directory
    "\CST\" does not already exist, it will automatically be
    created.


3. Usage
-- -----

The current version has nine timers, each of which has two
associated labels (described respectively as a "label" and a
"code") so that you can identify each case.  As an example, the
"label" may be used to identify the case, and the "code" may a
time-management code to identify what work is being done on that
case.

Select a case by moving the pointer up and down with the up and down
arrows respectively.

Press "c" to start (or continue after pausing) the selected case's
timer.  A dot appears to indicate that the case is being timed.
Press "p" to pause it.  The dot is removed.  Press "r" to reset
the selected timer, and "shift-r" to reset all timers.

In order to be very cpu-friendly, CaseTime does not refresh the
display continuously.  This is so that your Psion can still auto
switch off when no real activity is occurring.  In this way, you
can set a timer going, switch off, and only switch back on when
you need to stop it or have a look.

The display is refreshed at various opportune times such as when
"continue" or "pause" are pressed, and when the CaseTime
application is switched to the foreground.  The only time that the
displayed times may need manually updating is when: (a) a timer is
active; and (b) the application has remained on and in the
foreground for a short period of time.  The user is alerted to
this by a flashing "+/-" symbol in the top left-hand corner of the
screen.

To refresh the display manually, simply press "space".

Edit the label or code for the selected case by pressing "l" or
"k" respectively.

Delete the label or code for the selected case by pressing "d" or
"w" respectively.  To delete all labels or codes, press "shift-d"
or "shift-w" respectively.

The default at start-up is to allow only one active timer at a
time.  When you start a timer (using "c"), and move to a different
case, the timer from which you have just moved is paused, and you
may then start a new case's timer.

If you would like to allow more than one active timer at a time,
toggle this feature with "t".  The current timer will then not pause
when you move off it, allowing you to start multiple timers.  This
mode of operation is indicated by a dot to the top left.

Each timer can operate either in the forwards or backwards
direction.  To switch to the reverse direction, press "," (same
key as "<" on most keyboards) or the left arrow.  To switch back
to the forwards direction, press "." (same key as ">" on most
keyboards) or the right arrow.  The cursor changes to indicate the
direction of time for the selected case.

Time may also go negative.  If the time is negative, the ":" which
separates the hours from the minutes changes to a "*".

One possible use for this "negative" time is to credit a case with
time (for example time already charged but not yet spent). For
example a case having negative time, with the timer operating in
the forwards direction, will tick gradually towards zero and then
into positive time, positive time indicating that there is no time
left in credit and the displayed time is yet to be charged.

Note that, although not displayed, CaseTime keeps track of elapsed
seconds for each case too.  To display a timer's elapsed seconds,
press "e".

To add a minute to the current timer, press "+".  To subtract a
minute, press "-".  To add five minutes, press "*", and to
subtract 5 minutes, press "/".

To zap an entire line, press "z".  To zap everything, press "Z".

When a timer is continued or paused, there is an option to add
automatically a certain amount of time to that timer.  This may be
useful if you are a lawyer and you feel that a certain amount of
additional time should be charged for the inevitable time spent
messing about before and after actually starting work proper on a
case.  Press "shift-o" to set these.

When one or more timers is active and the Psion is switched on, a
little beep is emitted to warn the user that there is a timer
ticking away lest they had forgotten.

To turn off this beep (and various other beeps emitted by
CaseTime), go to the above-mentioned options (press "shift-o").

You can cycle through various fonts by pressing "f" (cycle
backwards with "shift-f").  Toggle bold with "shift-b".  Toggle
italics with "shift-i". Return to normal font style with
"shift-n".

For information about CaseTime, press "shift-a", and to register
press "g".

CaseTime handles multiple timer files in a very similar way to the
way other Psion applications handle their own files.  Files can be
opened, created or quit from either the system screen or from
within CaseTime.

Within CaseTime, press "o" to open a file, press "n" to create a
new file, press "s" to save the current file with its current
name, and press "a" to save the current file under a new name.  To
exit CaseTime, press "x".

All the commands mentioned above can be activated using either
just the stated key, or with the Psion key together with the
stated key.  Of course, in addition they may be activated via the
menu (press the menu key).

Remember you can also get rid of the status window to save screen
space (for example by pressing "Fn-menu" on the Siena).

Timer files are stored in the \CST directory of the Psion's
internal drive.  If this directory does not exist on start-up of
CaseTime, it is automatically created.

A small amount of help is available by pressing the help key.


4. Contact information
-- -------------------

The author may be contacted via email at DrMRBrewer@hotmail.com,
or you may write to me at:

  Dr Michael Brewer
  Top Floor Flat
  25 St Luke's Avenue
  London  SW4 7LG

Note that this address is likely to change more often than my email
address, so please try emailing me first!

Or you could even phone me on +44 (0)20 7627 8387.  Again, this number
may change.


5. Homepage
-- --------

The homepage for CaseTime is at:

  http://www.robots.ox.ac.uk/~mrb/casetime.html


6. Registration
-- ------------

CaseTime is shareware, so if you find it useful then please register
it.  The registration fee is not large, but will at least help me to
justify to my wife why I spend so much time on my Psion!

To register, follow the instructions at the above-mentioned homepage.

In summary, you can either mail 5 UK pounds or 10 US dollars to the
above-mentioned address (note warning about address).  Please give
full return postal details, and preferably your email address for
speedier receipt of the registration code.

Or you can pay by credit card from any country by using the regnet
service --- visit the following site directly:

  http://www.reg.net/product.asp?ID=2654

Unregistered versions of CaseTime are fully working, so what you
see is what you get.  When you register you will receive a
registration code which will remove the nag screen which is only
displayed: (a) when you start and end the program; and (b) when
any active timer goes beyond one hour and the screen is refreshed.
The same registration code will be valid for all future versions.


7. Distribution
-- ------------

CaseTime version 2.0 may be distributed freely, so long as both
the application file (casetime.opa) and this file (readme.txt) are
included unmodified in the distribution.  On no account may a
registration code be used by any person other than the rightful
owner.


8. Disclaimer
-- ----------

This program is provided "as is" without any warranty, expressed
or implied, including but not limited to fitness for a particular
purpose.  Under no circumstances will the author (Michael Brewer)
be held responsible for loss of data, damage to software/hardware,
or any other type of loss or damage as a direct or indirect result
of using this software.


-----------------
7 December 1999
