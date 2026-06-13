                                README.TXT
                                   for
                                 Mathweb
                               Version 1.3                            


DEFINITION OF SHAREWARE

Shareware distribution gives users a chance to try software before
buying it. If you try a Shareware program and continue using it, you
are required to register it.

Copyright laws apply to both Shareware and retail software, and the
copyright holder retains all rights, with a few specific exceptions
as stated below. Shareware authors are accomplished programmers, just
like retail authors, and the programs are of comparable quality. (In
both cases, there are good programs and bad ones!) The main difference
is in the method of distribution. The author specifically grants the right
to copy and distribute the software, either to all and sundry or to a
specific group. For example, some authors require written permission
before a commercial disk vendor may copy their Shareware.

Shareware is a distribution method, not a type of software. You should
find software that suits your needs and pocketbook, whether it's retail
or Shareware. The Shareware system makes fitting your needs easier,
because you can try before you buy. And because the overhead is lower,
prices are lower also. Shareware has the ultimate money-back guarantee --
if you don't use the product, you don't pay for it.

WHAT IS MATHWEB?

Mathweb is a mathematical assistant, programming language, and
analysis tool. It computes using webs of arithmetic or string
expressions, builtin and user defined functions, and scripts of a
general purpose programming language.  For registered users, a
builtin x-y plotting feature can be activated, which automatically
generates plots of chosen pairs of variables during computation.

Mathweb's simple, cellular organization also provides the ability
to edit, store, search, and report on the contents of hypertext
webs of text, as well as webs of functions and arithmetic
expressions. These features help the user in the analysis of
complex issues, relationships and subjects.  

WHAT CAN MATHWEB DO?

The shareware distribution of Mathweb comes with the following
example applications (or webs):

    Payment -- Compute the monthly payment necessary to pay off
         a loan at a specified interest rate.

    Cave -- A hypertext adventure.

    Logistic -- A simple web illustrating the onset of chaos.

    Pert -- Find the critical path and estimated time for a
         construction project.

    Quotes -- A compendium of quotes by famous and not so famous
         people.

    RKutta -- Compute the numerical solution to the differential
         equation for simple harmonic motion.

    Piggy -- Computes your net worth from a count of the coins
         in your piggy bank.

    Roots -- Compute the roots of a polynomial using Newton's
         method and the method of bisection.

    Solve -- Inverts a matrix and solves simultaneous linear
             equations using the builtin function which supports
             Gauss-Jordan elimination.

    Trapzd -- Compute the integral, to a desired degree of accuracy,
         of an arbitrary function.

    Ca -- Illustrates the development of limit cycles in cellular 
         automata.

    Stats -- Computes descriptive statistics of a collection of
         data points, and plots an estimate of the distribution
         function of the population from which the samples were
         drawn.  Generates sequences of random numbers from
         normal, uniform, or exponential distributions.

    Filter -- An example which illustrates the scripting language
         by demonstrating digital IIR and FIR filters 

    Hebb -- Illustrates Hebbian learning using a delta rule.


These examples only scratch the surface of the possible uses
of Mathweb.  It's uses are limited only by available processor power,
memory, and the imagination of the user.  From time to time,
additional examples are published on the web page
http://www.earthnet.net/~eieio/.

HOW DOES MATHWEB WORK?
Each cell in a Mathweb web contains a name, a text field, a value field,
and a formula/script field. References to other cells, which appear in
any field of a cell, may be used as hot-links to those cells.  Cells may
refer to themselves, making possible cycles of hypertext reference, and
recursive script execution.

Output of the program includes results of script execution and reports
of cell contents, and may be directed to the LCD display, the text field
of a cell, or to a file in memory, which may be uploaded to a desktop
computer for further analysis or printing.  This version of Mathweb,
version 1.2, runs only a Psion Series 3a.

UNREGISTERED VERSION HANDICAP -- The unregistered version of
Mathweb v1.3 is handicapped.  It can save files only under the names
of the included example files, or the names 'new', 'myweb1',
'myweb2', and 'myweb3'.  The registered version of the program
has the normal Psion provisions for file naming and saving.

INSTALLING MATHWEB

1.  Before installing Mathweb on your Psion 3a, create a \web\
directory. This is where the program will keep its data files, unless
you direct it to store them elsewhere.

2.  If you have no \app\ directory on your Psion, create one
to hold the Mathweb executable.

3.  Use PKUNZIP or equivalent to decompress the MathWb.zip file into
Mathweb.app, this Readme.txt file, and a collection of sample webs,
labeled *.web.

4.  Use MCLINK, WINLink3, PsiWin or some other PSION remote file transfer
program to copy Mathweb.app to the \app\ directory on your chosen solid
state disk drive.

5.  Then transfer the sample webs to the \web\ directory that
you created in step 1.

6.  Choose Install from the Apps menu of the system screen, and
select Mathweb for installation.

7.  If all goes well, the Mathweb icon will appear on the desktop,
along with sample webs which you copied to the \web\ directory.

8.  Try the simple tutorial in the next section of this Readme file.

FIRST TUTORIAL

If Mathweb was sucessfully installed, there will be several sample webs
to choose from under the Mathweb icon.

====Piggy

Select the web called "Piggy" and press return.  Your Psion should
display a window divided into two panes by a horizontal line.

The top pane, the narrow one, contains the words "PIGGY--home".  This is
the name pane, a read-only pane, whose leftmost word is the name of the
web, and whose rightmost word is the the cellname (home).

The larger pane is the text pane.  It contains an edit box in which
text may be entered into the text field of the currently displayed
cell. 

Every Mathweb web contains a "home" cell, which is diplayed on entry to
the web; and which cannot be deleted. The text area of the home cell is
frequently used to describe the purpose of the web, and to present a
simple map of its contents.  This information is supplied for the Piggy
web, as you can verify by using the arrow keys to scroll through the text
field of the cell.

====|====Browsing the Web
Press the menu button and you will see in
the menu bar an entry called 'Browse'.  Select that entry and the
four choices 'Prev', 'Next', 'Overview', and 'Trail' are displayed
with their accelerators PSION+, PSION-, PSION/, and PSION*.  Use the
+ and - accelerators to browse the Piggy web, returning to the point
from which you began.

You will notice that the cells in the web appear in alphabetic order
when browsed, and that browsing forward from the last cell in sequence
returns you to the first cell in sequence.

Choose the / accelerator, and a window will open, which may be
scrolled to show the names of all of the cells in the web in
alphabetical order.  If you want to go to a particular cell shown in
the browser, position the highlight to that cell's name and press
ENTER.  To exit the browser without changing cells, press ESC.

If the name of the cell you want is not in the window, you may scroll
to it with the up and down arrow keys, or type the first letter of the
cell name, which will cause the browser to go to the portion of the
overview which contains cells beginning with the letter typed.

Choosing the * accelerator activates the trail browser which is 
discussed in the section on hypertext links.

====|====Changing the Current Cell
Aside from browsing, there are three other methods of moving from
one cell to another.  These methods are: 'Linking' to a cell, 'Going'
to a cell, and following a hypertext link.  The next three short
paragraphs describe how to perform each operation.

====|====|====Linking to a Cell
When you 'link' to a cell, you transfer to that cell with the
expectation of returning to the current cell.  To try this, choose
'Linkto' from the Cell menu, and enter the name of the cell
'pennies' into the dialog box that appears.  When you press enter,
the screen should show the 'pennies' cell.  Return to the cell you
left by pressing ESC, or choosing 'Unlink' from the Cell menu.

Mathweb keeps a trail of cells to which return is expected, so that
you can return from many levels of linking by repeatedly pressing ESC.
You may view the return trail by activating the trail browser in
the browse menu.  From that browser, you may link to a cell on the
trail by positioning the underline cursor to the cell, and pressing
return.

====|====|====Going to a Cell
When you 'go' to a cell, you transfer to that cell with no
expectation of returning to the current cell.  To try this, choose
'Goto' from the Cell menu, and enter the cell name 'dimes' into
the dialog box that appears.  When you press enter, the screen
should show the 'dimes' cell.  The cell that you left will not
be placed on the trail, so you will have to 'browse' your
way back.

====|====|====Hypertext Links
Wherever the name of another cell appears in the text pane
of the current cell, you can use that name as a hypertext link
to the other cell.  In Piggy, hypertext links to other cells
are marked by preceding them with ampersands (&).  This convention
is unnecessary for the functioning of the links, but it makes
it easier to navigate the web.

To follow a hypertext link, position the cursor in the text stream
a bit before the link, and select 'Hunt' from the Cell menu.
Mathweb will scan forward within the current cell until it finds
a valid link, and then transfer to the cell named in that link.
The destination cell will replace the one currently shown.  To
return to the previous cell, press the Esc key, or select 'Unlink'
from the Cell menu.

NOTE:  Whenever the name of a cell is typed into an edit pane,
it automatically becomes a hypertext link to that cell.

NOTE:  When you select a cell name in the overview browser or
the trail browser, and press Enter, the named cell is 'linked' to.
You may return to the cell from which you left by pressing ESC.

====|====The Math Window
In addition to the text window, each cell also has a math window,
which you may see by using the diamond key.

The math window is divided into three panes.  The top one, as
before, is the name pane, a read-only pane which contains the
name of the web followed by the name of the currently displayed
cell.

The middle pane is the value pane.  Into this pane are entered
values which one wishes to associate with the cell; and from this
pane may be read the results of calculation, the latest value of
the cell.

The bottom pane is the script (or formula) pane.  It contains an edit
box, into which an expression may be entered for calculating
the value of this cell.  The simplest kind of script is an arithmetic
formula which may employ cell names and built-in functions to compute
a new value for the cell in which it appears.

NOTE:  You can transfer between panes with the TAB key.

NOTE:  Cell names in formulas may be used as hypertext links to other
cells in the say way as cell names in text fields.

NOTE:   Actually, the script field may contain a sequence of
statements from the script language which are executed in order.
An expression can be considered a statement from the script language.
For each expression evaluated, the value of the cell containing
the script is set to the value of the expression. (See grammar below).

====|====Calculating a Result
Next we will see how a web can be used to calculate values of interest.
In this case, our net worth, or the sum of the coins in our piggy bank
is the value of interest.

Select the Math window, and use the browse accelerators PSION+ or
PSION- to traverse the cells of the PIGGY web.  Notice that most of
the cells have names corresponding to the names of denominations
of US coinage.

Enter into the value field of each cell whose name is a denomination
of coinage, the total number of coins of that denomination which you
have on hand. (Use TAB to position the cursor to the value field).

Position to the cell called 'Piggy' and choose 'Execute' from the
Special Menu.  Unless the Execution preferences have been changed
since you installed PIGGY, you should see a line of text such as the
one below:

      Piggy contains $74.13

The format of the output is controlled by the Execution Preferences
which may be reached from the 'Preferences' selection in the Special
menu.  Notice also that the value in the cell 'piggy' has changed to
the same value as the one listed.

====|====Reporting on Cell Contents
To see an alphabetic list of all the cells in the PIGGY web, together
with the value of each cell, you may select 'Report' from the 
Special menu.  The following list should appear (your values may
be different):

    dimes 231
    half_dollars 43
    home
    nickels 242
    pennies 1343
    piggy 74.13
    quarters 0
    silver_dollars 4

If you did not get such a list, it may be because you changed the
Report preferences accessible from the 'Preferences' entry of
the Special menu.

SECOND TUTORIAL

Our goal for this tutorial is to create a new, empty web, to add
cells to it, and to create links between the cells.  To this end we
will make a web which lists the names and addresses of several people
we know, and also add a bit of useful information not usually found
in such a list.

