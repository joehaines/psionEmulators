***************************
Data Query Express Plus
Version 1.0 A, 1999
for PSION Siena and Series 3 (mx)
***************************
Welcome, please check first, wether you have unzipped this files:
- readme.txt
- liesmich.txt
- Dqep3e.opa     DQEPLUS for  Series 3 (english language)
- Dqep3d.opa     DQEPLUS for Serie 3 (german language)
- DqepSe.opa     DQEPLUS for Siena (english language)
- DqepSd.opa     DQEPLUS for Siena  (german language)
- dqehilfe.rsc
- dqehelp.rsc
***************************
Please read these instructions attentive, bevor you start the program.

1. Overview
This programs allows you a fast access to database-queries.
You can use this programm for databasefiles, established with
the PSION-database-application.
The great adavantage of DQEPLUS is that you can start an
query directly from PSION-System-sreen, once you have
created a query. But there are some additional functions,
like calculation and searching an replacing ...

2. Installation
Copy the files "Dqeplus.opa" into the directory:
"M:\APP" (onto the internal disk of the PSION!)
Press PSION-I for installing the programm and choose
DqepXXX.opa.
Starting the program will establish directory "M:\APP\DQEP"
Copy dqehelp.rsc into "M:\APP\DQEP" directory.

3. Quickstart
Start the programm bei "clicking" Dqeplus under the programsymbol.
After program starts you now choice the database, wich you want
to analyze.
After this you can i.e. start searching by pressing PSION-S.
Fill in your criteria and press RETURN.
Searching starts and you will get a result in form of a database-
result-file. All matches will appear in this file at once.
Close this resultfile by pressing "PSION-X". IF you now
close the Dqeplusprogram by pressing "PSION-X" your
profile is stored.
If you choose Dqeplus on the sytemsscreen, your profile
will be executed and you will get the results on the screen.
Note: If you close the resultfile the program will close too.

3. A. Statusindicator
On the left bottom of the screen you can ever see a statusindicator,
wich informs you about taken action.
During program action on the rigt bottom of the screen appears
an indicator, that informs you about the progress of an action.
The statusdisplay may show:

"X": This means no action is taken
"S": Searchmodus
"F": Filter is activated
        This can also appear in combination with other symbols
"DN": Date- and Numberfunction
"CA": Calculation
"ST": Statistics
"SR": Search- and replace
"FR": Frequencies
Additional Profiles wich uses the "exportfunction" are shown
in statusindicatorwindow by an additional little "box".

The progressindicator gives you information about the actual
datarecord proceeded, total number of records and in some
cases about the numbers of matches.

4. Description of functions

NEW PROFIL
Gives a query a name to execute it from systemscreen.
This is only the first step to store a profile, storing is only
available if you execute a search or some other action.
The creating of a new profile follows this order:

a) NEW Profil
b) Choose a database
c) Execute a searchprocess or another action
d) Save the profil
or
press "save an exit"

The way of creating a profile is like recording a macro:
First you record your action an then you are able to store it.
As an alternative you can create an new profile from systemscreen
of your PSION. This is the same way that uses other PSION-
applications. But: In DQEPLUS you have the possibility to
give your profile an description, witch allows you to have a
notice that contains more than the eight characters of the
filename. If you choose to create a profile form systemscrren
(PSION-N) then you have the possibility too to store an
additional remark, typing "New profile" again within in
the program. In this case "your" profilename will appear
in a dialogbox and you will be able to type in a description
of this profile.

STORE PROFIL
This stores your profile.

REJECT
This sets back all parameters, an open database remains open.

Execute Profile
This function executes an given profile. After executing all
parameters of this profile are up-to-date. This allows you
to change profiles.

UPDATE ALL PROFILES
This is a very powerful function to execute all profiles,
wich uses the exportfunction in outputformt.
Exportfunction will be explained later, but I will show
you the effect of "update all":
Imagine you have an adressdatabase, that includes names
of towns (i.e. London, New York, Tokio). Now you can
create for each town a query. Using exportoption means,
that the result of your query ist stored in an exportfile.
So you can create an exportfile named "ADRLON", "ADRNY"
and "ADRTOKIO". You can execute these profiles alone or:
Use "UPDATE ALL PROFILES". In this case all profiles
of this kind will be executed in one process. This is a very
easy way to split great databases in some smaller ones and
this automatically.
Note: The profiles of this kind must(!) be stored in the
directory: "M:\APP\DQE" of your PSION-machine!
But that is the only limitation.

