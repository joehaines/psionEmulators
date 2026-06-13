                                README.TXT
                                   for
                                 SPYDERT
                               Version 1.42                            


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


SPYDERT INTRODUCTION

====What Is SpyderT?
SpyderT provides you with the ability to create hypertext webs of
text; and to search and report on the contents of those webs. Report
output may be directed to particular cells within a web, to the
screen, or to text files. Typical applications for SpyderT include: 
hypertext notepad and database, outline processor, design tool,
presentation planner and prompter, contingency planner, and help
system host.

Each cell in a SpyderT web contains a name and a text field. Within
the text field, references to other cells may appear, and be used as
hot-links to those cells.  Cells may refer to themselves or to other
cells which refer to them, making possible cycles of hypertext
reference.  The current version of SpyderT runs on the Psion Series
3a.

====Whom Is SpyderT For?
SpyderT is for anyone who must keep track of complex systems of any
kind, including thoughts, presentations, literary references, software
architectures, human relationships,..., you name it.  A certain
amount of practice is necessary to realize the maximum benefit from
the program.  This practice develops skills for navigating hypertext
webs, and promotes a network perspective on problems and systems.

====Installing SpyderT 
* Before installing SpyderT on your Psion 3a, create a \web\ directory.
This is where the program will keep its data files, unless you direct
it to store them elsewhere.

* If there isn't one already, create an \app\ directory on the
datapack which you intend to hold the SpyderT executable.

* Use MCLINK, WINLink3, PsiWin or some other PSION remote file
transfer program to copy SpyderT.app to the \app\ directory on your
chosen solid state disk drive.

* Then transfer the following sample webs to the \web\
directory that you created above:  	Quotes, Websites, Grammar,
and Cave.

* Choose Install from the Apps menu of the system screen, 
and select SpyderT for installation.

* If all goes well, the SpyderT icon will appear on the 
desktop, along with names of the sample webs which you 
copied to the \web\ directory.


GETTING STARTED

====Using An Existing Web
When SpyderT was installed, several sample webs were installed in the
\web\ directory as well.  Names of those webs should be displayed
under the SpyderT icon (a spider hanging from a thread) on the
desktop.  Select the web called "Cave", and press return.  A window,
like the one shown below, will be displayed:

The window is divided into two panes.  The top one, containing the
text "CAVE--home", is the name pane.  It is a read-only pane which
always contains the name of the web followed by the name of the 
currently displayed cell.

The bottom pane is the text pane.  It contains an edit box in which
text may be entered into the text field of the current cell.

Every SpyderT web contains a "home" cell, which is displayed on entry
to the web; and which cannot be deleted.  The text field of the home
cell is frequently used to describe the purpose of the web, and to 
present a simple map of its contents.  This information is not
supplied for the Cave web, because the contents are meant to be
explored.

====Exploring the Cave.
You may use the arrow keys to scroll the text field of the home cell
of the Cave web.  Read the text, and make your choice whether to
'&EnterTheTrapDoor' or '&GoHomeAndWatchTV'.  Once you have made your
choice, you can carry it out by positioning the cursor a bit before
the chosen option in the text stream, and choosing 'Hunt' from the
cell menu.

ASIDE:  Ampersands, '&', are used in the web called 'Cave' to indicate
links to other cells.  This is not necessary for the functioning of
the program, but it does make it easier to get around.

You will find yourself in another cell, having the name of the choice
that you made.  Depending upon what you chose, you may be presented
with more choices.  Explore the cave for a while, always positioning
the cursor before your choice, and using 'Hunt' from the cell menu, to
move to your chosen destination.

When you have finished, move on to the next section, which discusses
the many ways to browse the contents of a web.

====Browsing the Web
Press the menu button and you will see in the menu bar an entry called
'Browse'.  Select that entry and the four choices 'Prev', 'Next',
'Overview', and 'Trail' are displayed with their accelerators PSION+,
PSION-, PSION/, and PSION*.  Use the '+' and '-' accelerators to
browse the Cave web, returning to the point from which you began.