====Making an Empty Web
Position to the Mathweb icon in the system menu.  Hold down the
PSION key, and press the 'N' key.  A dialog box will appear,
asking you for the name of a new Mathweb file.  Type in the
name 'New' and press Enter.  Mathweb will respond by showing 
you the home cell of a web named 'New'.  Browsing forward
or backward, you will see that home is the only cell in the web.

====Adding Cells
A new cell may be inserted into our web by using the 'Add', 'Linkto',
or 'Goto' commands in the Cell menu.  For now choose 'Add', and answer
the dialog with the name of someone with whom you are friendly or
acquainted.

    If the "Bad Cell Name" dialog appears, it is probably because you
    left a space between the person's first and last name.  Cell names
    may not have spaces in them; but it is OK to use the underscore
    character in place of the space you would normally use.
    (Some software types like to jam words up together and indicate
    separate words by the use of capitalization, as in WhatsInAName).

    If you repair this defect, and still get the "Bad Cell Name" dialog,
    it may be because your friend's name begins with a number, such as
    '3Cpo', or a special character, as in '#Sterling'.  All cell names
    must begin with an alphabetic character.

If there is no cell which has the name you entered, one is created.
You may type in whatever information you wish about this person,
including, for example: address, phone number, height, weight, and
number of outstanding parking tickets.  Keep in mind that you will be
able to use the 'Find' command in the Edit menu to search your web on
the information which you have entered.

Repeat the process of adding cells with names of associates until
you have entered several persons into the little database.  Next we
will enter connections between the people in the web.

====Connecting the Cells
Goto, Linkto, or Browse to the cell for the first person you entered,
and enter the text field for that person.  Scroll down to the bottom
of the text field, start a new line, and enter the words:

      TALKS TO:

Then enter the names of persons with whom you know that this person
communicates on a daily basis.  If the person involved is already
in your database, make sure you spell the name the same way as it
appears in the cell name.  If the person involved is not already
in your database, you may want to considering entering the person.

Go through the cells of the database, adding the links wherever you
know them to exist, and adding persons where it seems appropriate.
When you finish, you will not only have a list of acquaintances and
whatever information you wish to keep about them; you will also have
a map of information flow among your associates.

By setting the report preferences to InPointers or OutPointers and
choosing Report from the Special menu, you can see a report on this
information flow.

====Searching the Web
In addition to following chains of links which you type into the text
field of the web, you can use the 'Find' function in the Edit menu to
go to the next cell having a specified text string, or to the closest
previous cell having a specified text string.  Your search may be case
sensitive or not.  You may set default search preferences by obtaining
the Preferences dialog (choose Preferences from the Special menu), and
then choosing 'Searching' from that dialog.

====Saving the Web
To save the database you have made, choose 'Save As' from the File menu,
and pick one of the names 'myweb1','myweb2', or 'myweb3'.  You
may include, or not include the suffix 'web', as you wish.  If you use
another suffix, the web will be saved with that suffix.

SPECIFYING MATHWEB OUTPUT
====Purpose of Mathweb's Output
Mathweb is a personal, portable scratchpad for thinking about complex
phenomenon, systems, and relationships.  It is not intended to communicate
the results of that thought to others.  Instead it provides flexible,
primitive tools to format output and results in a way that can be
understood by the user, and transferred to other programs or computers
for further analysis.

====Kinds of Output
The next three subsections describe the kinds of output generated by the
Mathweb program.  Subsequent sections will introduce further information
on the mechanics of controlling that output.

====Report Output
The Mathweb 'Report' entry, in the Special menu, permits the user to
select some or all of the name, text, script, or value information
in some or all of the cells, and format it into simple ASCII text streams
which are conveyed to the current output destination of the program.

This feature permits the user to create indexes of webs, list for analysis
the formulas involved in a web, perform complex recombinations of cell
contents, dump web contents to files for upload to a host computer,
and to generate so-called "comma-separated text" files for exporting
Mathweb data to databases and other programs.

Also available are cross-reference reports called "InPointers" and 
"OutPointers", which assist in understanding the structure of a web;
and the report called "Trail", which may be used to keep track of
your current hierarchic position resulting from a sequence of
"linkto" or "hunt" requests.

====Execution Output
For script execution, there are two kinds of output.  The first is
automatic listing, which may be generated without any specific request
from the user; and the second is script output, which is generated
by "out" and "report" statements within the script fields of cells.

====|====Automatic Listing
This kind of output is selected in the Execution Preferences dialog
box.  Its purpose is to provide output from script executions which
are composed purely of formula evaluation (without "out" or
"report" statements).  In the Execution Preferences dialog box, the
user may choose between DontList, GenericList, and FormattedList.

====|====|====NoList
If the user chooses NoList, no automatic results will be directed
to the current output destination, although the value fields of affected
cells will be updated.

====|====|====GenericList
Generic output consists of a title line containing a list of all the
cells computed in a calculation; followed by one line per iteration
containing the values of each of those cells at the end of the iteration.
See the section entitled "Specifying Execution Output" for more
information.

====|====|====FormattedList
Formatted output consists of an optional title line derived from a
format string, followed by one line per iteration of the values of cells
specified in the format string; separated by the other characters
of the format string. See the section entitled "Specifying
Execution Output" for more information.

====|====Script Output
See the section on the script language for information on how to
create script output.

====Plotting Output
When enabled, plotting output goes to the plotting screen which
appears for that purpose on the left side of the Mathweb screen.
The Text, Value, and Script panes shrink to make room for the
plotting window.  Plotting happens automatically, so long as it
is turned on.  The cells plotted, and the limits of the screen
are controlled by setting them in the plotting dialog box which
can be reached by choosing Plotting from the Preferences menu
reached from the Special menu.

NOTE:  Plotting is available only to registered users. 

====Choosing Output Destinations
Output of reports or calculations may be directed to the screen, to a
cell within the web, or to a file in the Psion's memory.  To set the
output destination, select the 'Preferences' entry in the Special menu.
A dialog menu with the following contents will appear:

    Preferences for:
    * Input
    * Output
    * Numbers
    * Execution
    * Searching
    * Reporting
    * Plotting

Choose the 'Output' choice to reach the Output Preferences dialog,
which contains the following contents:

    Output Preferences:
    * Destination   <- File ->
    * Dest Name       mugga
    * Newline Type  <- CRLF ->

When output is directed to the screen, it is displayed one screen at a
time, with a prompt before the next screen is displayed.  

When output is directed to a file or to a cell, the name of the file or
cell is entered by typing it into the Dest Name field of the dialog. 

When output is directed to a file, the type of newline generated can be
controlled by the third entry in the Output Preferences dialog. If the
output will be exported to an IBM compatible computer, it is a good idea
to use the 'CRLF' option for newline.  If the output will remain on the
Psion, or will be exported to a Macintosh or Unix computer, the LF option
works better.

NOTE: Although the web itself may be saved, File output from the web
is not permitted in the unregistered version of Mathweb.


====Specifying Report Output
To specify what information will appear in Report output; select the
Special menu, and choose the 'Preferences' entry. The Preferences
dialog (shown above) will appear.  Select 'Report' from that dialog,
and the Report Preferences dialog will appear:

    Report Preferences
    * Report on All Cells?  <-Yes->
    * Report Type          <- Formatted ->
    * Report Format       \n\N\n\V\n\F\n\T

The first question in the dialog tells Mathweb whether or not to report
on all cells.  The second tells it what kind of report to generate.  The
third tells it which information to include for each cell in a formatted
report, if that is the type of report that is selected.

====|====Report Cell Selection
Answering Yes to the  "Report on All Cells?" question will assure that
when the user elects to generate a report, all of the cells in the web
will be reported, according to the report type and format specified.
Answering No will cause the report function to select a subset of the
cells in the web.

The subset of cells selected for reporting depends upon an algorithm
executed by the report function.  If there is a highlighted text selection
in the active edit box at the time a report is requested, Mathweb will
scan that selection for valid cell names, and generate a report on all
the cells named in that selection, in the order in which they appear.
If there is no highlighted selection in the active edit box, Mathweb
will generate a report on the current cell only.

====|====Report Type
There are four types of reports.  They are the Formatted report,
the InPointers report, the OutPointers report, and the Trail
report.

====|====|====The Formatted Report
The appearance of a formatted report depends upon the content of the
Report Format string in the Report Preferences dialog.  If nothing is
entered in the report format string, no output will be generated when
Report is chosen from the Special menu.

If the report format string is not empty, then choosing Report from
the Special menu will cause that string to be scanned and interpreted
for each cell in the selected cell list.

When the report format string is interpreted for a cell, most
characters in the string are copied directly to the output destination.
Substitutions are made; however, for character pairs which begin with
the backslash (\).  The substituted text is copied to the output in the
place of those character pairs.

Character pairs for which substitutions are made during interpretation
of the report format string are:

'\n'    A newline sequence is copied to the output in the place of
        a backslash followed by a lower case n.

'\N'    When upper case N appears in the report format after the
        backslash character, the report generator substitutes for the
        pair of characters the name of the cell for which the format
        string is being interpreted.

'\V'    Backslash-V causes the value of the cell to be substituted for
        the two characters.

'\F'    Backslash-F is replaced in the report by the formula field of
        the cell being interpreted by the report formatter.

'\T'    Backslash-T results in the outputting of the text field of the
        cell being reported.

Any other two character sequence beginning with a backslash will cause
the second character only to be output to the report.  This includes
the string '\\' which is replaced in the output by a single backslash.

If the report format string is not empty, but neither does it contain
any of the character sequences above, it (the report format string)
will be copied verbatim to the output once for each cell which has
been selected.

====|====|====InPointers Report
For each cell in the report cell selection, this report outputs a
list of all the other cells in the web which contain references
in the selected field (text or formula) to the cell.

====|====|====OutPointers Report
For each cell in the report cell selection, this report outputs
a list of all the other cells to which the selected field (text 
or formula) of this cell refers.

====|====|====Trail Report
The Trail Report merely lists from most recent to most ancient,
all of the cells placed on the Trail by Linkto requests, that have
not yet been popped off by Unlink requests.

====Sample Reports
Below you will find described the way in which reports are formatted
for some of the sample webs included with the Mathweb distribution.

====|====Cave
This web was made to be explored as an amusement.  Its report
is set to an OutPointers report.  When Report is chosen from
the Special menu, the following helpful text is generated:

    AttackTheGnome
    ClimbTheRockChimney
    DiveIntoTheTorrent
         >home
    EnterTrapDoor
         >TakeTorchIntoPassageway
         >GoBackToShack
         >Fossil
    GoBackToShack
         >LayDownAndDie
         >TakeTorchIntoPassageway
    GoHomeAndWatchTV
         >home
         >home
    home
         >EnterTrapDoor
         >GoHomeAndWatchTV
    LayDownAndDie
    Noway
    SmileAndBrushPastHim
         >ClimbTheRockChimney
         >DiveIntoTheTorrent
    TakeTorchIntoPassageway
         >AttackTheGnome
         >SmileAndBrushPastHim

This report presents us with a synopsis of the choices available
at each cell of the web called 'Cave'.  One would be wise to print
the report BEFORE entering the cave.

====|====Payment
The web called "Payment" allows you to compute the monthly payment
necessary to pay off a loan of a given amount, at a prescribed
interest rate, in a specified number of monthly payments.

Sometimes, when you are working on a calculation, it helps to see all
of the formulas on the same screen or page.  The report format string
of the Payment web is:

	\n\N:=\F

