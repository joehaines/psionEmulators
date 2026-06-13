PsiTrain
~~~~~~~~
Overview
~~~~~~~~
PsiTrain a timetabling system, primarily intended
for British Rail services, though not limted to
this.  Indeed, it is possible to create your own
timetables - and I encourage you to share them by
placing a copy on my web page.

See the format.txt file for information on how
to create your own timetable files.


Installation
~~~~~~~~~~~~
The following files should be copied to the
specified locations :

\app\psitrain.opa
\app\psitrain\icons.pic
\app\psitrain\psitrain.pic (optional background)
\app\psitrain\psitrain.rsc (optional help file)
\app\psitrain\psitrned.opa (optional editor)
\app\psitrain\psitrned.rsc (optional help file)

\ptt\*.ptt                 (data files)
\ptt\*.stn
\ptt\*.trn

Data and program files can be placed on any disk.

PsiTrain will create psitrain.dat and defaults.dat
files on the internal disk, in the
\app\psitrain directory.

For a full installation, with PsiWin just drag
the whole directory structure across to the
root directory of the psion, or you can use
PsiZip to extract all files from the zip file
in the root directory. Then use Psion-I to
install in the usual manner.


Getting Started
~~~~~~~~~~~~~~~
Each database in the \ptt directory is shown under
the PsiTrain icon.  Start PsiTrain by selecting
one of them and pressing enter.

When PsiTrain is first loaded, you are presented
with the journey query screen. (Defaults can be
saved, later, for each database using Psion-D.)

Enter a departure and destination station and
press Enter.  PsiTrain responds with the next
train matching your query (if there is one in
the example database.)  You can now find earlier
or later services by pressing the left or right
arrows respectively.

Now press the diamond key, and a summary of all
services found (up to 5) will be shown. The left
and right arrows are still available in this
screen. Press Enter, and you will be shown a
summary of the entire service.  Press left/right to
look at other services. (In this mode, the services
will not match the query ... you are just
browsing the database.) Now press Esc to
clear the service display, 

Press Psion-J for another journey (or Psion-F,
for compatibility with other software), or
Psion-R to search for a return journey.

If the departure/destination station is left as
"Not Set" on the journey query screen, a
selection screen is shown allowing up to 5
stations to be entered.

An "Advanced" options screen is available from the
journey query screen by pressing "Menu".  This
enables Via/Avoid stations to be set; Whether
only direct services should be returned (for use
with British Rail extracts); and the ability to
automatically return the details of up to 5
train routes, either before or after the specified
lookup time.  The options on this menu are
retained for new journey queries, and only reset
when a new timetable is loaded.

See the online help for a description of the
other commands available, and icons.


Journey Options
~~~~~~~~~~~~~~~
Several fields are available to define a query
to find a specific service.

Date: Press tab to see a month, 3 month or year
view, progressively.

Via: This will cause PsiTrain to only return
journies which pass through the specified station,
which requires that station to be present in the
train definition record.
It is NOT currently used to build more complex
multi-leg journies, and is not appropriate for
UK Railways extracts.

Avoid: No journies are returned which pass through
this station.  Good to want to avoid bumping into
your boss, for instance !

Route: Can be used to restrict the search to
direct routes only. Mainly used in conjunction with
UK Railways extracts, which have a "change at"
comment in the train description field where
appropriate.

Display Services: Normally only one service is
returned.  Using this option, up to 5 services
can be returned at once.

Later/Earlier: Used in conjunction with the
option above, additional services later or earlier
than the specified date and time are returned.


Engine Details
~~~~~~~~~~~~~~
If the current date and time are used in the
query, the next service matching the selected
stations is returned. If none is found or if the
current date and time is not used (ie not within
5 minutes) then the shortest service (in travelling
time) service within 15 minutes of the specified
time is displayed. If no service is found within
15 minutes of the selected time, the nearest
service is returned (and may therefore not be the
shortest).


Registration
~~~~~~~~~~~~
The shareware fee is œ10, and will be used to fund
further developments in PsiTrain.

To register please send a cheque or cash to the
address below.  (Foreign currencies are accepted
for the equivalent of an additional œ2).

Please ensure you provide the registration name
exactly as you would like it to be entered in the
registration screen.  In return I'll email or send
your personal registration code.

Thanks for supporting the product :

Mike Smith
118 Leake Road
Gotham
Nottingham
NG11 0JN

mike@boink.demon.co.uk
http://www.boink.demon.co.uk/


Legal Mumbo-Jumbo
~~~~~~~~~~~~~~~~~
Please note that registration does not imply or
guarentee support, warranty etc.  Any supplied
databases (both with the product and on the
support website) are examples only, and should
not be relied on for real journies.

PsiTrain Copyright is held by Mike Smith.