NOTE:  To use an accelerator, hold down the PSION key while you press
the accelerator key.  You will notice that the cells in the web
appear in alphabetic order when browsed, and that browsing forward
from the last cell in sequence returns you to the first cell in
sequence.  Choose the '/' accelerator, and a window will open in the
middle of the screen, which may be scrolled to show the names of all
of the cells of the web in alphabetical order.  If you want to go to a
particular cell shown in the browser, position the highlight to that
cell's name and press ENTER.  To exit the browser without changing
cells, press ESC.

If the name of the cell you want is not in the window, you may scroll
to it with the up and down arrow keys, or type the first letter of
the cell name, which will cause the browser to go to the portion of
the overview which contains cells beginning with the letter typed.

Choosing the '*' accelerator activates the trail browser, the use of
which is described in the section called 'Linking to a Cell'.

====Changing the Current Cell
Aside from browsing, there are three methods of moving from one cell
to another.  These methods are:  'Linking' to a cell, 'Going' to a
cell, and following a hypertext link.  The next three short paragraphs
describe how to perform each operation.

====|====Linking to a Cell
When you 'link' to a cell, you transfer to that cell with the
expectation of returning to the current cell.  To try this, choose
'Linkto' from the Cell menu, and enter the cell name 'Noway' into the
dialog box that appears.  When you press enter, the screen should
show the 'Noway' cell.  Return to the cell you left by pressing Esc,
or choosing 'Unlink' from the Cell menu.

SpyderT keeps a trail of cells to which return is expected, so that
you can return from many levels of linking by repeatedly pressing
ESC.  You may view the return trail by activating the trail browser in
the browse menu.  From that browser, you may link to a cell on the
trail, by positioning the underline cursor to the cell, and pressing
ENTER.

====|====Going to a Cell
When you 'go' to a cell, you transfer to that cell with no expectation
of returning to the current cell.  To try this, choose 'Goto' from
the Cell menu, and enter the cell name 'Fossil' into the dialog box
that appears.  When you press enter, the screen should show the
'Fossil' cell.  You will have to 'browse' your way back to the cell
you left, since it was not added to your trail.

====|====Hypertext Links
Whenever the name of another cell appears in the text pane of the
current cell, you may use that name as a hypertext link to the other
cell.  In the 'Cave' web, hypertext links to other cells are marked by
preceding them with ampersands (&).  This convention is unnecessary
for the functioning of the links, but makes it easier to navigate the
web.

To follow a hypertext link, position the cursor in the text stream a
bit before the link, and select 'Hunt' from the Cell menu.  SpyderT
will scan forward until it finds a valid link, and then transfer to
the cell named in the link.  The destination cell will replace the
one currently shown.  To return to the previous cell, press the Esc
key, or select 'Unlink' from the Cell menu.

NOTE:  Whenever the name of a cell is typed into an edit pane, it
automatically becomes a hypertext link to that cell. 

NOTE:  When you select a cell name in the overview browser or the
trail browser, and press enter, the named cell is 'linked' to.  You
may return to the cell from which you left by pressing ESC.

====Searching the Web
In addition to following chains of links which you type into the text
field of the web, you can use the 'Find' function in the Edit menu to
go to the next cell having a specified text string, or to the closest
previous cell having a specified text string.  Your search may be
case sensitive or not.  You may set default search preferences by
obtaining the Preferences dialog (choose Preferences from the Special
menu), and then choosing 'Searching' from that dialog.

Try this now by using the 'Find' function on the Edit menu to search
for the word 'treasure' in the Cave web.

====Creating A New Web
Our goal for this section is to create a new, empty web, to add cells
to it, and to create links between the cells.  To this end we will
make a web which lists the names and addresses of several people we
know, and also adds a bit of useful information not usually found in
such a list.

====Making An Empty Web
Position to the SpyderT icon in the system menu.  Hold down the PSION
key, and press the 'N' key.  A dialog box will appear asking you for
the name of a new SpyderT file.  Type in the filename 'Friends' and 
press 'Enter'.  SpyderT will respond by showing you the home cell of a
web named 'Friends'.  Browsing forward or backward, you will see that
home is the only cell in the web.

====Adding Cells
A new cell may be added into our web by using the 'Add', 'Linkto', or
'Goto' commands in the Cell menu.  For now choose 'Add', and answer
the ensuing dialog with the name of someone with whom you are 
friendly or acquainted.