When a report of all cells is generated from the Finance web using this
string, the result is:

    apr:=
    home:=pymt!
    i:=apr/12
    n:=
    power:=pow(1+i,n)
    prin:=
    pymt:=prin*i!*power!/(1-power)

which is a list of the formulas used in computing the payment
due on a note with a specified principal, apr, and number of periods.

====|====Quotes
The web called Quotes has a report fomat string that can be used to
copy the contents of the web, except for the value field, into a single
stream, which may be saved into a file for upload, or scanned as it
goes by on the LCD screen.  The format string reads:

	"\n\T\n          --\N\n"

Here is a selection of the output:

     My work is being destroyed almost as soon as it is printed.
     One day it is being read; the next day someone's wrapping fish in it.
                           --Al_Capp

     If we had no faults, we should not take so much pleasure in
     noticing them in others.
                           --Duc_De_La_Rochefoucauld

     He who knows does not speak.
     He who speaks does not know.
                           --Lao_Tzu

====|====Pert
This web computes the critical path time to completion of a
construction project.  The report format string has been arranged to
print a comma-separated text file of the task name, and completion time
for every cell in the web.  The report format string is:

	\n"\N","\V"

The resulting report is:
    "BuildFoundation","3.3"
    "BuildUpperStructure","17.5"
    "ClearLand","1.5"
    "CompleteExterior","12"
     .........

To change the order in which the cells come out, answer No to the
question in the report preferences dialog about whether you want
to report on all cells.  Then create a list of the cell names in
the order in which you want them to appear, select the list and
choose 'Report' from the Special menu.

The Pert web is also a good example of the utility of the InPointers
report.  If we request an InPointers report for the text fields of all
the cells in Pert, we obtain:
    BuildFoundation
        <ClearLand
        <path1
        <path2
        <path3
    BuildUpperStructure
        <BuildFoundation
        <path1
        <path2
        <path3
    ClearLand
        <home
        <path1
        <path2
        <path3
    CompleteExterior
        <BuildUpperStructure
        <path3
    CompleteInterior
        <ElectricalWork
        <path1
        <path2
        <PlumbingWork
    ....
    ....

Not only does this report show us the prerequisites for beginning each
task in the construction project, it shows to which path or paths each
task in the project belongs.
    

COMPUTING WITH MATHWEB

In this section is described the mechanism by which Mathweb script
execution is invoked, the method of specifying computation output,
the rules of evaluation used by the Mathweb script interpreter, and
examples which illustrate the use of those rules.

====Executing a Script
If necessary, set execution preferences by accessing the Execution
Preferences dialog accessible from the Preferences choice of the
Special menu.
Position to a cell which contains a script which you
wish to execute.  Choose 'Execute' from the Special menu.  The script
of the cell will be executed, and if there is a result returned, it
will be placed into the value field of the cell.  If the script or
formula contains references to other cells, followed by exclamation
points or argument lists, the formulas of those cells too, will be
executed.  If Execution preferences call for Results to be output,
the results of the calculation will be directed to the current output
stream.

====Contents of Script Fields
Script fields may contain sequences of script language statements,
which are evaluated in order.  The arithmetic expression is a special
kind of script language statement, often called a formula, whose
execution causes the value of the cell in which it is executed to be
set to the value of the expression.  Examples of valid contents of
script fields are shown below:

    a14+25*x             # arithmetic expression

    2                    # arithmetic expression

    2 xx! (y-14)         # sequence of arithmetic expressions

    if(y<1) {            # if statement
       14
    } else {
       21
    }

    subprogram1!         # sequence of script language statements
    subprogram2(14.3)
    while(x>4) {
        subprogram3(x)
        set x to x+1
    }
    x-4

See the section entitled 'Script Language Grammar' for details
on the permissable contents of script fields.

====Expressions and Formulas
An expression is a prescription for arriving at a new value
from a combination of numeric and string values and the
results of built-in and user defined function calls.
Examples of expressions include:
       1
      1+3
     1+sin(14*3)
    mugga*pugnacious/4
    sign(my cat relations+3)
    'eat' cat 'pie'

A formula is a script language statement which consists of a
single expression.  When a script containing a formula is
executed, the contents of its value field is replaced by the
value computed for the formula.

====Rules for Refering to Other Cells in Scripts
Mathweb allows the script of one cell to contain references to other
cells.  There are two kinds of references: execution (cellname followed
by exclamation point or argument list), and value (cell name
not followed by exclamation point or argument list) references.
     
====|====Execution References
An execution reference to a cell causes that cell's script to be
executed and the subsequent value of the cell to be substituted for
the reference.  The execution reference works like a function call 
in a programming language, with the name of the referenced cell
analogous to the function name.  Examples of execution references:

       cellname!   or    cellname()         (these two equivalent)
       cellname(expr,1+3/other_cell_name)   (arguments specified )

====|====Value References
A value reference to a cell causes that cell's value to be substituted
for the reference.  The script of the cell is not re-executed.  The
value reference works just lilke a variable reference in most
programming languages.  The cellname is analogous to the variable
name.

====|====Using Execution() References
The example below illustrates a simple application of the execution cell
reference!

The web Example1 has two cells, a and b.  Their values and formulas
are shown below:

    Web Example 1
    Cell a value:3
    Cell a formula:b!-4

    Cell b value:blank
    Cell b formula:13+4

When cell a is executed, the b! in its formula is resolved by
computing a value for b, and substituting it into the formula for a.
When the computation is done, the value in b is 17, and the value in
a is 13. The new values of a and b are independent of their previous
values.

In the web Example2, the formula in cell a contains an execution
reference to the formula in cell b, and the formula in cell b contains
an execution reference to the formula in cell a.

    Web Example 2
    Cell a value:4
    Cell a formula:b!-1

    Cell b value:14
    Cell b formula:a!+2

When the formula for cell a is interpreted, the execution reference to b
causes the program to first attempt to evaluate the formula in cell b.
The formula in cell b, in turn, requires an interpretation of the formula
in cell a.  This computation cannot be carried out, as it will repeatedly
start new calculations of each cell until it runs out of memory, and
a STACK OVERFLOW ERROR is generated.

====|====Using Value References
The example below illustrates how script interpretation proceeds when
cell references are not followed by exclamation points or 
argument lists:

The web Example3 also has two cells, a and b, whose values and
formulas are shown below:

    Web Example 3
    Cell a value:14
    Cell a formula:b-7

    Cell b value:21
    Cell b formula:a+4

When these cells are interpreted, a first, then b; the execution
proceeds as follows:

Cell a is set to the value of cell b, 21, minus 7, or 14.
Cell b is set to the new value of cell a, 14, plus 4, or 18.

When the two cells are interpreted, b first, then a, the execution
takes a different path:

Cell b is set to the value of cell a, 14, plus 4 or 18.
Cell a is set to the new value of cell b, 18, minus 7, or 11.

Since the cell references in each cell a and b were to the current
value of the cell, and didn't request recalculation (like a reference
followed by an exclamation point or argument list), there is no
problem with stack overflow.

It is important when computing the values of cells which refer to
each other to consider the order in which interpretation occurs.  This
is necessary all the time in programming languages, but is less
commonly necessary in cellular calculators.

====Number or String?
During evaluation of expressions, a cell value may be automatically
converted from a number to a string or vice versa, depending
upon what operation is applied to it.  In a numeric operation,
such as:

    maize + beans

the operands maize and beans must be numeric, so their values
will be forced to numbers if they are not numbers already.
In a string operation, such as:

    maize cat beans

the operands maize and beans must be strings, so their values
are force to strings if they are not strings already.

In a context where the same operator applies to both numbers
and strings, there are special rules to decide which to use.
When assigning a value with the set statement:

    set maize to <expression>

maize takes the type (string or number) of the expression.

In a comparison:

     if(expr1=expr2)

if both expressions are numeric, a numeric comparison is made;
otherwise any numeric operand is converted to a string, and a
string comparison is made.

There are two convenient methods for converting an expression
from one type to another:

    number cat ""       concatenate a null string to a number
                        to obtain a string

    string+0            add zero to a string to obtain
                        a number

To force a string comparison between two expressions, coerce
one to a string:

    expr1 cat "" = expr2

To force a numeric comparison between two expressions, coerce
both to numbers:

    expr1+0 = expr2+0

The numeric value of a string is the value of the longest
prefix of the string that looks numeric.  Thus when the
following strings are converted to numbers:

  "1E5"+0  "All my relations"+0  "1X15.21"+0

The numeric results are:

   100000       0         1

The string value of a number is computed by formatting the
number in accordance with the numeric tyhpe and number of
digits in force at the time of the conversion.  For more
information see the section on "Numbers Preferences".

====Specifying Execution Output
To specify what information will appear in Execution output;
select the Special menu, and choose the 'Preferences' entry. The
Preferences dialog (shown previously) will appear.  Select 'Execution'
from that dialog, and the 'Execution Preferences' dialog will appear:

    Execution Preferences
    * Executuion Results   <-FormattedList->
    * List Format             %month,%prin
    * Number of Iterations         10 

The first item in the dialog lets the user select whether results
will be automatically output (without the use of 'out' or 'report'
statements in script fields), and if so whether the format will be
generic, or specified by the string which is entered in the second 
dialog item. The third item tells Mathweb how many times to execute
the script.

====|====Execution Results
One of three possible choices may be selected in this dialog item:

====|====|====Don't List
No automatic output is listed as a result of script execution.
This choice provides the shortest time of execution.  Even though
no automatic output is generated, you may view the results (at the
end of the last iteration) by choosing to "Report" the values of
cells of interest, or by using the browser to visit each cell for
which a value is required.

====|====|====Generic List
Results are automatically presented for each cell executed. Typical
generic output looks like that shown below:

    apr,month,itm,prin,pmyt
    0.075,1,137.09,21593.41,481
    0.075,2,134.96,21249.51,481
    0.075,3,132.81,20903.46,481
    0.075,4,130.65,20555.27,481
    0.075,5,128.47,20204.92,481
    0.075,6,126.28,19852.39,481
    0.075,7,124.08,19497.67,481
    ...........................
This kind of output is primarily useful for cellular calculation
output when it is desired to generate the result of a calculation
with a minimum of effort.

====|====|====Formatted List
For formatted listing, a header may or may not be presented, depending
upon the content of the List Format (line 2 of the dialog).

The content of the formatted result chunk generated for each iteration
depends upon rules described in 'Formatting Exectuion Results',
which is the next section.

====|====|====Formatting Execution Results
====|====|====|====Format of the Header Line
If the first character of the List Format is a tilde ('~'), no header
line is generated for the calculation.  If it is not a tilde, the
header line is generated in accordance with the following rule:

    A newline is output.  Then the contents of the List Format string
    are copied directly to the output, except that any percent signs
    in the format string are ignored.  Wherever the character pair
    '\n' appears in the List Format, it is replaced with a newline
    sequence.

====|====|====|====Format of Each Iteration Chunk
The output generated at each iteration of a calculation depends upon
the content of the List Format string.  The manner of this dependence
is described in the rule below:

    The contents of the List Format string are scanned,
    and copied to the output until a percent ('%') sign is
    encountered.  The next word after the percent sign is
    read to determine if it is a valid cell name.  If it is,
    the current value associated with that cell is substituted
    for the cell name in the output string.  Scanning and outputting
    of the List Format string is then resumed until the next percent
    sign or the end of the string is reached.  Wherever the character
    pair '\n' appears in the List Format, it is replaced with a
    newline sequence.
 
NOTE: The selection of cells for formatted output is controlled
entirely by the format string, and has nothing to do with the
selection of cells for execution.  This is necessary so that you can
view only the requested cells at each iteration, regardless of how
many cells are calculated.

