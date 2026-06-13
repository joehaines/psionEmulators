PsiTrain

Overview

PsiTrain a timetabling system, primarily intended for British Rail services, though not limted to this.  Indeed, it is possible to create your own timetables - and I encourage you to share them by placing a copy on my web page.

The shareware version only permits single leg journies, but still proves to be very useful. The unregistered version is restricted to 4 stations and 50 trains in each database.


Installation :

The following files should be copied :

\app\psitrain.opa
\app\psitrain\icons.pic      (optional, but recommended, file)
\app\psitrain\psitrain.pic (optional background)
\app\psitrain\psitrain.rsc (optional help file)
\app\psitrain\psitrned.opa (optional editor)
\app\psitrain\psitrned.rsc (optional help file)

\ptt\*.ptt                 (data files)
\ptt\*.stn
\ptt\*.trn

Data and program files can be placed on any disk.

PsiTrain will create psitrain.dat and defaults.dat files on the internal disk, in the \app\psitrain directory.

For a full installation, with PsiWin just drag the whole directory structure across to the root directory of the psion, or you can use PsiZip to extract all files from the zip file in the root directory. Then use Psion-I to install in the usual manner.

See the format.txt file for information on how to create your own timetable files.


Getting Started

Each database in te \ptt directory is shown under the PsiTrain icon.  Start PsiTrain by selecting one of them and pessing enter.

When PsiTrain is first loaded, you are presented with the journey query screen. (Defaults can be saved, later,  for each database using Psion-D.)

Enter a departure and destination station and press enter.  PsiTrain responds with the next train matching your query (if there is one in the example database.)  You can now find earlier or later services by pressing the left or right arrows respectively.

Now press the diamond key, and a summary of all services found (up to 5) will be shown.  The left and right arrows are still available in this screen. Press Enter, or the Diamond again, will return you to the detail screen for the selected service.

Press Psion-J for another journey (or Psion-F, for compatibility with other software), or Psion-R to search for a return journey.

See the online help for a description of the other commands available.


Engine Details

If the current date and time are used in the query, the next servicing matching the selected stations is returned.  If the current date and time is not used (ie not within 5 minutes) then the shortest service within 15 minutes of the specified time is displayed.  If no service is found within 15 minutes of the selected time, the nearest service is returned.


Registration

The full timetable engine is currently being built in C.  Once available, all supporters of the shareware product will be able to obtain the commercial version at a discounted rate.

The shareware fee is œ10, and will be used to fund the initial investment in hardware for the commercial version, as it is likely that the full database will require 4 Mb ROMs.

To register please send a cheque or cash to the address below.  (Foreign currencies are accepted for the equivalent of an additional œ2).

Please ensure you provide the registration name exactly as you would like it to be entered in the registration screen.  In return I'll email or send your personal registration code.

Thanks for supporting the product :

Mike Smith
118 Leake Road
Gotham
Nottingham
NG11 0JN

mike@boink.demon.co.uk
http://www.boink.demon.co.uk/

