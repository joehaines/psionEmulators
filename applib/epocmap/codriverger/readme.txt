                           CoDriver Installation

                     In-car navigation for the Psion 5.

Installation

The program is downloadable as a "zip" file, which contains this "readme"
and the "sis" installation file for the Psion. Once "unzipped" (using the
"unzip" program widely available for the PC or Psion) all that needs to be
done is to double click on the "sis" file on your PC, or double tap on it
on your Psion, and the program should be installed on your Psion.

Note that this does not work if you have the old (2.0) version of PsiWin.
If this is the case, the easiest option is to download "setup.exe" from
Psion's website -
 http://www.psion.com/downloads/epoc32/epocinstall/setup.exe
Once installed on your Psion, this gives you an "install program" option,
which can be used to install the "codriver.sis" file. Hope that's not too
complicated!

Once installed, CoDriver should appear as an icon on the "Extras" menu and
can be run from there. Detailed help is available from within the program
by choosing "Help" on the tools menu.

The program will initially load a example route, but you will want to
import a route from your own routefinding program before you use the
program to take you anywhere. Choose "Import" from the File menu, and
follow the instructions from there. Note, importing routes is the most
complicated part of CoDriver, and you might like to read the "Help" first!

Registration

Registration costs £15 and removes the nag screen, entitles you to use
beyond an evaluation period and encourages me to go on developing the
program.
You can register by post, address given in the program, or online by credit
card at http://www.reg.net
The Reg.net id number is 4327.

Contact

I can be emailed at patrick.fox@virgin.net.
The program is ©1999 Patrick Fox
Feel free to link to the program at
http://freespace.virgin.net/patrick.fox/codriver/


Version History


 9/3/99     Version 1.00 released.

 10/3/99    Distribution file altered to "sis" - thanks to Steve Lichfield
            for his help in this.
 11/3/99    Version 1.01 released.
            * Bug fixes -
            Import from Psion RoutePlanner was crashing more often than
            not. Changed so RoutePlanner uses the LaserJet III printer
            driver (slower but more predictable than the General one) and
            rewrote the import routine accordingly.
            Error during import no longer leaves a blank screen but
            reverts to beginning of previous route.
            Non-open RoutePlanner produces prompt.
            Recognises French and German "Left"/"Right", although
            automatic import from RoutePlanner still fails in these
            languages due to different keyboard shortcuts. Manual import
            works.
            The .ini file couldn't be read for Continental users who use
            commas as decimal points. Fixed.
 13/3/99    Version 1.02 released.
            * Bug fixes -
            Progress bar now keeps correct position on route when stepping
            backwards manually (it was getting left behind before).
            Reinstated error handling for Import (which had accidentally
            got turned off while I was debugging 1.00 and not turned back
            on!). Also made it rather more informative depending on the
            likely type of error.
            Autoswitchoff is now only disabled while the program is in the
            foreground.
            * New Features -
            Limited international support. CoDriver can now import
            automatically from German and French versions of RoutePlanner.
            (Note it still gives you instructions in English, but it will
            now at least work!)
 14/3/99    Version 1.03
            * New Features -
            When road numbers are displayed by RoutePlanner as eg A20/E15,
            CoDriver now takes the first of these not the second (which
            doesn't seem to be used anywhere. Does anyone use these E
            numbers?!).
            Greater accuracy in GPS mode with RoutePlanner routes (2%
            "fudge factor" added, since RoutePlanner usually
            underestimates distances slightly).
            Added registration details for online registration through
            www.reg.net.
 15/3/99    Version 1.04
            * Bug fixes -
            Program no longer occasionally crashes with "Kern Exec" or
            "Comm Server" errors.
            * New Features -
            Extra options to turn beeps on/off and to adjust the size of
            the "RoutePlanner fudge factor" - see above.
 21/3/99    Version 1.10
            * New Features -
            CoDriver can "guess" turn direction based on quoted compass
            heading when direction not stated by RoutePlanner.
            Pause GPS input option, to allow GPS use by other programs (eg
            RoutePlanner).
            Dutch language sound files available.
 24/3/99    Version 1.11
            * New Features -
            Support for roadnames beginning "R..."
            Choose first or last roadname when RoutePlanner gives a list
            (eg A20/E15).
 30/3/99    Version 1.12
            * Bug fixes -
            Program was failing to recognise unclassified roads from MS
            AutoRoute - fixed.
            Registered users using a GPS were still getting an occasional
            nag screen!
            * New features -
            Audible "No GPS attached" alarm in GPS mode.
 15/4/99    German Version released as 1.10D
            * Basically the same as English version 1.10, it incorporates
            the bug fixes from 1.12 but not the new features added to 1.11
            or 1.12.

                                    Back