====|====|====Execution Format Examples
====|====|====|====Example 1
The List Format string in the Execution Preferences shown above,

    "%month,%prin"

will cause the following type of output to be generated if both
cells are executed:

    month,prin
    1,159820.00
    2,159638.88
    3,159456.62
    4,159273.22
    5,159088.68
    6,159902.98
    ...........

If, on the other hand, the cell called 'month' was accidentally left
out of the calculation, but not the format, the output would look
more like:

    month,prin
    1,159820.00
    1,159638.88
    1,159456.62
    1,159273.22
    1,159088.68
    1,159902.98
    ...........

====|====|====|====Example 2
The web called 'Pert' provides an example of a List Format string used
for a script interpretation when there is only one iteration, and the
outcome is to be displayed for only one cell:

    "~The Critical Path takes %CriticalPath days."

In this case, the starting tilde eliminates the header chunk, and the
output looks like:

    The Critical Path takes 43.5 days.

Were the tilde removed from the front of the Format List, the output
would look like:

    The Critical Path takes CriticalPath days
    The Critical Path takes 43.5 days

====|====|====|====Example 3
Use of the '\n' character pair in the format string can cause multiple
lines to be output for each iteration.  Consider the format string:

   "Mtrx: %a1,%a2,%a3\n           %b1,%b2,%b3\n           %c1,%c2,%c3"

If we assume that all of the matrix elements are scheduled for
recalculation or included by reference, the output of two iterations
with this Format List might look like:

     Mtrx: a1,a2,a3
           b1,b2,b3
           c1,c2,c3

     Mtrx:32.0,31.5,14.1
          21.1,17.5,21.4
          3.5,1.2,2.3

     Mtrx:32.0,21.1,3.5
          31.5,17.5,1.2
          14.1,21.4,2.3


====SAMPLE CALCULATIONS

====|====Payment
The web called "Payment" allows you to compute the monthly payment
necessary to pay off a loan of a given amount, at a prescribed
interest rate, in a specified number of payments.

The Payment web uses cells with the following formulas to compute the
monthly payment:

    cellname       formula
     apr         (no formula... enter annual periodic interest rate)
     home        pymt()
     i           apr/12
     n           (no formula... enter number of monthly payments)
     power       pow(1+i!,n)
     prin        (no formula... enter the beginning balance)
     pymt        prin*i!*power!/(1-power)

The user may use the following procedure to obtain a result from the
Payment web:

    1) Put the annual periodic interest rate into the value field
       of the apr cell.  Make sure that you use the decimal fraction
       (such as 0.075 for 7.5%).

    2) Put the beginning balance into the value field of the prin cell.

    3) Set the value of n to the total number of monthly payments.  This
       is twelve times the number of years that the loan is to run.

    4) Position to the pymt cell and select Execute from the Special menu.

Since the 'home' cell has the formula: pymt(), you may also compute
a result by positioning to the home cell and selecting 'Execute' 
from the Special menu.

Notice in the cell 'pymt' that the first occurance of 'power', and the
reference to 'i', are execution references.  The '!' after the first
power reference assures that the formula for 'power' is executed
whenever the formula for 'pymt' is executed.  There is no '!' after
the second reference to 'power' because it is not needed, the value
having already been computed.  The '!' after 'i' ensures that the
periodic interest is computed from the annual periodic interest rate,
'apr', whenever pymt is computed.

====|====Pert
One of the sample webs provided on the Mathweb distribution disk is
called Pert.  The Pert web describes the tasks involved in building
a small house, and organizes those tasks according to which may be
carried out concurrently.  There are three different paths through
the web of tasks, from start to completion.  The three different
paths may be browsed in the web by following hypertext links.  They
are also described in three summary cells called path1, path2, and
path3.  Those cells contain formulas which compute the total time
required to traverse the associated paths.

    cellname       formula
    path1            ClearLand+BuildFoundation+BuildUpperStructure+
                          PlumbingWork+CompleteInterior
    path2            ClearLand+BuildFoundation+BuildUpperStructure+
                          ElectricalWork+CompleteInterior
    path3            ClearLand+BuildFoundation+BuildUpperStructure+
                          CompleteExterior+Landscaping

By entering the estimated time it takes to complete each of the tasks
into each task's cell, the times taken for each path through the process
may be computed from the formulas.

Pert has another cell and formula:

    CriticalPath     max(path1!,path2!,path3!)

When the web is positioned to the CriticalPath cell, and the 'Execute'
function is chosen from the Special menu, the time necessary to build
the house is placed into the value field of the cell CriticalPath.

The '!'s after the references to path1, path2, and path3
ensure that new values for these cells are computed every time
the cell CriticalPath is executed.  If the '!'s were not there,
it would be possible to change values upon which the path1, path2,
or path3 values depend, and to compute CriticalPath without the
new values being reflected in the values of path1, path2, or path3.
Therefore the value of CriticalPath would not be changed, even
though, for instance, the value for ClearLand did change.

The function of this web would not change if every use of the
exclamation point, !, were changed to the parenthesis pair ().

====|====Logistic
The emergence of chaotic dynamics from simpler behavior may be observed
in this example called the logistic map:

    cellname        formula
     r             (no formula...set this parameter to alter dynamics)
     x              r*x*(1-x)

To explore the behavior of this calculation, set the number of iterations
to 20 or so, set different values for r and x (make sure x is between
zero and one), go to cell x, and chose 'Execute' from the Special menu.

For a value of the parameter r of 0.4, and an initial x value of 0.7,
successive values of x approach zero and stay there.

For an r value of 2.4, and initial x value of 0.7, successive values
of x approach a constant 0.583

For r=3.0, and an initial x of 0.5, an extended damped oscillation
takes place between two values of x.

For r=3.5, initial x=0.7; an extended oscillation among four numbers
takes place.

For r=3.8, initial x=0.7; a chaotic sequence of numbers is the result.