PROFILINFORMATION
You will get a view of the profile-criteria by using this function.
In cases of textsearching-profiles you will be informed detailed.
If your profile includes a calcultion or others than you have
to type in a descritption by your own, that informs you about
action taken by execetuting this profile.
Profilinformation delivers in every case the data and time of
latest executing.
Alternatively you can also get a profilinformation directly from
PSION-system-screen, this means you can have a look to a
profile before executing it.
The procedure is like executing form systemscreen, but you have to
do something additional: Immediately (!) after choosing your profile
press " i " (like information). Afters this occurs the profilinformation
of your profile and it will not be executed. Note: Time to press "i"
is not more than max half of a second. Trie a bit and you will know
how to use this possibility.

SEARCHING TEXT
You can search with two searchwords combined by the logical operators
"AND", "OR", "NOT". If you use only one searchword set the second
to "*".
CAPITAL: Capital letters and small letters are observered or not.
OUTPUT: You can "DISPLAY" your searchresult on the screen or "Export" it
in an exportfile. Even you can only "COUNT" the matches your query delivers.
Label: You can choose only one filed to be searched. This speeds up
your search and gives your more specification to the query.
NOTE: The display of the results appear in an own databasefile. The behaviour
while opening depends on the behaviour of your source database. This means,
if your source database will be sorted after openning, the resultfile will be
sorted too. You have no possibility to change the behaviour of the resultfile
individually.

SEARCHING DATES AND NUMBERS
This functions works in the same way like searching for text. But the operators
are the mathematical operators. A special function is to search for ranges of
Values like: Numbers from 10000 TO 20000. Only in this special subfunction the
second Value is needed. You can also search for ranges of dates.
In the first line of the dialog you have to chose what kind of numbers you
want to search (Date or Number).
NOTE: The format of date (mm.tt.yy an other) stored in your databse has to
be same as defined in the PSION-System-declaration (Clock-Application)!
Otherwise you큞l get no match or wrong results.
Two digit entries (XX) for year will be read as 19XX.
Outputformat "FILTER" defines a filter (see explanation "textfilter"), that
can be used by "find text" function.

TEXTFILTER
Activating a textfilter means a kind of preselection of your database.
After a filter is created a following search uses only the datarecords,
that matches the query of the filter. By this way you are able to create
even more complex queries.
NOTE: To activate a filterfunction you have to set the Switch in the
bottomline of the dialogbox to "ON". To deactivate a filter you set it
off by choosing "OFF".
Established textfilter is only used by "Find text". Other functions
like "Statistic" will deactivate a filter.

SEARCH AND REPLACE
This functions allows you to change strings or part of strings in your database.
You can use wildcards (*,?) in the searchstring. By matching the searchstring
the searchstring an the result of replacing will be shown in a dialogbox.
You can choose wich action then should be done.
NOTE: You have to choose one dataselabel for this function, it is not possible
to search and replace all labels  in one process.
Attention: Searching and replacement changes the content of your database!
In spite of the result make a backup before using this function.
A active filter will be deactivated.


CALCULATION
DQEPLUS is able to do calculations with labels of your database.
See this example:

You have the labels "Price", "Quantity" and "Total".
Now you can write the following equation into the dialogbox:

"Total"
"="
"Price"*"Quantity"

In each record of your database will the label "Total" filled with the result of
the calculation "Price"*"Quantity".