If the "Bad Cell Name" dialog appears, it is probably because you left
a space between the person's first and last name.  Cell names may not
have spaces in them; but it is OK to use the underscore character in
place of the space you would normally use.  (Some software types like
to jam words up together and indicate separate words by the use of
capitalization, as in WhatsInAName).  If you repair this defect, and
still get the "Bad Cell Name" dialog, it may be because your friend's
name begins with a number, such as '3Cpo', or a special character, as
in '#Sterling'.  All cell names must begin with an alphabetic 
character.

Once the cell has been added, you may type in whatever information you
wish about this person, including, for example, their address, phone
number, height, weight, probable number of outstanding parking
tickets; and so on.  Keep in mind that you will be able to use the
'Find' command in the Edit menu to search your web on the information
which you have entered.

Repeat the process of adding cells with names of acquaintences until
you have entered several persons into the little database.  Next we
will enter connections between the people in the web.

====Connecting the Cells
Goto, Linkto, or Browse to the cell for the first person you entered,
and enter the text field for that person.  Scroll down to the bottom
of the text field, start a new line, and enter the words:

      TALKS TO:

Then enter the names of persons with whom you know that this person
communicates on a daily basis.  If the person involved is already in
your database, make sure you spell the name the same way as it appears
in the cell name.  If the person involved is not already in your
database, you may want to considering adding the person.

Go through the cells of the database, adding the links wherever you
know them to exist, and adding persons where it seems appropriate. 
When you finish, you will not only have a list of acquaintances and 
whatever information you wish to keep about them; you will also have a
map of information flow among your associates.

By setting the report preferences to InPointers or OutPointers and
choosing Report from the special menu you can see a report on this
information flow.

====Saving the Web
To save the database you have made, choose 'Save As' from the File
menu.  A dialog box will appear.  If the name 'Friends' is
appropriate for the people you entered into your web, you may just
press Enter to save the file under that name.  If some other file
name, like 'People' would be more appropriate, enter that name into
the file name dialog.   You may include, or not include the suffix
'web', as you wish.  If you use another suffix, the web will be saved
with that suffix.  '.web' is the default filename suffix for SpyderT 
files.

SPYDERT OUTPUT
====Purpose of SpyderT's Output
SpyderT is a personal, portable scratchpad for thinking about complex
phenomenon, systems, and relationships.  It is not intended to
communicate the results of that thought to others.  SpyderT does not 
generate elaborate reports.  Instead it provides flexible, primitive
tools to format output and results in a way that can be understood by
the user, and transferred to other programs or computers for further 
analysis.

====Report Output
The SpyderT 'Report' entry, in the Special menu, permits the user to
select some or all of the cell name and text information in some or
all of the cells; and format it into simple ASCII text streams which
are conveyed to the current output destination of the program.  This
feature permits the user to create indexes of webs, list contents of
the cells in a web, dump web contents to files for upload to a host
computer, and to generate so-called "comma-separated text" files for
exporting SpyderT data to databases and other programs.  Also
available are cross-reference reports called "InPointers" and
"Outpointers", which assist in understanding the structure of a web;
and the report called "Trail", which may be used to record your 
current hierarchic position resulting from a sequence of "link" or
"hunt" requests.

====Choosing Report Output Destinations
Output of reports may be directed to the screen, to a cell within the
web, or to a file in the Psion's memory.  The latter option is
available only after SpyderT has been registered. To set the output
destination, select the 'Preferences' entry in the Special menu.  The
following dialog menu will appear:   

     Preferences for:
     * Output
     * Searching
     * Report

Choose the 'Output' choice to reach the Output Preferences dialog:

      Output Preferences
      * Destination    <-Cell->
      * Dest Name      Xanadu
      * Newline Type    CRLF
 

When output is directed to the screen, it is displayed one screen at a
time, with a prompt required before the next screen is displayed.  

When output is directed to a file or to a cell, the name of the file
or cell is entered by typing it into the Dest Name field of the
dialog. 

When output is directed to a file, the type of newline generated can
be controlled by the third entry in the Output Preferences dialog. If
the output will be exported to an IBM compatible computer, it is a
good idea to use the 'CRLF' option for newline.  If the output will
remain on the Psion, or will be exported to a Macintosh or Unix
computer, the 'LF' option works better.

