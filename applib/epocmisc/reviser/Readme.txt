Reviser v5.22 for the Psion Series 5

Phil Whiles4 Ivy CottagesBoyatt LaneOtterbourneWinchesterHants SO21 2HT

philwhiles@poboxes.com
http://www.geocities.com/SiliconValley/Lakes/3216/

Contents
	What is Reviser ?
	History
	Registration
	Installation
	Getting started
	Notes to users upgrading from Reviser 3x
	Warranty

What is Reviser ?
Reviser is a general purpose revision aid, allowing you to collate sets of questions and answers on any subject, along with multiple test criteria.
Basically (using the main menu from left to right) :
• You create a Reviser file (File)
• You add some Data to it (Data)
• You define a Test (Test)
• You run the Test on the Data (Start)

When you Start a Test, Reviser  picks out a specific subset of questions and answers from the larger data set.
The test is presented as a random selection of this subset, presented in  a flash card format. ie a Question card followed by an Answer card.
Using the marking options provided you can then mark the questions you are confident with, allowing you to concentrate on those you are less familiar with.
You know .... Revision !
This Readme is only intended as a gentle introduction to Reviser to get you installed and started. For further reading, please see the online Help, the format of which is informal and is intended to describe the concept of Reviser, rather than a terse menu option guide.
Hopefully, understanding the concepts will make the menus and commands in Reviser self explanatory.

History
v5.0	0	13/05/98		First release for the Series 5
v5.10	26/05/98		added pen use in List view					fixed Start-delete bug where only every other rec was deleted.						fixed bug in new test where prev test got new tests includes					dont allow blank test names anymore
5.20		10/06/98		added toolbar off					added D drive support !!					fixed edit menu option within quiz					quiz screen title cosmetic change					added confirmation of quiz marking when selected from menu.					pen click in quiz screen added (shortcut to Next)					marking summary cosmetic changes					redisplay after edit in quiz
5.21		18/06/98		pen click in quiz screen added !					Can now register online.
5.22		22/06/98		fixed D drive support !					added reviser.aif !
										
Registration
Reviser is shareware. If you find it useful, and wish to continue using it, please register. For the small fee requested, you receive a personal password which once entered removes the annoying nag screens.
In registering, you encourage the development of even more quality software for the Psion !
If you provide an email address with your registration I shall also send you notification of changes and planned upgrades.
To register send  £10 as either :
cash
UK cheque
Eurocheque
-  to the address at the top of this text. If you send cash, please put it between two sheets of stiff paper, to disguise it in the envelope !
Or ...
RegNet #2321   This program can be registered through RegNet - The Registration Network.Register online on the World Wide Web at the following URL: http://www.swregnet.com/2321p.htmor by calling 1 800 WWW2REG (1 800 999-2734) or (805) 288-1827.

Installation
Unzip the distribution file and install as follows :
create a directory "C:\System\Apps\Reviser"
install the following files to that directory :		Reviser.app		Reviser.hlp		Reviser.aif		Reviser.mbm		Toolbar.mbm
if you wish to run Reviser with the demonstration files, install any the files (Geography /French/Spanish/German)  to a directory of your choice. (C:\Reviser makes sense !).
you can now start Reviser by double clicking on  the demo file in the system screen, by launching Reviser from the Extras bar (in which case a file called Reviser will be created in the root of your default disk), or by using the System screen menu option "File - Create New - File" and choosing Reviser.
Getting Started
When you start Reviser you should see it's main screen showing a backdrop of Reviser icons, a status line and a toolbar. If you have started Reviser from the Extras bar, or from "File - Create New - File", you will be prompted for :
a default test name
the label for your data field X
the label for your data field Y
Each reviser file must have at least one test definition within it, hence the default. The default test is loaded each time the Reviser file is opened.
The labels are used to refer to your data's X and Y fields ie "English" and "French". ie in the Start menu, when adding data and in tests.
You will now need to add some data ! Read the online help first, and before leaping into adding data, give some thought to the numbering scheme you are going to adopt.
If you have started Reviser with one of the demo files you should use the Browse feature to examine the data used, look at the Key to see the numbering scheme used, open a test , note the includes used, then Start the test.
Notes to users upgrading from Reviser 3x
Reviser for the Series 5 is a significant  redesign of the Series 3x version of Reviser.
The most significant changes to be aware of if upgrading from the Series 3x version are :
The 5 version uses the new dbms SQL model, and consequently combines the data entries and the test definitions into one file.
Because of a limitation in the Series 5 handling of database files, the Data file created by the Series 5 Data program cannot be written to by a program such as Reviser. Hence Reviser now has a private file, that cannot be examined by the inbuilt Data app.
The Series 3 version of Reviser used a string field to represent the Lesson and Group fields so that the Series 3 Data app. could share the Reviser data file. As this sharing is no longer possible on the 5, the lesson and group fields are now represented in the data entries as integers. The upshot of this is that you no longer have to consider the consequences of string "2" being greater than "100" when considering the numbering scheme to use.

Warranty
You get absolutely none what so ever !
The author holds no responsibility for this software, and you use it at your own risk.
(I'll take credit for Grade A exam results though :-)