Note that label must be quoted ("). Furthermore you can also use a
calculation without any filedname. Allowed are all functions available on
your psionmachine.
The calculationstring is limited to a lenght of 255 charakters, but this string
will during process changed into a real equation. In the example means this:
If the "price" is 1000 an "Quantity" is 10000 the real equation is:
1000*10000. This is important, because it may occur an error, if the real
equation is longer than 255 charakters. 
Note: If an error occurs during calculation the targetfieldlabel will be filled with
"Error". The reason for an error may be a wrong calculationstring (i.e Division
by zero) or the specified field is not found. The second error will show "Error"
in every record of your database.

STATISTICS
This functions calculates SUM, MINIMUM; MAXIMUM, MEAN, STDDEV
for one label of your database. The resultbox shows you also the value for
missing fields. Missing fields are empty or filled with text.
Active filter will be deactivated.

FREQUENCIES
The function calculates the frequencies of all field-contents in one label of your
database. The output is shown as an PSION-word-file.
See this example:
You have an adressdatabase with contains townnames, like Liverpool, New York
and Tokio. These names are stored in the field "TOWN".
The function counts the frequency of "Liverpool", "New York" ...
As result you큞l get the absolute frequency (n), absolutly percantage and relative
percantage in relation to all records.
The relative percantage is calculated by using only valid values (non emtpty fields).
This function is very suitable for evaluation of small opinion polls.
Another possible use is to find double datarecords.
Limitations: Outputfile is limited to a size of 40 kilobyte, like any other PSION-Word-
file.
Note: In some cases there will be an message appear (You are not able to change this
file ...) before resultfile opens. Ignore this, there is no danger for your data.
Active filter will be deactivated.



STATUSINFORMATION
Here you큞l get information about used databse and other criterias.

PREFERENCIES
You can set the usual path for your databases (usaually M:\dat) and the path
for your exportfiles (if exporting results).
You can set the comma digits for functions thats calculates something.
Reg.nr.: If you decide to use the program after you had tested it, you should
register (See end of this readme). You큞l get a registernumber. Type it and the
program will work without any restriction.

COPY STRUCTURE
Copies the structure from the opened database to an new database, wich will
contain all labels, but is empty.

SAVE & EXIT
Saves your profile and close the program.

QUIT
Leaves the program without saving anything.

***********************************************************

GENERAL REMARKS UND LIMITATIONS

Memoryspace
The programs needs after installing minmal the same space as your source databases
takes. If you use the filterfunction space is increasing in relation to filtercriteria.
If memory runs out out during executing DQEPLUS you큞l get systemmemory-warning
and the program will immediately quit. Check out memory space before starting the
program.

Remarks on filterfunction
In general filter is activated by switing "ON" in textfilterdialog. If you use
an date- numberfilter, using exportformat in Date / Numbersearch outputformat
this filter needs no confirmation to be activated. In every case you switch off
any filter by setting the switch in TEXTFILTER-DIALGOG to "OFF", even date-
and numberfilter!
If you use date- numberfilter and then create a textfilter, you큞l see an advice,
that there is already a date- or numberfilter in action. By overwriting this information
you erase the older filter with the newer one.

SPECIAL PROFILES
Two profiles have a special behaviour. This are the "only-filter-profiles".
It means that these profiles contents only filtercriterias, no other searchinformation
or anyelse action. Usually an executing profile shows you the resultfile. Closing the
resultfile takes you back to PSION-systemscreen. "Only filter-profiles" stay in the
program and do not return to systemscreen after they have been executed.
This gives you a platform to create new profiles, wich have their base on an preselected
database (preselected by filtercriteria).
How to create a "only filter profile". At first do like creating any other profile. But here is
the important point. After the appearence of the programsurface you have to set your
filtercriterias, and only this. Then save your profile. Do not add a serch or other action after
the creation of the filter, otherwise you큞l get a combinded profile, wich contens filter- and
searchaction. Note that a cobinded filter is a possible profile, too.


FREQUENCIES
This function establish the resultfile "M:\opd\stat.wrd". This file is not deleted by
 leaving the program after using frequencies-function in a session. It will be deleted when
you use the program next  time.

REMOVE DQEPLUS from your PSION
If you want to remove the program, do this:
Press PSION-E on the systemscreen.
Go to directory M:\APP\DQEP. Now you can delete
all files. Then erase directory M:\APP\DQEP.
Erase M:\OPD\DQEPLUS.cfg. If visible "M:\opd\stat.wrd"
erase it too (only when used frequencies-function in last
session).


Known problems:
In very rare cases may occur an error if you set the
resultfile (Dqqeres.dbf) to the LIST-VIEW (using
PSION diamond-key. List-view will appear but
closing the file will schow an error.
This behaviour was only seen by databases, that
were not established by Psion-database-application,
but imported by using PSINWin X.X.
In any case you have the possibility to avoid this
error: If you use list-view in resultfile switch to
another viewmode and then close the resultfile.


Registration
THIS PROGRAM IS SHAREWARE.
This allows you to proof DQEPLUS, before you decide to register.
The author expects that you register after a time of cca. 14 days using DQEPLUS.
Taking registriation allows yout to use one copy of DQEPLUS.
Any manipluation, decompilation and trading of DQUEPLUS by user
is prohibited.
The program remains property of the author, you큞l get a liscense to use DQEPLUS
by registration.
DQEPLUS is sincerely proofed, but the author cannot be made liable for damage of any
kind, caused by using this Software.
Note: Unregisered version of DQUEPLUS allows only storing of one profile, named "DQEPLUS",
even exportfunction is not available.
Information about registering on http://www.snafu.de/~w.janssen

Copyright by Wolfgang Janssen, Berlin (Germany), May 1999

