TIP:  By choosing 'Cell' for the destination, and by carefully setting
the report format, the user can generate a handy index of the web
within the cell named in the dialog box.  The index will be
particularly useful since all cell names output by the report are
automatically hypertext links to the cells which they represent.

====Specifying Report Output
To specify what information will appear in Report output; select the
Special menu, and choose the 'Preferences' entry. The Preferences
dialog (shown above) will appear.  Select 'Report' from that dialog, 
and the Report Preferences dialog will appear:

      Report Preferences
      * Report on All Cells    <-Yes->
      * Report Type           Formatted
      * Report Format         \n\N\n-----\n\T 

The first question in the dialog tells SpyderT whether or not to
report on all cells.  The second tells it what kind of report to
generate.  The third tells it which information to include for each
cell in a formatted report, if that is the report type that has been
chosen..

====|====Report Cell Selection
Answering Yes to the  "Report on All Cells?" question will assure that
when the user elects to generate a report, all of the cells in the
web will be reported, according to the report type and format
specified.  Answering No will cause the report function to select a
subset of the cells in the web.  The subset of cells selected for
reporting depends upon an algorithm executed by the report function. 
If there is a highlighted text selection in the active edit box at
the time a report is requested, SpyderT will scan that selection for
valid cell names, and generate a report on all the cells named in that
selection, in the order in which they appear.  If there is no
highlighted selection in the active edit box, SpyderT will generate a
report on the current cell only.

====|====Report Type
There are four types of reports.  They are the Formatted report, the
InPointers report, the OutPointers report, and the Trail report. 

====|====|====The Formatted Report
The appearance of a formatted report depends upon the content of the
Report Format string.  If nothing is entered in the report format
string in the Report Preferences, no output will be generated when
Report is chosen from the Special menu.

If the report format string is not empty, then choosing Report from
the Special menu will cause that string to be scanned and interpreted
for each cell in the selected cell list.  When the report format
string is interpreted for a cell, most characters in the string are
copied directly to the output destination.  Substitutions are made,
however; for character pairs which begin with the backslash (\).  The
substituted text is copied to the output in the place of those
character pairs.  Character pairs for which substitutions are made
during interpretation of the report format string are:

'\n'    A newline sequence is copied to the output in the place of a
backslash followed by a lower case n.

'\N'    When upper case N appears in the report format after the
backslash character, the report generator substitutes for the pair of
characters the name of the cell for which the format string is being
interpreted.

'\T'    Backslash-T results in the outputting of the text field of the
cell being reported.

Any other two character sequence beginning with a backslash will cause
the second character only to be output to the report.  This includes
the string '\\' which is replaced in the output by a single
backslash.  If the report format string is not empty, but neither
does it contain any of the character sequences above, it will be
copied verbatim to the output once for each cell which has been
selected.

====|====|====Inpointers Report
For each cell in the report cell selection, this report lists all the
other cells in the web which refer to it. 

====|====|====Outpointers Report
For each cell in the report cell selection, this report lists all the
other cells to which the selected cell referss.
 
====|====|====Trail Report
The Trail Report lists from most recent to most ancient, all of the
cells placed on the Trail by Linkto requests, that have not yet been
popped off by Unlink requests.

====Sample Reports
                             Quotes
The web called Quotes has a report fomat string that can be used to
copy the entire contents of the web, into a single stream, which may
be saved into a file for upload, or scanned as it goes by on the LCD 
screen.  The format string reads:

	"\n\T\n                   --\N\n"

Here is a selection of the output:

     My work is being destroyed almost as soon as it is printed.  
     One day it is being read; the next day someone's wrapping 
     fish in it.
                         --Al_Capp

     If we had no faults, we should not take so much pleasure in 
     noticing them in others.
                         --Duc_De_La_Rochefoucauld

     A foolish consistency is the hobgoblin of little minds...
                         --Emerson

     You never can tell..
                         --GeorgeBernardShaw

     He who knows does not speak.
     He who speaks does not know.
                         --Lao_Tzu

                        Cave
This web is made to be explored as an amusement.  Its report is set to
an OutPointers report.  When Report is chosen from the special menu,
the following helpful text is generated:

AttackTheGnome
ClimbTheRockChimney
DiveIntoTheTorrent
    >home