====|====Amort
This sample web computes the approximate principal balance and amount
paid in interest each month, given an annual periodic interest rate,
and the principal balance the previous month.  The cells of the web,
and the formulas they contain are:

    cellname       formula
     apr       (No formula....constant entered by user)
     home          month! prin!
     itm           mi!*prin
               (computed monthly interest rate times principal)
     mi            apr/12  (annual periodic interest rate/12)
     month         month+1   (number of the month...start at 0)
     prin          if(prin>pymt) {  ( this month's principle recomputed)
                       prin+itm!-pymt
                   } elseif (prin>0) {
                       0
                   } else {
                       exit
                   }
     pymt      (No formula....constant payment amt entered by user)

The reference to mi in the formula itm, and the reference to itm
in the formula prin are execution references to make sure that the
values of mi and itm are recomputed before they are used on the first
iteration of the calculation.

The formula in the prin cell illustrates the use of the if structure
to control the number of iterations actually computed (no more than
necessary to reach zero principal).

The formula in the 'home' cell illustrates the use of multiple
expressions in a single formula.  The expressions are evaluated
from left to right.  Since these are execution references, the
cells referred to are recomputed each time 'home' is evaluated.
After each expression is evaluated, the value of 'home' is set to
the value of the expression.  Thus, when execution is complete,
the value of 'home' is found to be the same as the value of 'prin'.

To use this web, put the beginning principal into 'prin', the annual
periodic interest rate, as a decimal fraction, into 'apr', the
monthly payment into 'pymt', and initialize the 'month' to 0.  Set the
number of iterations (see Execution Preferences) somewhat larger
than you expect the number of required months to be, position to the
'home' cell, and choose 'Execute' from the Special menu.  The monthly
change in principal will be displayed as the calculation progresses.
Negative amounts indicate money accumulating rather than money being
paid out.

====|====RKutta
There is a method of iteratively solving second order differential
equations which is called Runge-Kutta.  By applying this method of
solution to the equation for simple harmonic motion;

    y''=-ay

one may obtain the following formulas, which we can program into a web:

    cellname      formula
---------------------------------
      home         yold! ypold! y! yp!
      h            (no formula...value is set to stepsize)
      a             constant parameter chosen by user
      yold         y
      y            yold+(k1!+4*k2!+k3!)/6
      ypold        yp
      yp           ypold+(k1p!+4*k2p!+k3p!)/6
      k1           h*ypold
      k2           h*(ypold+k1p!/2)
      k3           h*(ypold+2*k2p!-k1p!)
      k1p        -a*h*yold
      k2p        -a*h*(yold+k1!/2)
      k3p        -a*h*(yold+2*k2!-k1!)

Set initial values for for 'y' and 'yp'.  Set 'a' to a positive value,
position to the 'home' cell, and choose 'Evaluate' from the Special menu to
see a computed solution to this differential equation.

This example illustrates the use of multiple expressions within
the same formula.  The home cell contains a sequence of four
execution references to yold, ypold, y and yp.  This causes the
current values of y and yp to be placed into yold, and then
new values to be computed based upon those old values. 

====|====Solve
This example illustrates the use of the script language in conjunction
with one of the built-in functions,gaussj(), to solve four linear equations
in four unknowns, for two separate right hand side vectors.

cellname        script
------------------------------------------------------------
    cdx          no script, column index

    home	     out("\nCoefficient Matrix")
                 inputm(4,4)
                 out("\nRight side Matrix")
                 inputy(4,2)
                 if( gaussj("m11",4,"y11",2)=0 ) {
                     out("\nSolution vectors:")
                     outputy(4,2)
                 } else {
                     out("\nMatrix is singular")
                 }

    inputm       set inname to "inputm"
                 out("\n")
                 for(rdx=0 to (%1-1)) {
                     for(cdx=0 to (%2-1)) {
                         in(m11[rdx*%2+cdx])
                     } #endfor
                 }#endfor
                 outputm(%1,%2)

    inputy       set inname to "inputy"
                 out("\n")
                 for(rdx=0 to (%1-1)) {
                     for(cdx=0 to (%2-1)) {
                         in(y11[rdx*%2+cdx])
                     } #endfor
                 }#endfor
                 outputm(%1,%2)

    m11-m44      sixteen cells for the coefficient matrix
                 (row column order)
                 (no scripts)

    output       cell to which output is directed
                 (no scripts)

    outputm      for(rdx=0 to (%1-1)) {
                     for(cdx=0 to (%2-1)) {
                         out(m11[%1*cdx+rdx]," ")
                     }
                     out("\n")
                 }

     outputy     for(rdx=0 to %1-1) {
                     out("\n")
                     for(cdx=0 to %2-1) {
                         out(y11[%1*cdx+rdx]," ")
                     }
                 }

     rdx         no formula -- row index

     y11-y24     eight cells for the two right hand side vectors:
                 (row column order)
                 (no scripts)

To use the web, enter sixteen numbers in row-column order in the
text field of the cell inputm.  These are the elements of the
4X4 matrix.  Enter eight numbers in row-column order in the text 
field of the cell inputy.  These are the elements of the 2X4
matrix which represents two different right hand sides of the
matrix equation:

       Mx=y

Each column of the y matrix is one possible right hand side.

Go to the home cell and execute its script.  It should perform
Gauss-Jordan elimination on the two matrices, putting the solution
vectors in the y matrix, and the inverse of the mij matrix into
the mij elements.  The output will be directed to the cell output.
 
BUILT IN FUNCTIONS

We have seen in the examples some of the built-in functions supplied
by Mathweb.  In this section we list each built-in function, and any
information necessary for its use.

====min(expr1,expr2,expr3,...)
This function returns the minimum of the values of the expressions
denoted by expr1, expr2, expr3, ...etc.

====max(expr1,expr2,expr3,...)
This function returns the maximum of the values of the expressions
denoted by expr1, expr2, expr3, ...etc.

====mean(expr1,expr2,expr3,...)
This function returns the arithmetic mean of the values of the
expressions denoted by expr1, expr2, expr3, ...etc.

====abs(expr)
This function returns the absolute value of the expression denoted
by expr.

====sign(expr)
This function returns +1 if the expression expr evaluates to a
positive value, -1 if it evaluates to a negative value, and 0 if
it evaluates to 0.

====stp(expr)
This function returns +1 if the expression expr evaluates to a
non-negative value (including 0), and 0 if it evaluates to a
negative value.

====sig(expr)
This function returns the sigmoid logistic function of its
single expression argument:

                 sig(expr):= 1/(1+exp(-expr))

====sin(expr)
This function returns the value of the sine of expr.  The value of
expr is assumed to be in the trigonometric units chosen in the
trigonometric units choice list of the 'Numbers Preferences' dialog
reached from the 'Preferences' choice in the Special menu.

====cos(expr)
This function returns the value of the cosine of expr.  The value of
expr is assumed to be in the trigonometric units chosen in the
trigonometric units choice list of the 'Numbers Preferences' dialog
reached from the 'Preferences' choice in the Special menu.

====tan(expr)
This function returns the value of the tangent of expr.  The value of
expr is assumed to be in the trigonometric units chosen in the
trigonometric units choice list of the 'Numbers Preferences' dialog
reached from the 'Preferences' choice in the Special menu.

====asin(expr)
This function returns the value of the arcsine of expr.  The value
is returned in the trigonometric units chosen in the trigonometric
units choice list of the 'Numbers Preferences' dialog reached from
the 'Preferences' choice in the Special menu.  An invalid argument
will cause the value returned to be flagged "Not A Number".

====acos(expr)
This function returns the value of the arccosine of expr.  The value
is returned in the trigonometric units chosen in the trigonometric
units choice list of the 'Numbers Preferences' dialog reached from the
'Preferences' choice in the Special menu.    An invalid argument will
cause the value returned to be flagged "Not A Number".

====atan(expr)
This function returns the value of the arctangent of expr.  The value
is returned in the trigonometric units chosen in the trigonometric
units choice list of the 'Numbers Preferences' dialog reached from
the 'Preferences' choice in the Special menu.  An invalid argument
will cause the value returned to be flagged "Not A Number".

====sqrt(expr)
This function returns the non-negative square root of
the value of expr.

====ln(expr)
This function returns the value of the natural logarithm of expr.
An invalid argument will cause the value returned to be flagged
"Not A Number".

====exp(expr)
This function returns the value of e ( the base of natural logarithms)
raised to the power of expr.  An invalid argument or overflow will
cause the value returned to be flagged "Not A Number".

====log(expr)
This function returns the value of the logarithm to the base 10
of expr.  An invalid argument will cause the value returned to be
flagged "Not A Number".

====pow(expr1,expr2)
This function returns the value of the first argument raised to
the power of the second.

====rand()
This function returns a random number in the interval [0,1).  To
reseed the random number generator, call it with a single argument
which is the seed you wish to use, for example, rand(1365).

====first(expr1,expr2)
Returns a string containing the first expr2 bytes of the string
value of expr1.

====last(expr1,expr2)
Returns a string containing the last expr2 bytes of the string
value of expr1.

====butfirst(expr1,expr2)
Returns a string containing all but the first expr2 bytes of the
string value of expr1.

====butlast(expr1,expr2)
Returns a string containing all but the last expr2 bytes of the
string value of expr1.

====slen(expr)
Returns the number of bytes in the string value of expr.

====rref("cellname",expr1,expr2)
Produces the reduced row echelon form of the matrix beginning
with the quoted cellname, extending in row-column order for the
number of rows and columns specified in expr1 and expr2.  Assumes
that numbers smaller than 0.5E-d are zero (d is the number of
decimal digits specified in the Numbers preferences if the number
format is fixed or exponential.  d=14 if number format is general.)
The cellname supplied must be placed in quotes.

====gaussj("cellname1",expr1,"cellname2",expr2)
Performs Gauss-Jordan elimination on the matrices beginning with
cellnames 1 and 2.  The first matrix is taken to be a square matrix
with expr1 rows and columns.  The second matrix is a collection
of zero or more right hand side vectors.  The function solves
the matrix equations:
        Mx=y
where y is one of the right hand side vectors.  The solutions
replace the right hand side vectors.  The square matrix is 
replaced by its inverse, if it has one.  If the square matrix
is singular, the function returns -1, otherwise it returns 0.
The cellnames supplied must be placed in quotes.


MENUS AND PREFERENCES

====Mathweb Menus
There are five Mathweb menus, called File, Edit, Cell, Browse, and Special.  The dialogs and functions invoked from these menus are described below:

====|====File Menu
The File menu has three entries: New, Open, and SaveAs:
    *    New saves the contents of the current web, and creates a new,
         empty web with the name NEW.
    *    Open displays a dialog box into which the name of the file to
         be opened can be entered.  If the desired file is not in the
         \web\ directory, press TAB to enter the extended file open
         dialog.
    *    SaveAs displays a dialog box into which the name is entered
         under which the current web will be saved.  To save into a
         directory other than the current one, press TAB to enter
         the extended file dialog.  Once the target directory is
         located, press return to re-enter the dialog box for the
         file name.

====|====Edit Menu
The entries in the Edit menu are Copy, Paste, Bring, and Find:
    *    Copy saves the current selection in the edit box to the
         clipboard. (Make a selection by holding down the shift key
         and moving the cursor to cover the text to be selected).
    *    Insert pastes the contents of the clipboard into the current
         cursor position.  There is no Delete in the edit menu.
         To delete the current selection in the edit window, press
         the Delete key.  The selected text will be removed from the
         edit window and saved in the clipboard.
    *    Bring checks to see if any other application capable of exporting
         text has a block of text selected.  If so, the most recent such
         selection is copied into the cursor position of the current
         pane of the current cell.  The selection is also pasted into
         to the clipboard for future Insert operations.
    *    Find searches forward or backward for the next occurrence of
         the currently selected text.  If no text is selected, a dialog
         box appears, which allows the entry of the text to search for,
         as well as the preferred direction of search.

====|====Cell Menu
The entries in the cell menu are Add, Delete, Goto, Linkto, Hunt, and
Unlink:
    *    Add asks for the name of the cell to add.  Press Esc to cancel
         the request.  Otherwise, enter the new cell name and press
         enter.  If a cell with the new name already exists, a dialog
         will inform you of the fact, and ask if you wish to see that
         cell.  Otherwise, the new, empty cell will be displayed.
    *    Delete causes the current cell and its contents to be deleted,
         unless it is the home cell.  The home cell cannot be deleted.
    *    Goto transfers to a different cell, without saving the current
         cell into the backtrack(see Unlink).  If there is a cell name
         selected in the current edit window, that is the cell to which
         a transfer is made.  If not, a dialog appears, asking for the
         name of the cell to go to.
    *    Linkto saves the current cell into the backtrack, and transfers
         to a different cell.   If there is a cell name selected in the
         current edit window, that is the cell to which a transfer is
         made.  If not, a dialog appears, asking for the name of the
         cell to go to.
    *    Hunt scans from the current cursor position forward, looking
         for a cell name. If it finds one, it saves the current cell
         in the backtrack, and transfers to the cell whose name was
         found.
    *    Unlink returns to the cell most recently added to the trail.
         If there is no cell on the trail, Unlink goes to the home
         cell. The contents of the trail can be seen by choosing
         Trail from the browse menu.

====|====Browse Menu
The cells in a web can be browsed in alphabetical sequence or reverse
alphabetical sequence by choosing Prev or Next from the Browse menu.

By choosing Overview, one may see a window containing the names of
all the cells in the web.  Position to a particular cell by using
the up or down arrows, or by typing the first character in the
cell name.  Pressing ENTER causes a linkto to the highlighted cell.
Pressing SHIFT-ENTER causes a goto to the highlighted cell.  Pressing
ESC causes an exit from the browser to the current cell.

Choose Trail to see the cells currently on the trail as a result
of previous linkto or hunt requests.  Pressing ENTER causes a linkto
to the underlined cell.  Pressing SHIFT-ENTER causes a goto to the
underlined cell.  Pressing ESC causes an exit from the browser to
the current cell.

====|====Special Menu
The entries in the Special menu are About, Report, Execute,
Preferences, ZoomIn, ZoomOut, and Exit.
    *    About displays information about Mathweb.
    *    Report causes a report of the contents of the cells to be issued.
         The destination of the report output, and the content and format
         of the report are controlled by the Report entry of the
         Preferences dialog.
    *    Execute causes Mathweb to execute the script of the cell
         which is currently shown.  If that cell makes execution
         references to other cells, those cells too are executed.
         Input source, Output destination, Number format, and type
         of calculation are controlled by options reached from the
         Preferences entry of the Special menu.
    *    Preferences provides the means to control source of input,
         destination of output, format of numbers, parameters of
         execution, Search direction, Report format, and Plotting
         parameters.
    *    ZoomIn allows the user to increase the font size.
    *    ZoomOut allows the user to decrease the font size.
    *    Exit causes the Mathweb program to stop executing and return to
         the system screen.

MATHWEB PREFERENCES

The 'Preferences' entry in the Special menu opens a dialog box that
gives the user the opportunity to set six different classes of
parameters to control the operation of the program.  The six categories
are: Input, Output, Numbers, Calculation, Searching, and Report.

====Input Preferences
Input preferences govern the source of script execution input, and
whether input is echoed to the output stream.  The three input options
are: Source, Source Name, and Echo Input?.

====|====Source
Input source can be set to Keyboard, Cell, or File.
    *    If Keyboard input is chosen, the input stream is taken from
         input dialogs created by the IN statements in the script.
    *    If Cell input is chosen, the input stream is taken from the
         text field of the cell named in the Source Name selection.
    *    If File input is chosen, the input stream is derived from
         the file named in the Source Name selection.
         NOTE: This choice works only on registered copies of the program.

====|====Source Name
    The source name string may be set to any valid cell name or 
    file name.
    If Input Source is set to 'Cell' and no Source
    Name is supplied, or if no such cell exists, a new cell called
    INPUT is created, and the program attempts to obtain its first
    value from that cell, resulting in an immediate end of file
    indication in the system variable called eof.
    If Input Source is set to 'File' and no name is supplied in
    Source Name, the file name INPUT is assumed.  If there is no
    file by the name in the Source Name string, execution of the
    script terminates with a File Does Not Exist error.

====|====Echo Input?
    If Echo Input? is set to Yes, then characters which come in the 
    input stream are echoed to the output stream.  Otherwise, input
    characters are not echoed.

====Output Preferences
Output preferences govern the destination, and to some extent, the format
of the output of calculations and reports.  The three output options
are: Destination, Destination Name, and Newline Type.

====|====Destination
Output destination can be set to Screen, Cell, or File.
    *    If the Destination is set to Screen, output from reports
         and calculations appears one screen at a time on the LCD.
         After each screen the user is prompted to enter ESC to abort
         the calculation or report, or any other key to continue it.
    *    If the Destination is set to Cell, output from the calculation
         or report is copied to the end of the text area of the cell
         specified in the Destination Name option.  Since the capacity
         of the text area of a cell is limited to 2048 bytes, the text
         area only retains the last 2048 bytes of any output directed
         to it.
    *    When the destination is set to File, the output from
         calculations and reports is directed to the file named in
         Destination name.
         NOTE: This choice works only on registered copies of the program.

====|====Destination Name
The Destination Name option provides a place for the user to enter
the cell or file name into which output from calculations or reports
is directed.  If the Destination name option is empty, or does not
contain a valid cell name when the Cell or File destination is
selected, output is directed to a cell or file called OUTPUT
(or OUTPUT.TXT).  If there is no cell (or file) called OUTPUT, one
is created.

====|====Newline Type
The third Output Option is the Newline Type Option, which applies only
when the output destination is File.  The option lets the user choose
between newlines composed of linefeeds, or newlines composed of
carriage returns, followed by linefeeds.  The former option is
preferred when the files will not be exported to other computers.  The
latter option is preferable if the files will be exported to a
PC-compatible computer.

====Numbers Preferences
Numbers options govern the format of calculation results, number of
decimal places, and units of angular measure..  The three Number
options are:  Format, Number Of Digits, and Trigonometry Units.

====|====Format
The three Formats of numbers are General, Fixed, and Exponent:
    *    General numbers are floating point numbers with up to 15
         significant digits (a maximum of 24 digits overall)
    *    Fixed Numbers are fixed point numbers with the number of
         decimal places selected in the Number Of Digits option.
    *    Exponent Numbers are displayed in scientific notation with
         the number of significant digits specified in the Number of
         Digits option.

====|====Number of Digits
The number of digits option describes the maximum number of significant
digits in General and Exponent numbers, and the number of decimal places
in the Fixed format numbers.

====|====Trigonometry Units
The Trigonometry Units option can have the values of Degrees or Radians.
It governs the interpretation of arguments of trigonometric functions
and the presentation of results of inverse trigonometric functions.

====Execution Preferences
In Mathweb, script execution is controlled by the Execution Preferences
dialog reached from the Preferences choice of the Special Menu.

    Execution Preferences
    * Execution Results    <- Formatted List ->
    * List Format            %month,%prin
    * Number of Iterations         1

====|====Execution Results
This item refers to the automatic generation of output from
script execution.  One of three possible choices may be selected
in this dialog item:

====|====|====Don't List
No automatic output is listed as a result of script execution.
This choice provides the shortest time of execution.  Even though
no automatic output is generated, you may view the results (at the
end of the last iteration) by choosing to "Report" the values of
cells of interest, or by using the browser to visit each cell for
which a value is required.

====|====|====Generic List
Results are automatically presented for each cell executed. Typical
generic output looks like that shown below:

    apr,month,itm,prin,pmyt
    0.075,1,137.09,21593.41,481
    0.075,2,134.96,21249.51,481
    0.075,3,132.81,20903.46,481
    0.075,4,130.65,20555.27,481
    0.075,5,128.47,20204.92,481
    0.075,6,126.28,19852.39,481
    0.075,7,124.08,19497.67,481
    ...........................
This kind of output is primarily useful for cellular calculation
output when it is desired to generate the result of a calculation
with a minimum of effort.

====|====|====Formatted List
For formatted listing, a header may or may not be presented, depending
upon the content of the List Format (line 2 of the dialog).

The content of the formatted result chunk generated for each iteration
depends upon rules described in 'Formatting Exectuion Results',
which is the next section.

====|====List Format
The List Format string is used to describe how to present the output
of each iteration to the user.  See the section entitled "Specifying
Execution Output" for complete information on the List Format.

====|====Number of Iterations
The number of iterations is an integer between 1 and 65535.  It 
controls how many times the selected cells are calculated.  The
script language of Mathweb has other provisions for controlling the
number of iterations.  These are the 'for' statement, the 'while'
statement, and the 'do' statement.  These make it unnecessary to set
this preference to a number greater than 1 when other techniques are
in use.

====Search Preferences
When the user selects Find from the Edit menu, a search is performed
for selected text, or text specified in a dialog box.  The Search
preferences apply to the operation of the Find Request.  The available
options are:  Search For, Search Direction, and Case Sensitive?

====|====Search For
This is the string for which a search is made when the Find choice
is selected from the Edit menu.  No wild card characters or regular
expression specifiers are accepted here.  Enter just the character
string for which you are looking.

====|====Search Direction
The choices here are Forward and Backward.  When a direction is chosen,
the next search (Find) operation proceeds from the current cursor
position in the current field of the current cell, in the direction
indicated.  If a match is not found in the current cell, the search
continues into the next cell in the direction indicated.  The search
ends when either a match is found, or the search returns in a giant
circle to the beginning or end of the field in the cell in which it
began.
NOTE: When searching backward, an instance of the target which is
the last string in the text of formula field of a cell may be missed.
This problem will be repaired in a future release.


====|====Case Sensitive?
If -Yes- is selected for this option, a match is declared only when
text matches as to characters and case of each character.  Otherwise,
a character match alone is sufficient.

====Report Preferences
Report Preferences are Report On All Cells?, Report Type, and
ReportFormat:

====|====Report On All Cells?
This option allows the user to choose whether to apply the report
function to all cells in the web, or to a selection of cells.  See
the section entitled "Specifying Report Output" for a complete
discussion of this preference and its application.

====|====Report Type
This option allows the user to choose between four different types
of reports: Formatted, InPointers, OutPointers, and Trail reports.

====|====Report Format
The Report Format string allows the user to specify the contents of
the formatted report generated for each selected cell.  The rules
governing the syntax and interpretation of the report format string
are discussed in the section entitled "Specifying Report Output".

====Plotting Preferences
Choosing "Plotting" from the Preferences menu results in the 
presentation of the following dialog:

    Plotting     <-On->
    xmax          1.53
    xmin         -1.53
    ymax          1e3
    ymin          1e2
    X Intervals    10
    Y Intervals     5
    X,Y Cells     x,y

====|====Plotting
If Plotting is set to 'On' then the plot window will be enabled,
and the two cells specified in the X,Y Cells section of the dialog
will be automatically plotted against each other.  A new point will
be generated each time both cells change.

====|====xmax
xmax should be set to the maximum value of x to be included within
the plot window.

====|====xmin
Set xmin to the minimum value of x to be included within the
plot window.

====|====ymax
Set ymax to the maximum value of y to include in the plot window.

====|====ymin
Set ymin to the minimum value of y to include.

====|====X Intervals
Set this to the number of intervals to be marked off in the plot
window with vertical grey lines.

====|====Y Intervals
Set this to the number of intervals to be marked off in the plot
window with horizontal grey lines.

====|====X,Y Cells
Set this string to the names of the two cells to be associated with
the x and y directions, separated by a comma.  The first cell name
will be associated with the x axis.  The second with the y axis.

EXTENDED SCRIPT LANGUAGE
Mathweb allows script fields to consist of statements and 
expressions from a structured scripting language.  The next
sections describe the Mathweb Script Language in more detail.

====Filter -- A Script Example
The web called Filter has eight cells, in which two kinds of
digital filters are illustrated.  The first is a finite
impulse response, or FIR filter, which averages three delayed
samples of an input signal, to produce a 'filtered' copy of
the signal.  The operation of the FIR filter is specified in
the script field of the cell called 'fir':

    set tap1 to 0
    set tap2 to 0
    set tap3 to 0
    out("\nFIR Filter Example")
    out("\ninput signal,output signal)
    while(not eof) {
        in("\n",INPUT)
        set tap3 to tap2
        set tap2 to tap1
        set tap1 to INPUT
        set fir to tap1/4+tap2/2+tap3/4
        out(",",fir)
    }

The first thing fir does is to zero the three signal
accumulation cells tap1, tap2, and tap3.

Then it begins a loop which will continue until it runs out
of input, which is received from the input stream one value
at a time in a cell called INPUT.

Each time an input value is received, it is shifted into the
three part delay/accumulator composed of tap1,tap2, and tap3.
Each time an input value is received, a weighted sum is taken
of the new values of tap1,tap2, and tap3.  The weighted sum
is stored temporarily in the cell fir, and output to the
output stream.  Then the loop goes back to get the next
input value.

For input like that shown below:
    0
    1
    1
    1
    1
    0
    0
    0
    0
    1
    1
    1
    1
the output looks like:
FIR Filter Example
    input signal, output signal
    0,0.00
    1,0.25
    1,0.75
    1,1.00
    1,1.00
    0,0.75
    0,0.25
    0,0.00
    0,0.00
    1,0.25
    1,0.75
    1,1.00
    1,1.00

====Script Language Data Structures
There are three kinds of data structures in the Mathweb script
language: cells, system constants, and system variables.

====|====Cells
Cells contain a unique name, a text field, a value, and a script
field.  Cells are referred to in the script language by their names.
Cellnames are 1 to 48 characters beginning with a letter, followed by
zero or more letters, digits, or underscore ('_') characters.

====|====|====System Constants
There are three system constants: pi, e, and deg.  pi is always
interpreted as the value of the ratio of the circumfrence to the
diameter of a circle.  e is interpreted as the value of the base
of natural logorithms. deg is the number of degrees in a radian.

====|====|====System Variables
There are eight system variables: eof, inname, outname, RptFmt, RptType,
NumFmt, NumDec, and TrigUnits.  eof may be used in expressions.  It
takes the value 1 of the end of file has been reached on the input
stream.  Otherwise, eof takes the value 0.  eof may not be set by the
script.  The remaining variables may be set using the set statement, but
they may not be used in expressions.  inname and outname may be set to
the names of cells or files which are to provide input or accept output
respectively.

====|====Cell References
References to cells in the script language are of two kinds:
value references and execution references. Value references consist
simply of the cell name. Execution references consist of the name of
the cell followed by an exclamation point, '!', or an argument list
enclosed in parentheses.

====|====|====Value References
When a value reference appears in the script language, it is
interpreted by substituting the value of the cell indicated.

====|====|====Execution Referrences
When an execution reference appears in the script language, it is
interpreted by executing the script of the named cell, with any
arguments supplied, and then substituting the (possibly new) value
of the cell executed.

====|====Subscripted Cell References
To facilitate array operations and enhance control of subprogram
execution, subscripted cell references, both value references and
execution references, are allowed by the script interpreter.

A subscripted cell reference consists of a cell name followed by a
left bracket, followed by an expression, followed by a right bracket,
possibly followed by an exclamation point or argument list.  Examples
of valid subscripted cell references are shown below:

       a[1]
       a[1]!
       a[1]()
       a15[4+3*j](cellx,celly,1+cellz)
       newCell[a[24]!+5.1]
       x[sign(sin(y))]
       PanDowdy[newCell[a[24]!+5]!-21]

The meaning of a subscripted cell reference is as follows:
Imagine that the cells are arrayed in alphabetical order.  If the
subscript expression evaluates to the number floating point number F,
N is the largest integer less than or equal to F, and the cell name is 
twizzle; then the subscripted reference is presumed to refer to the
Nth cell past twizzle in the alphabetical sequence of cells.  A
reference to a[0] is the same as a reference to a.  Subscripts less than
zero or to cells beyond the end of the sequence result in the
generation of a subscript error.

====|====Comments
The # character, when it appears in a Mathweb script or input stream,
denotes the beginning of a comment.  The comment ends at the next
carriage return or line feed.

====|====Script Language Statements
Each Mathweb script consists of zero or more statements.  If there are
no statements in the script when Execute is chosen from the Special menu,
then nothing happens.

If the execution of an empty script in cell a takes place as a result
of an execution reference from another script ( a! or a() ), the
value returned to the other script is the content of the value field
of cell a.  If there is nothing in the value field either, then the
value 0 is returned.

If the script of a cell contains legal statements in the script language,
whether executed from the menu, or from another script; the statements of
the script are interpreted in order until the script ends, or an exit 
statement is encountered.  When the script ends or is exited, control
returns to the calling script (the one which used the execution
reference), or to the Mathweb top level screen.

The statements of Mathweb script language are described and illustrated
below.  For more detailled information, see the section entitled
Script Language Grammar.


                    SET STATEMENT

The set statement consists of the word 'set' followed by a cell name,
the optional word 'to', and an expession.  The action of the interpreter
is to set the value of the named cell to the value arrived at by 
interpreting the expression.  The set statement:

    set home to 13-a*5

will obtain the value of the cell called a, multiply it by 5,
subtract the result from 13, and place the result into the cell
called home.  A slight variation on the statement:

    set home to 13-a!*5

will execute the script in the cell called a, (which may change the
value of cell a), obtain the (possibly new) value from cell a,
multiply it by 5, subtract the result from 13, and place the result
into the cell called home.

There is a variant of the set statement which may be used to change
certain configuration choices for Mathweb during script execution.
In the variant, a so-called configuration word is substitued for the 
cellname which follows the word set.  Configuration words and the
parameter changed are listed below:

Configuration	     Parameter
word			changed
-----------------------------------------------------
RptFmt             report format string
RptType            report type
         (FORMATTED, INPTRS,OUTPTRS, or TRAIL)
NumFmt		    numeric format
         (FIXED, EXPN, or GENERAL)
NumDec		    expression for number of decimal
                  places, or significance of result (0-15)
TrigUnits    (DEGREES or RADIANS)
inname             cellname or filename providing input
outname            cellname or filename accepting output

Examples of the set statement variant include:

    set RptFmt to "\n\N=\V\n"

    set TrigUnits to RADIANS


                           IF STATEMENT

The If statement can take any one of the variants shown below:

if(a!=b) {
    zero or more statements
}

NOTE: (a!=b) does not mean a not equal to b.  it means execute
the script of cell a.  If the result is equal to b, then...)

NOTE:The contents of the parentheses in an if statement must
be an expression, conditional or arithmetic.

if(a<>b) {
    zero or more statements
} else {
    zero or more statements
}
(This is how you check to see if a is not equal to b.)


if(a<10) {
     zero or more statements
} elseif(a<5) {
     zero or more statements
} elseif(a<3) {
     zero or more statements
} elseif(....) {
.......
} else {
     zero or more statements
}


                          WHILE STATEMENT

The while statement looks like this:

while(expression) {
    zero or more statements
}
If the contents inside the parentheses evaluate to a non-zero value,
then the contents inside the curly braces will be repeatedly executed
until the expression in parentheses becomes zero.


                         DO STATEMENT

do {
    zero or more statements
} while ( expression )

Mathweb first executes the statements between the curly braces.  Then it
checks to see if the expression is non-zero.  If so, it executes the
statements between the braces again, and then checks to see if the 
expression is non-zero,... , and so on.

This statement differs from the while statement in that the statements
between the braces are always executed at least once.



                         FOR STATEMENT

There are two variants of the for statement:

for ( cellname=expression to expression step expression ) {
    zero or more statements
}

and

for(cellname=expression to expression) {
     zero or more statements
}

Mathweb executes the statements between the curly braces so long as
the value of cellname has not gone past the value of the second
expression, whether stepping from an initial value above or below
the ending value.

for instance, the statement:
     for(x=1 to 3) {
           out("mugga")
      }

executes the out statement three times; whereas the statement:

      for(x=3 to 3) {
             out("mugga")
       }

executes it once.

Also, the statement:

      for(x=3 to 1 step -1) {
             out("scud")
       }

causes the word scud to be output 3 times; whereas the statement:

       for(x= 3 to 1) {
           out("scud")
        }

will never output the word scud, since the step size is presumed to be +1.


                         IN STATEMENT

The format of the in statement is:

    in("this is a prompt",x,y,z,...,"\nthis is another prompt",a,b,....)

The in statement scans its argument list from left to right.  Each time
it encounters a string, it copies it to the current output (or sets
the input dialog prompt string if input is from the keyboard).  Each
time it encounters a cell name, the input statement scans the input
stream for a value to place into that cell.  If there is nothing else
in the input stream, it sets the eof system variable to 1.
Whenever "\n" appears as a string in an input statement, it is output
as a newline (unless the input is from keyboard).


                          OUT STATEMENT

The format of the out statement is:

out( sequence of expressions separated by commas )

Sample output statements:

    out("damned spot!",34,x+1,"whoof" cat "or all the seas with oysters")
    
    out("\nPounds of butter:",butter)

or

     out(x,y,z)

Whenever "\n" appears in a string in the output statement it is output
as a newline.


                          REPORT STATEMENT

The format of the report statement is:

       report(x,y,another_cellname,z,w.....)

When executed, the report statement sends to the current output a report
on each of the cells in its argument list, in the order encountered,
using the current report type and format.



                          EXIT STATEMENT

The exit statement consists simply of the word:
            exit
No matter where the statement is encountered, it causes the immediate
terminaiton of script execution, and the issuance of the message:

EXIT REQUESTED IN CELL xxxxx



                        COMPOUND STATEMENT

A compound statement consists of a left curly brace followed by zero
or more statements followed by a right curly brace:

{
    if(a){
         out("\nRound the corner off")
    }
    out("\nEat pie for dessert")
}



                       EXPRESSION STATEMENT

An expression statement is a conditional or arithmetic expression,
such as shown on each of lines below:

          1
         17
        a<b
       NOT x!
      a+14*sin(221-4)
       (((1)))
      (a(x,1) AND b)+14

NOTICE:  Whenever the script interpreter encounters an expression when
it is looking for a statement, it evaluates the expression and sets the
value of the cell in which it finds the expression to the value returned
by the expression. 

====|====Automatic Ouptut
Automatic output is the generic or formatted output selected in the
Execution Preferences dialog.  It is particularly useful when doing
cellular calculations, and the scripts contain no 'out' or 'report'
statements.

====|====Script Generated Output
Two script language statements, 'out' and 'report', send data to
the output stream.  Both statements are described in the section
above called 'Script Language Statements'.

CAUTION AGAINST USING BOTH KINDS OF OUTPUT AT ONCE
If the Generic and Formatted Calculation Output is not turned off by
setting Execution Results to 'Don't List', it will come out at the end
of each iteration, possibly messing up the format of the output
generated by script statements.

====|====Script Language Grammar
The permissable content of a script field is one or more statements.
Lines beginning with a '#' character are taken to be comment lines.
Wherever a '#' character appears in a formula, the remainder of that
line is taken to be a comment.

The grammar recognized by the Mathweb script interpreter is shown
in Backus Naur Form below (items in [ ] are optional):

<script> ::= <stmtlist>

<stmtlist> ::=    empty
                | <stmt> <stmtlist>

<stmt> ::=        <compound stmt>
                |  <set stmt>
                | "if" "(" <expr> ")" <compound stmt> <optelse>
                | "while" "(" <expr> ")" <compound stmt>
                | "do" <compound stmt> "while" "(" <expr> ")"
                | "for" "(" cellname "=" <expr> "to" <expr>
                                      ["step"  <expr>] ")" <compound stmt>
                | "in" "(" <inlist> ")"
                | "out" "(" <outlist> ")"
                | "report" "(" <cellist> ")"
                | "exit"
                | <expr>

<compound stmt> ::= "{" <stmtlist> "}"

<set stmt> ::= "set" <configwd> ["to"] <configval>
                |  "set" <configwd> ["to"] <string>
                |  "set" <configwd> ["to"] number
                |  "set" <cellname> ["to"] <expr>
                |  "set" <cellname> "["<expr>"]" ["to"] <expr>               

<optelse> ::=     empty
                | "else" <compound stmt>
                | "elseif" "(" <expr> ")" <compound stmt> <optelse>

<inlist>    ::=  <inspec>
                | <inspec> "," <inlist>

<inspec>   ::= <string> | cellname

<outlist>    ::=  <expr>
                 | <expr> "," <outlist>

<outspec>   ::=  <expr> | <string>

<cellist>    ::=  cellname
                 |  cellname <cellist>

<expr> ::=        <expr>
                | <pexpr> <relop> <pexpr>

<relop> ::=       "=" | "<" | "<=" | ">=" | "<>"

<pexpr> ::=       <term prefix> <termlist>

<term prefix> ::= empty | "+" | "-"

<termlist> ::=    <term>
                | <term> <termop> <termlist>

<termop> ::=      "+" | "-" | "OR" | "cat"

<term>   ::=      <factor>
                | <factor> <factop> <term>

<factop> ::=      "*" | "/" | "%" | "AND"
                
<factor>  ::=     cellname
                | cellname "!"
                | cellname "("<arglist>")"
                | cellname "[" <expr> "]"
                | cellname "[" <expr> "]"!
                | cellname "[" <expr> "]""("<arglist>")"
                | a number
                | <string>
                | "NOT" <factor>
                | "pi"
                | "deg"
                | "e"
                | "eof"
                | <builtin>
                | "(" <expr> ")"

<builtin>  ::=  <funcname> "(" <arglist> ")"

<funcname> ::= "min" | "max" | "mean" | "abs" | "sign" | "stp"
                  | "sig" | "sin" | "cos" | "tan" | "asin" | "acos"
                  | "atan" | "sqrt" | "ln" | "exp" | "log" | "pow"
                  | "rand" | "first" | "last" | "butfirst"
                  | "butlast" | "slen" | "rref" | "gaussj"

<arglist>    ::= empty
                  | <expr>
                  | <expr> "," <expr>
                  | <expr> "," <expr> "," ....... "," <expr>

<configwd>   ::= rptfmt | rpttype | inname | intype | outname
                 | outtype | numfmt" | numDec | trigunits

<configval>  ::= formatted | inptrs | outptrs | trail | fixed | expn
                  | general | degrees | radians | cell | file 
                  | screen | keyboard | quoted cell or filename

<string>   ::=    <quote> zero or more characters <quote>
                  | <identifier> 

<quote>  ::=     ' | " 
   
NOTE:  Ending quote character must be same as beginning one.

<identifier> ::=  a letter followed by zero or more letters,digits,
                  or underscores, which is not a builtin, a cellname,
                  a system constant, a system variable, or a reserved
                  word.

====Errors, Limits, License, and Support
====|====Error Conditions

====|====|====Out of Memory Error
When Mathweb writes a web to a disk, it writes it
first as a temporary file, called temp.tmp, and then
renames the temporary file to the name of the web.

If a memory shortage prevents the entire web from 
being written, the update is not completed, and the
old version of the web remains on the disk.

====|====|====Script Errors
When a script is executed, there are several error messages that
can result.  These messages are listed below:

BAD IDENTIFIER IN CELL XXXXX
One of the identifiers in the formula for the cell named xxxx is
neither a cell name nor a built-in function.

ILLEGAL CHARACTER IN CELL XXXXX
There is a character in formula xxxx that is not permitted.

STACK OVERFLOW IN CELL XXXXX
The formula in cell xxxx contains a cell reference which is part of
a circular chain of cell references, which is not permissable when
all the references in the circular chain are execution references.

MISSING PAREN IN CELL XXXX
Self Explanatory.

MISSING '{' IN CELL XXXXX
An If statement is missang a left curly brace in the formula field
of cell xxxx.

MISSING '}' IN CELL XXXXX
An If statement is missang a right curly brace in the formula field of
cell xxxx.

SUBSCRIPT MISSING ']' IN CELL XXXXX
A subscript was not terminated with a right bracket.

SUBSCRIPT OUT OF RANGE IN CELL XXXXX
A subscript was less than zero or greater than the number of cells
in the web.

INVALID EXPRESSION IN CELL XXXXX
Problem with expression syntax.

EXPECTED FORMAT STRING (IN QUOTES) IN CELL XXXX
Set statement expected calculation or report format string.  It got
something else.

INVALID REPORT TYPE IN CELL XXXXX
Set statement expected a report type of Inptrs, Outptrs,
Trail or Formatted.  It found something else.

ILLEGAL STATEMENT IN CELL XXXXX.
Script interpreter was looking for a statement and found
something that wasn't.

BAD SET STMT IN CELL XXXXX
A set statement had improper syntax.

OBJECT OF SET MUST BE RESERVED WORD IN CELL XXXXXX
The word set was not followed by either a cellname or a valid
configuration word of RptTypeX, RptFmtX, NumFmtX, NumDecX,
or TrigUnitsX.

NUMBER FORMAT SHOULD BE FIXED, EXP, OR GENERAL IN CELL XXXXX.
A 'set NumFmtX' statement should have set the value to fixed,
exp, or general.

BAD NUMBER OF DECIMAL PLACES IN CELL XXXXX.
The number of decimal places specified in a 'set NumDecX" statement
was less than zero or greater than 15 or not numeric.

TRIG UNITS TYPE SHOULD BE DEGREES OR RADIANS IN CELL XXXXX
Set statement setting trig units set it to something illegal.

BUILTIN FUNCTION CALL ERROR IN CELL XXXXX
There is a problem with a comma or parenthesis in a built-in
function reference in the formula field of cell xxxx.

BUILTIN FUNCTION ARG ERROR IN CELL XXXXX
There is a problem with the number or type of arguments to a built-in
function in the formula field of cell xxxx.

ARGUMENT LIST ERROR IN CELL XXXXXX
There is a problem with the argument list of an execution
reference to another cell in the cell named.

EXPECTED ARGUMENT NUMBER IN CELL XXXXXX
In the specified cell script, a percent sign was
not followed by a integer denoting the argument number.

ARGUMENT NUMBER INVALID IN CELL XXXXXX
An argument number following a percent sign is either too
big or negative in the script of the cell specified. 
POORLY FORMATTED CELL LIST IN CELL XXXXX
A report statement had an improperly formatted cell list.

BAD DO STMT SYNTAX IN CELL XXXXX
Bad syntax in a do statement.

BAD FOR STATEMENT IN CELL XXXXX.
A for statement in the specified cell had an error in some portion
of the statement, other than the compound statement portion.

EXPECTED '=' IN CELL XXXXXX
An error in a for statement.

EXPECTED 'TO' IN CELL XXXXXXX
An error in a for statement.

SYNTAX ERROR IN CELL XXXXX
There is an error in the script field syntax of cell xxxx.

MISSING ',' OR ')' IN OUT STMT IN CELL XXXXX
Self Explanatory.

EXPECTED '(' IN CELL XXXXX
A left parenthesis was expected, but not found, in the script
field of cell xxxxx..

EXPECTED ')' IN CELL XXXXX
A right parenthesis was expected, but not found, in the script
field of cell xxxx.

ATTEMPTED TO READ PAST END OF FILE IN CELL XXXXXX
Self Explanatory.

BAD IN STATEMENT IN CELL XXXXX.
Error in an 'in' statement.

SCRIPT ERROR IN CELL XXXXX.
There is an otherwise unclassified error in the script of the
specified cell.

INTERNAL SOFTWARE ERROR IN CELL XXXXX
There is a bug in Mathweb.  Please report the circumstances to David
Clifton at eieio@boulder.earthnet.net, or 71270.1441@compuserve.com.

MISUSE OF RESERVED WORD IN CELL XXXXX
One of the reserved words, (any word in quotes in the grammar),
is out of place in cell XXXXX. 

UNEXPECTED END OF SCRIPT IN CELL XXXXX
The script field of cell xxxx ended before a complete expression
was parsed.

BAD XCELL SPEC IN PLOT DIALOG
The first name (before the comma) in the X,Y Cells part of the
plot dialog is not a valid cell name for this web.

BAD YCELL SPEC IN PLOT DIALOG
The second name (after the comma) in the X,Y Cells part of the
plot dialog is not a valid cell name for this web.

INNVALID PLOT LIMITS
The value of xmin,xmax,ymin, and ymax are such that the difference
of the x's or the difference of y's are zero, or so small or so large
that they cannot be expressed as a floating point number with Psion
arithmetic routines.

LIMITS, WARRANTIES, AND SUPPORT

====Program Limits
The following limits apply to version 1.2 of Mathweb:

    Maximum Size of a Cell Name:	              48		
    Maximum Characters in a Text Pane:        2047
    Maximum Characters in a Value Pane:        255
    Maximum Characters in a Script Pane:      2047 
    Maximum Number of digits in a number:       24
    Maximum Significant digits in a number:     15
    Maximum Number of Cells:                 limited by 64K
                                             program data
                                             data segment size.

====Warranty, License, Registration, and Distribution


         LIMITED WARRANTY AND DISCLAIMER OF WARRANTY 
 
THIS SOFTWARE AND ACCOMPANYING WRITTEN MATERIALS (INCLUDING
INSTRUCTIONS FOR USE) ARE PROVIDES "AS IS" WITHOUT WARRANTY OF
ANY KIND. FURTHER, NEITHER EIEIO NOR DAVID CLIFTON PROVIDES ANY
WARRANT, GUARANTEE, OR MAKES ANY REPRESENTATIONS REGARDING THE
USE, OR THE RESULTS OF USE, OF THE SOFTWARE OR WRITTEN MATERIALS
IN TERMS OF CORRECTNESS, ACCURACY, RELIABILITY, CURRENTNESS,
OR OTHERWISE. THE ENTIRE RISK AS TO THE RESULTS AND PERFORMANCE
OF THE SOFTWARE IS ASSUMED BY YOU. IF THE SOFTWARE OR WRITTEN
MATERIALS ARE DEFECTIVE YOU, AND NOT EIEIO OR ITS DEALERS,
DISTRIBUTORS, AGENTS, OR EMPLOYEES, ASSUME THE ENTIRE COST OF ALL
NECESSARY SERVICING, REPAIR, OR CORRECTION.
 
THE ABOVE IS THE ONLY WARRANTY OF ANY KIND, EITHER EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, THAT
IS MADE BY EIEIO OR DAVID CLIFTON, ON THIS PRODUCT.  NO ORAL OR
WRITTEN INFORMATION OR ADVICE GIVEN BY DAVID CLIFTON, EIEIO, ITS
DEALERS, DISTRIBUTORS, AGENTS OR EMPLOYEES SHALL CREATE A WARRANTY
OR IN ANY WAY INCREASE THE SCOPE OF THIS WARRANTY AND YOU MAY NOT
RELY ON ANY SUCH INFORMATION OR ADVICE. YOU MAY HAVE OTHER RIGHTS
WHICH VARY FROM STATE TO STATE. 
 
NEITHER EIEIO NOR ANYONE ELSE WHO HAS BEEN INVOLVED IN THE CREATION,
PRODUCTION OR DELIVERY OF THIS PRODUCT SHALL BE LIABLE FOR ANY DIRECT,
INDIRECT, CONSEQUENTIAL OR INCIDENTAL DAMAGES (INCLUDING DAMAGES FOR
LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS
INFORMATION, AND THE LIKE) ARISING OUT OF THE USE OR INABILITY TO USE
SUCH PRODUCT EVEN IF EIEIO OR DAVID CLIFTON HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGES.
 
This Limited Warranty shall be governed and construed in accordance
with the laws of the State of Colorado. 

                           ACKNOWLEDGMENT
 
BY USING THE SHAREWARE VERSION OF MATHWEB YOU ACKNOWLEDGE THAT YOU HAVE
READ THIS LIMITED WARRANTY, UNDERSTAND IT, AND AGREE TO BE BOUND BY ITS'
TERMS AND CONDITIONS. YOU ALSO AGREE THAT THE LIMITED WARRANTY IS THE
COMPLETE AND EXCLUSIVE STATEMENT OF AGREEMENT BETWEEN THE PARTIES AND
SUPERSEDES ALL PROPOSALS OR PRIOR AGREEMENTS, ORAL OR WRITTEN, AND ANY
OTHER COMMUNICATIONS BETWEEN THE PARTIES RELATING TO THE SUBJECT MATTER
OF THE LIMITED WARRANTY.

                             LICENSE

You may use the demonstration version of Mathweb for as long as you like.
Should you wish to save files with names other than the ones
supported in the demonstration version of the program, you will 
need to register as a user of Mathweb, and pay the
registration fee.

You may distribute demonstration copies of Mathweb to other persons or
organizations, provided such distribution takes place in accordance
with the provisions of the DISTRIBUTION paragraphs below.


                            REGISTRATION

The registration fee for Mathweb is $33.  For this price, you will
receive a diskette with the latest shareware version of Mathweb, the
printed user's manual, a key to disable registration reminders, and a
collection of sample webs.

You can register Mathweb using the Compuserve shareware registration
service, RegNet, or by sending cash to the author.  For details on
ordering and payment, see below:

                       ORDERING AND PAYMENT

METHOD 1:  Compuserve shareware registration service

Log onto Compuserve and go to shareware registration under the 
computers menu (GO SWREG).  Access the registration database with
the program name, Mathweb, or the registration number 12884.

METHOD 2:  By credit card through RegNet, a shareware registration
service. (There will be a $5 handling charge for registrations
through RegNet.  RegNet processes registrations only, please
contact the author at 71270.1441@compuserve.com or
eieio@boulder.earthnet.net for product information or technical
support.)
You can order registration packets with credit cards by contacting
RegNet using any of the following methods:

Wold Wide Web: http://www.swregnet.com/351p.htm
Voice Phone: 1-800-999-2734
             1-805-288-1827  (International calls)
FAX:         1-805-288-1867
E-mail       orders@swregnet.com

Please have the following information ready:
1) Program name (Mathweb) or index number (#351)
2) Destination for registration packet
3) Your credit card number and expiration date

