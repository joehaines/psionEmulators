ReadMe.txt for Roller version 0.8 pre-release Alpha
===================================================

0. Introduction
---------------

Roller is a dice rolling program, with a difference, this one is actually useful
and it is the first of it's kind (that I know of) for the Psion, thanks to Jody
Armstrong for pointing out this omission and setting me on the course of writing
what is turning into one of the most complex programs I've written on the Psion.

So Roller rolls dice, but it does more than that, it stores upto 40 dice types,
and 40 rolls, all accessable from the main screen, it also allows you to check
against values and either report success or failure, or add extra rolls to those,
it even allows you to check one roll against another.

1. Installation
---------------

Roller comes as a .SIS file, which is Psion's standard file format for installation
if you have EPOC Install on your PC simply double click on the .SIS file and follow
the onscreen prompts, if you don't have a PC but have installed Message Suite then
you can simply copy Roller.SIS to your Psion and open it as normal, this will install
the file directly on your Psion. (Currently not available, manual install only at present)

2. Running
----------

Once installed you will have a new icon on the Extras Bar called Roller, with a nice
picture of a Die, simply click on this to run Roller, after a few seconds you will
be presented with the main Roller Screen, with a Toolbar and one Icon on the desktop.

* * * * * QUICK START * * * * * *

Press Enter or tap the Icon, you've just rolled an AD&D Stat (complete with extra
percentile if it's 18).

Press menu or tap the side menu button, select Plugin|Magic|Start, select how many lives
you want, tap the buttons on the window to add or subtract lives, flip a coin, or exit.

Press menu or tap the side menu button, select Plugin|Generic Roll|Roll, select number of dice, dice type and number to add/subtract and tap okay to roll.

* * * * END QUICK START * * * * *

3. Using Roller
---------------

Roller uses the standard EPOC32 interface, with a toolbar on the right, with icons
and a clock, menus can be called up using either the side button or the menu key,
the program responds to the correct EPOC32 events and will shutdown properly if 
asked.

3.1 Menus
---------