EnterTrapDoor
    >TakeTorchIntoPassageway
    >GoBackToShack
Fossil
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

This report presents us with a synopsis of the choices available at
each cell of the web called 'Cave'.  One would be wise to print the
report before entering the cave.

MENUS AND PREFERENCES
====SpyderT Menus
There are five SpyderT menus, called File, Edit, Cell, Browse, and
Special.  The dialogs and functions invoked from these menus are
described below:

====|====File Menu
The File menu has three entries: New, Open, and SaveAs:
     *	New saves the contents of the current web, and creates a new,
        empty web with the name NEW.

     *	Open displays a dialog box into which the name of the file to 
        be opened can be entered.  If the desired file is not in the
        \web\ directory, press TAB to enter the extended file open
        dialog.

     *	SaveAs displays a dialog box into which the name is entered
        under which the current web will be saved.  To save into a
        directory other than the current one, press TAB to enter
        the extended file dialog. Once the target directory is
        located, press return to re-enter the dialog box for the
        file name.

====|====Edit Menu
The entries in the Edit menu are Copy, Insert, Bring, and Find:
     *	Copy saves the current selection in the edit box to the
        clipboard. (Make a selection by holding down the shift key
        and moving the cursor to cover the text to be selected).

     *	Insert pastes the contents of the clipboard into the current
        cursor position.  There is no Delete in the edit menu.  To
        delete the current selection in the edit window, press the
        Delete key.  The selected text will be removed from the edit
        window and saved in the clipboard.

     *	Bring checks to see if any other application capable of
        performing the link-paste server function has a selected
        block of text.  If so, the most recent such selection is
        copied into the current cell at the cursor location.  It is 
        also pasted into the clipboard for future Insert operations.

     *	Find searches forward or backward for the next occurrence of
        the currently selected text.  If no text is selected, a dialog
        box appears, which allows the entry of the text to search for,
        as well as the preferred direction of search.

====|====Cell Menu
The entries in the cell menu are Add, Delete, Goto, Linkto, Hunt, and
Unlink:

     *	Add asks for the name of the cell to add.  Press Esc to cancel
        the request. Otherwise, enter the new cell name and press enter.
        If a cell with the new name already exists, a dialog will
        inform you of the fact, and ask if you wish to see that cell.
        Otherwise, the new, empty cell will be displayed.

     *	Delete causes the current cell and its contents to be deleted,
        unless it is the home cell.  The home cell cannot be deleted.

     *	Goto transfers to a different cell, without saving the current
        cell into the trail.  If there is a cell name selected in the
        current edit window, that is the cell to which a transfer is
        made.  If not, a dialog appears, asking for the name of the
        cell to go to.  If there is no cell with the requested name,
        a new cell with that name is created.

     *	Linkto saves the current cell into the trail and  transfers
        to a different cell. If there is a cell name selected in the
        current edit window, that is the cell to which a transfer is
        made.  If not, a dialog appears, asking for the name of the
        cell to go to.  If there is no cell with the name requested,
        a new cell with that name is created.

     *	Hunt scans from the current cursor position forward, looking
        for a cell name. If it finds one, it saves the current cell in
        the trail, and transfers to the cell whose name was found.

     *	Unlink returns to the cell most recently added to the trail,
        and removes that entry from the trail.  If there is no cell
        on the trail, Unlink goes to the home cell.

====|====Browse Menu
The cells in a web can be browsed in alphabetical sequence or reverse
alphabetic sequence by choosing Prev or Next from the Browse menu.
By choosing Overview, one may see a window containing the names of all
the cells in the web.  Position to a particular cell by using the up
or down arrows, or by typing the first character in the cell name.  
Pressing Enter causes a 'linkto' to the highlighted cell.  Pressing
Shift-Enter causes a 'goto' to the highlighted cell.  Pressing Esc
causes an exit from the browser to the current cell.  Choose Trail to
see the cells currently on the trail as a result of previous linkto or
hunt requests.  Pressing Enter causes a 'linkto' to the underlined
cell.  Pressing Shift-Enter causes a 'goto' to the underlined cell.  
Pressing Esc causes an exit from the browser to the current cell.

