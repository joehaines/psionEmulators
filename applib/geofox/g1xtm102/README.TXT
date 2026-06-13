This is Version 1.02 of XTM.

These release notes are provided to give late breaking
information and upgrade guidelines for XTM.  If you
are installing XTM for the first time, you MUST READ
THE MANUAL (manual.pdf - an Adobe Acrobat file) because
these notes do NOT tell you how to get started!

1. Upgrading to Version 1.02 from an earlier release

If you have used an earlier XTM any disk images you have
created will work under Version 1.02.  Performing an
install from your PC using the SIS file will update the
emulator.

2. Upgrading to Version 1.02 from a Beta release

If you are upgrading from a Beta release, you will also
need to copy the program XTMMOUSE.EXE onto your handheld
if you want to use the new mouse driver.

The released software includes minor changes to the
Beta font files to correct the "jagged lines" problem.
Unfortunately, EPOC32 appears to keep copies of the
old fonts in memory, even after the new font files have
been installed.  Thus, even though you have installed
the new font files as part of installing Version 1.02,
when you run XTM you still get the old ones.  The only
foolproof way we have found to make the old ones go
away is performing a "soft reset" of the machine.  Using
a bent paper clip, press the reset button on your
machine.  On a Series 5 it's the shiny hole in the corner
of the backup battery compartment.  On a Geofox-One it's
on the bottom near the speaker.

3. Changes from Version 1.01 to Version 1.02

There are no major functional changes in Version 1.02,
only minor changes to increase compatibility with certain
applications.

* The Int 5Fh graphics handler has been improved to make
  the acCIS 4.0 communications package paint the screen
  correctly.  This may affect other HP200LX applications,
  but will not affect normal DOS programs.

* The EPOC32 file system driver has been improved to support
  file access under DR-DOS for applications originally written
  for MS-DOS version 1.x.  MS-DOS users will not see any change
  in behaviour.

* The keyboard driver on a Series 5 now passes Shift-Tab to
  applications.

4. Changes from Beta 3 to Version 1.01

* A mouse driver is included which translates pen
  taps into mouse clicks.

* COM2 is available on a Geofox-One.

* XTM will still run when the COM port(s) are in use
  by another application or the remote link is on.

* Some minor BIOS and EPOC file server changes to improve
  XT compatibility.

* Error in font file which caused jagged lines in text
  mode corrected.

* Some minor CPU changes to improve Intel compatibility.

5. Known Restrictions

* On a Series 5, Alt key sequences are not always recognised
  by some DOS programs.

* The EPOCFS file server interface does not reset all its
  parameters when XTM reboots, leading to spurious error
  messages when some redirections are re-established.