METHOD 3:  By sending cash to the author.

Send your mailing address, the desired platform (Psion3a),
and $33 to the address below:
    EIEIO
    P.O. Box 1086
    Niwot, CO 80544
    U.S.A.
Send cash in the form of U.S. dollars, a check drawn on a U.S. bank
(MICR code on the bottom), or International Money Orders for $33 (U.S.).
Add a 25% premium over the exchange rate if you send Canadian checks
in Canadian dollars, or major currencies (Pounds, Marks, Francs, or Yen).
 

                             DISTRIBUTION

You are encouraged to pass a copy of the shareware version of Mathweb
along to your friends for evaluation.  If you do so, you must provide
them with the entire set of Mathweb shareware version files. These include:

    README.TXT
    MATHWEB.APP
    VENDOR.DOC
    FILE_ID.DIZ
    CAVE.WEB
    AMORT.WEB
    PAYMENT.WEB
    LOGISTIC.WEB
    PERT.WEB
    PIGGY.WEB
    QUOTES.WEB
    RKUTTA.WEB
    KING.WEB
    CA.WEB
    HEBB.WEB
    ROOTS.WEB
    TRAPZD.WEB
    MATRIX.WEB
    SOLVE.WEB
    FILTER.WEB

Alternatively, you may provide the .ZIP file which contains the files
listed above in compressed format.

If you intend to charge for distribution, you must adhere to the
requirements of the vendor.doc file.


                        OBTAINING SUPPORT

You may send messages, reports, and requests for information
to David Clifton at either of the two E-Mail addresses below:

    eieio@boulder.earthnet.net
    71270.1441@compuserve.com