====|====Special Menu
The entries in the Special menu are About, Report, Preferences, Zoom,
and Exit.

     *	About displays information about SpyderT.

     *	Report causes a report of the contents of the cells to be
        issued.  The destination of the report output, and the content
        and format of the report are controlled by the Options entry
        of the Special menu.

     *	Preferences provides the means to control Output destination,
        Search direction, and Report format.

     * 	ZoomIn increases the current font size.

     *  ZoomOut decreases the current font size.

     *	Exit causes the SpyderT program to stop executing and
        returns to the system screen.

====SpyderT Preferences
The 'Preferences' entry in the Special menu opens a dialog box that
gives the user the opportunity to set three different classes of
parameters to control the operation of the program.  The three
categories are: Output, Searching, and Report.

====|====Output Preferences
Output preferences govern the destination; and to some extent, the
format of reports.  The three output options are: Destination,
Destination Name, and Newline Type.

====|====|====Destination
Output destination can be set to Screen, Cell, or File.

     *  If the Destination is set to Screen, output from reports
        appears one screen at a time on the LCD. After each screen
        the user is prompted to enter ESC to abort the report, or
        any other key to continue it.

     *	If the Destination is set to Cell, output from the report
        is copied to the end of the text area of the cell specified
        in the Destination Name option.  Since the capacity of the
        text area of a cell is limited to 2048 bytes, the text area 
        only retains the last 2048 bytes of any output sent to it.

     *	When the destination is set to File, the output from reports
        is sent to the file named in Destination name.  This option
        works only after the program has been registered.

====|====|====Destination Name
The Destination Name option provides a place for the user to enter the
cell or file name into which output from reports is directed.  If the
Destination name option is empty, or does not contain a valid cell
name when the Cell or File destination is selected, output is
directed to a cell or file called OUTPUT (or OUTPUT.TXT).  If there
is no cell (or file) called OUTPUT, one is created.

====|====|====NEWLINE TYPE
The third Output Option is the Newline Type Option, which applies only
when the output destination is File.  The option lets the user choose
between newlines composed of linefeeds, or newlines composed of 
carriage returns, followed by linefeeds.  The former option is
preferred when the files will not be exported to other computers. The
latter option is preferable if the files will be exported to a
PC-compatible computer.

====|====Search Preferences
When the user selects Find from the Edit menu, a search is performed
for selected text, or text specified in a dialog box.  The Search
preferences apply to the operation of the Find Request.  The available
options are:  Search For, Search Direction, and Case Sensitive?

====|====|====Search For
This is the string for which a search is made when the Find choice is
selected from the Edit menu.  No wild card characters or regular
expression specifiers are accepted here.  Enter just the character
string for which you are looking.

====|====|====Search Direction
The choices are Forward and Backward.  When a direction is chosen, the
next search (Find) operation proceeds from the current cursor
position in the current field of the current cell, in the direction
indicated.  If a match is not found in the current cell, the search
continues into the next cell in the direction indicated.  The search
ends when either a match is found, or the search returns in a giant
circle to the beginning or end of the field in the cell in which it
began.  KNOWN BUG:  When searching backward, an instance of the
target which is the last string in the text field of a cell may be
missed.  This problem will be repaired in a future release.

====|====|====CASE SENSITIVE?
If -Yes- is selected for this option, a match is declared only when
text matches as to characters and case of each character.  Otherwise,
a character match alone is sufficient.

====|====Report Preferences
Report Preferences are Report On All Cells?, Report Type, and
ReportFormat:

====|====|====Report On All Cells?
This option allows the user to choose whether to apply the report
function to all cells in the web, or to a selection of cells.  See
the section entitled "Specifying Report Output" for a complete
discussion of this preference and its application.

====|====|====Report Type
This option allows the user to choose between four different types of
reports:  Formatted, InPointers, OutPointers, and Trail reports. 

====|====|====Report Format
The Report Format string allows the user to specify the contents of
the report generated for each selected cell.  The rules governing the
syntax and interpretation of the report format string are discussed in
the section entitled "Specifying Report Output".

SPYDERT ERRORS AND LIMITS

====Error Conditions

====|====Out of Memory Condition
When SpyderT writes a web to a disk, it writes it first as a temporary
file, called temp.tmp, and then renames the temporary file to the
name of the web.  If a memory shortage prevents the entire web from
being written, the update is not completed, and the old version of
the web remains on the disk.

