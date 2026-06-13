Further to the discussion on two S3a's communicating with each other by 
making sounds:

Some experimental programming was done within Psion some time ago, based 
on this very idea.  The research was never taken very far but what was 
achieved may be of some interest.  It's being released now purely as a 
piece of fun, and without any commitment or support from Psion 
whatsoever.

Important *** don't try running this software on an S3 classic ***

Warning: earlier versions of this software resulted in people's S3a's 
having to be reset.  These bugs are believed to be fixed now, but it 
goes without saying that you use this software at your own risk.  Back 
up all data first.

Another warning is that all the strange beeping involved is likely to 
drive you half mad fairly quickly!

What to do with the files
-------------------------
First of all, you'll need TWO S3a's.  Copy the .IMG file and the .PDD 
file onto the same \IMG\ top-level directory, and repeat for the second 
machine.  Select Btest from under RunImg and run it.  (Note that it will 
not run if another program is using the Serial Port.  Thus you will have 
to turn off Remote Link first.)

If you want to go on, press any key, when prompted, and then start 
typing.  Instead of the characters appearing on the screen, your S3a 
will emit strange tones.

By this stage, you should also have started Btest on the second S3a.  If 
you position the two S3as appropriately, keys typed on one will show up 
on the screen of the other.  By audio transport.

You may need to experiment to find the best positioning between the two 
machines.  Then see if you can conduct a conversation!

The test program also has the special feature that, if you press Ctrl-F, 
it will send out a whole long sentence of beeps and tones all at once.  
Try it and see.

The next stage
--------------
To go further, you will need to modify the test program BTEST.IMG.  The 
source for this, BTEST.C, is included with this zip.  This will built 
under the C SDK, in the standard way, to BTEST.IMG.  Over to you.

NB An Opl version of BTEST oughtn't to be too hard to write, but you 
will need to understand the concepts of Psion-style asynchronous i/o.

With either C or Opl, you could aim at putting a "beaming" layer over 
Link Paste ("Bring").  Eg write a program that captures the key 
combination Shift-Control-B in background.  Go to the database, 
highlight the whole record, and press Shift-Control-B.  The background 
program catches this, uses Bring code to get all the highlighted data, 
and pumps it out the audio port to the other machine.  Result: automated 
transfer of business cards, via audio, from one S3a to another.

The PDD
-------
Provided you exit BTEST cleanly, the PDD (Physical Device Driver) will 
be removed from memory.  Otherwise, to clear it out at any stage, start 
BTEST, and then answer the start-up query by pressing Psion-Esc.