The menus allow you to access every function of the program the menus are

	File
	----
	Open Data - Will Load a set of Dice and Rolls into
		    the program, at present only the default set is available.
	Save Data - Will Save the current data set, creating
		    three files, <filename> which can be clicked to reload
		    roller and load in the other two files <filename>.die and
		    <filename>.rolls
	Save As   - Will Save the current data set as to new
		    filename.
	Exit	  - Closes Roller, and saves the current data to the correct
		    set, when you load next time you will continue using this
		    set of files.

	Dice
	----
	Add Die	  - Adds a die definition to the files, see the section on
		    Dice for more information.
	Edit Die  - Not Implemented, Allows you to select a Die and then change
		    the information about .

	Rolls
	-----
	Roll Dice   - Performs the same as tapping an Icon or clicking on Roll Dice
		      on the Toolbar.
	Add Roll    - Creates a new roll, see the section on Rolls for more information
		      on creating and editing rolls.
	Edit Roll   - Edits the currently selected roll (the inverted one), see the
		      section on Rolls for more information.
	Delete Roll - Not Implemented, This will delete the currently selected roll,
		      after a comfirmatory dialog.
	View Roll   - Shows the whole Roll string (3D6=18 1D100 for AD&D Stat), since
		      the backdrop view will clip this if it's too long.


	Plugin
	------
	Magic	    - Name of the plugin module pre-installed.
	Generic Roll- Creates a generic roll of nDn + n quickly
	Config	    - Allows the adding and removing of plugin modules.

	Window		(only available if plugin windows are open)
	------
	Hide	    - Allows the user to hide any of the open windows
	Show	    - Brings any windows hidden back into view again
	Close	    - Allows the user to close any of the open windows, even if
		      they are hidden.

	Tools
	-----
	Preferences    - Allows you to set preferences for the program, will allow you to
		         choose which confirmatory messages you will recieve and if the
		         backdrop shows Roll String.
	Register       - Unregistered version only, allows the user to register the program
			 removing nag screens and allowing certain features to work.
	Help on Roller - Displays the Roller help file, which will explain in detail about
			 the various parts of the program, and the concepts used, essential
			 reading (or will be when it's written)
	About	       - Displays the DudleySoft (possibly PalmScape) Splashscreen.

3.2 Dice
--------

	In the other dice rolling programs i've seen you can enter any number for sides,
	now while this is flexible it's hardly correct, you can't get 23 sided dice (23
	was chosen since it's a prime, and you can't make a regular 23 sided polyhedron.
	You could choose to create a D23 as a new dice in this program, but you have to
	decide to do it. The program starts with a series of Dice already defined, these
	are D4,D6,D8,D10,D12,D20, D100 (Percentile) and D66 (Percentile, well sort of).
	There is also a special Dice called Number which allows you to use constant
	values in your rolls (as modifiers normally), I'm also concidering adding a 
	Variable type that asks for a value at runtime. To create a new Dice simply
	select Add Dice and you will be presented with a dialog box.
	
		Name		- Name for your dice, if you leave it blank it will
				  generate the name as D<n> where n is the number of
				  sides selected.
		Type		- Currently Allows two types Die and Percentile.
		Sides 1		- If your using a Die type this is the sides on the
				  die and you can ignore the other two values, on
				  percentiles, this is the sides on the first die.
		Sides 2		- Ignore if your using a Die type, on a percentile
				  this represents the number of sides on the second
				  die (see you don't get this flexability if you
				  use inferior programs that roll a D100 as 1-100)
		Flags		- Ignore if your using a Die type, on a percentile
				  this controls how values are generated.
				  0 	- Values go from 0 to <sides>-1, 00 will
					  become 100 automatically.
				  1	- Values go form 1 to <sides>
				  2	- Values go from 0 to <sides>-1, 00 stays
					  as 0.

3.3 Rolls
---------

	Now comes the complicated part, when we want to roll a die at present we
	need to create a Roll, this is a multi part operation, but you can go
	back and change values at any point, and the dialog shows the current
	roll status as part of it's display. At some point in the future I will
	add a quick roll feature, that will mimic lesser dice rolling programs.

	Adding and Editing Rolls use the same routines, and are identical except
	when you edit a roll the program sticks you at the last die entered and
	a new roll starts at the first die.

3.3.1 Roll Editing Dialog
-------------------------

	This dialog is the core of the program and provides the amazing flexability
	that you wouldn't get with a simpler program.

		Operation	- This is where the power really comes from, any fool
				  can write a program to generate a few random numbers
				  but with this system you can link them together to
				  form complex equations, the operation types are as
				  follows:
				
			New	- Starts a new roll value, if there is a previous value
				  it will be output, nearly all rolls will start with
				  new, or + or - (the final version will only allow these
				  three values to be selected here)
			Add	- Adds this result to the previous result, speaks for
				  itself so I'll shut up.
			Subtract- Subtracts this result from the last result, see comment
				  for Add.
			Multiply- Multiplies the last result by this one.
			Divide  - Divides the last result by this one.
			Less    - Compares the last result with this one if this result is 
				  less than the last one then one of two things happen, if
				  the Diagnose flag is set then A result is output saying
				  we failed and telling us the result of this value, if not
				  then we stop the roll. if its greater then we either
				  output a success message and continue or just continue.
			Greater - Works the same as Less except the other way around.
			Equals  - Works the same as Less and Greater except it only succeeds
				  if the results are the same (used to great effect in the
				  AD&D Stat roll)

		Type		- The type of die that you want to roll, you can select
				  from any of the already selected die types here, a list
				  pops up if you tap on the box when it's selected.
		Number		- For a Die or percentile type this is how many of them
				  you want to roll, for a number type this is the actual
				  number you want to use.
		Finish (Button) - This takes you to the finish dialog, Note that every
				  Die after the current one will be lost if you select this
				  in the wrong place, check before you finish that you are
				  at the last die entry.
		More > (Button) - This allows you to enter more dice, it moves you forward
				  one die in the list, you can go forward upto Die 8.
		Back < (Button) - This takes you back one space, all the values after this
				  are remembered, so you don't have to re-enter them
				  if you've made a mistake further back.
		Cancel          - Cancels the current operation, the roll wont be written
				  back until you actually finish the roll.

3.3.2 Roll Finish Dialog
------------------------

	Bet you thought you'd finished after that, well you've still got to name your
	Roll and set a couple of flags, this dialog is simpler than the pervious one,
	Note that at this point you can't go back to the Edit Dialogs (I may add this
	at a later date), below are the fields in this dialog:

		Roll		- This simply tells you the roll that you've created
				  in a standard format.
		Allow Negative  - If this is checked then the program will allow values
				  to go negative, if not then they will automatically
				  be set to zero should they fall below.
		Diagnose Checks - Doesn't check for Bank account, this flag says that
				  when we use a check function we should inform the
				  user if the check has Succeeded or Failed, if this
				  is set you will see something like this as a result
				  14 Failed (18) the first value is the value rolled,
				  the second value is the value it was checked against
				  since this can be a dice roll, simply telling the
				  user that he has failed without confirming it would
				  seem a little unfair, this can be used for Saving 
				  throws to give a visual indication of the status. (NB
				  if you use the Enter Value type I'm planning on adding
				  you won't be told the result when you have to enter a
				  value, so you can't cheat the result)
		Okay   (Button) - Saves the roll using the information entered, either as
				  a new roll (if you chose that option) or over the one
				  you were editing.
		Cancel (Button) - This cancels the editing, no changes will be made.

4. Forthcoming Features
-----------------------

As you can tell this program still has a lot of stuff to be added, below is a brief
list of features that will almost definately make it into Version 1.0

	* Registration Code, the Shareware version will only allow 20 die types and 10
	  roll types to be stored at any time. Possibly use my Registration Manager
	  format for the Registration code (or just rip the code out of Black Widow 5.
	* Keyboard control over the backdrop roll list (cursors to change the
	  selected item, allow copying and deletion and editing from the screen.
	* More die types, including Variable and Variable Dice, the first allows
	  you to select a constant value, the second allows a variable number of
	  dice to be rolled at once, entered by the user at Roll time.
	* Dice string convertor, will convert a string as displayed by the
	  dice view and icons into the internal format of the program, either
	  for quick rolling or for saving to the desktop. (To be a plugin)
	* One off rule driven roll (the normal roll system)
	* Comprehensive Help file, You'll need it to get the full use out of
	  the system, but default you can create simple nDn+nDn... form rolls
	  by just entering numbers, but you need to experiment to get the most
	  out of this program. (80% done)

Features that might creep in at a future date (Possible and BlueSky stuff)

	* Character sheets (this would require a massive new module that would
	  allow a lot of new features, such as automatic save throws, auto
	  character generation (using any selected roll type), automatic hit point,
	  Magic point,etc ... update, level up handling, overnight routines (for
	  healing/ magic point regeneration, spell learning, prayer learning.
	  Plus a full character sheet editor (HTF would that work? Multiple windows?
	  Drag and drop? How do you double/right click?)
	* Games Master Plugin - Allow a lot of games master functionality as well
	* Plugin Die Types (possibly using my Generic Plugin Module system), to give
	  access to all the features required by the Character sheet.
	* Infrared/Serial linkup will allow Users to send and recieve rolls and
	  results, run a game on Psion Palmtops? Where do you get a honda to honda
	  serial lead?)
	* PBEM (Play By E-Mail) system to send roll messages across the net and recieve
	  them at the other end as attachments and to be able to load them in and
	  use the results.

5. Copyright Notice
-------------------

(C) 1998 DudleySoft

To be finalised, however this product is current Copyright to DudleySoft, and is
Freeware at present, this may be liable to change in time, this incomplete alpha
version may be distributed only as the .SIS file provided in its current unaltered
form, you may copy the Roller.die and Roller.rolls files in the System\Apps\Roller
directory between machines, but the rest of the product may only be distributed in
it's current packaged form, any other form of distribution is prohibited, as is
any charge (excluding cost of packing and handling that may be incured from the
transfer of the .SIS file, i.e. Floppy Disks, Postage or Internet connection 
charges).

6. Contact Information
----------------------

The easiest and quickest way to contact me (James Watson) is to use my email
account, dudley@dudleysoft.com, this is currently forwarded to my home account
and is currently checked at least once a day.

I wont be releasing my Snail Mail address, however am currently living in Scotland
near the City that was the European City of Culture and is apparantly Miles Better.

7. Extra (Esoteric) Information
-------------------------------

This program and ReadMe file was written to the Melodic Sounds of The Corrs, and Sleeper
and the not so melodic, but equally good sounds of Green Day. I used the Beta version
of OPLPlus by Andy Clarkson, on the EPOC Emulator (Rel version) emulating the Geofox
screen layout (for extra screen size) and tested on an 8Mb Psion Series 5, with
Compact Flash memory. The icons are spash screen were produced using Paint Shop Pro 4 and 
5 and converted using bmconv.exe, the PC used is a Pentium 200MMX clocked to 233Mhz with 
72MBytes of memory and a 4.3 GByte hard drive. The whole package and readme file were 
packed and ZIPped using WinZip 7 Beta.
				