====Program Limits
The following limits apply to version 1.1 of SpyderT:
Maximum Size of a Cell Name:			48	
Maximum Characters in a Text Field:		2047
Maximum Number of Cells:		limited by 64K
					program data
					segment size.
  

WARRANTY, LICENSE, REGISTRATION, AND DISTRIBUTION


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
 
BY USING THE SHAREWARE VERSION OF SPYDERT YOU ACKNOWLEDGE THAT YOU HAVE
READ THIS LIMITED WARRANTY, UNDERSTAND IT, AND AGREE TO BE BOUND BY ITS'
TERMS AND CONDITIONS. YOU ALSO AGREE THAT THE LIMITED WARRANTY IS THE
COMPLETE AND EXCLUSIVE STATEMENT OF AGREEMENT BETWEEN THE PARTIES AND
SUPERSEDES ALL PROPOSALS OR PRIOR AGREEMENTS, ORAL OR WRITTEN, AND ANY
OTHER COMMUNICATIONS BETWEEN THE PARTIES RELATING TO THE SUBJECT MATTER
OF THE LIMITED WARRANTY.

                             LICENSE

You may use the shareware version of SpyderT for a 30 day trial period.
If you would like to continue to use SpyderT after the 30 day trial
period, you are required to register as a user of SpyderT, and pay the
registration fee.  If you choose not to register or pay, you must 
remove SpyderT from your computer.

You may distribute evaluation copies of SpyderT to other persons or
organizations, provided such distribution takes place in accordance
with the provisions of the DISTRIBUTION paragraphs below.


                            REGISTRATION

The registration fee for SpyderT is $19.  When you register, you
will receive in the mail a key to disable registration reminders,
and enable report output to files.

You can register SpyderT using the Compuserve shareware registration
service, or by sending cash to the author.  For details on ordering
and payment, see below:

                       ORDERING AND PAYMENT

METHOD 1:  Compuserve shareware registration service

Log onto Compuserve and go to shareware registration under the 
computers menu (GO SWREG).  Access the registration database with
the program name, SpyderT, or the registration number ????.

METHOD 2:  By credit card through North Star Solution's
           Shareware Registration Service

You can order registration packets with Visa or MasterCard credit cards
from North Star Solutions shareware registration service.  Simply contact
North Star Solutions via any of the following methods:
Voice: 1-800-699-6395(10:00am-10:00pm,EST. Calls from US only.)
       1-800-699-6395(10:00am-10:00pm, Eastern Standard Time)
FAX: 1-803-699-5465(Available 24 hours.
    (International and Business Orders encouraged)
E-mail: CIS   71561,2751    Internet 71561.2751@compuserve.com
    (International and Business Orders encouraged)
Please have the following information ready:
 *  The program and version number ( SpyderT v1.4 )
 *  Where the registration packet should be mailed
 *  Your VISA or MasterCard number and expiration date
 *  Whether you want a MAC or a PC diskette (3.5 inch drives only)

IMPORTANT NOTES:
1)  North Star processes registrations only, please contact the
author at 71270.1441@compuserve.com or eieio@boulder.earthnet.net for any
product information or technical support.
2)  E-mailed and FAXed registrations are encouraged, but all registrations
are very much appreciated!


METHOD 3:  By sending cash to the author.

Send your mailing address, and $19 to the address below:
    EIEIO
    P.O. Box 1086
    Niwot, CO 80544
    U.S.A.
Send cash in the form of U.S. dollars, a check drawn on a U.S. bank
(MICR code on the bottom), or International Money Orders for $19 (U.S.).
Add a 25% premium over the exchange rate if you send Canadian checks
in Canadian dollars, or major currencies (Pounds, Marks, Francs, or Yen).
 

                             DISTRIBUTION

You are encouraged to pass a copy of the shareware version of SpyderT
along to your friends for evaluation.  If you do so, you must provide
them with the entire set of SpyderT shareware version files. These include:

    README.TXT
    SPYDERT.APP
    VENDOR.DOC
    FILE_ID.DIZ
    CAVE.WEB
    GRAMMAR.WEB
    QUOTES.WEB
    WEBSITES.WEB
