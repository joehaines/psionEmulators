FreeCell V 1.07 Apr '97 for the Psion 3a
(C) Copyright Mark Fitzpatrick 1996,1997

1. CONTACT INFORMATION
   ~~~~~~~~~~~~~~~~~~~
	If you have any questions, require some assistance,
	or just feel like exchanging some e-mail, I can be
	contacted via email:
	+ For Compuserve users:- 100026,2452
	+ For Internet users:-   markf@mssoft.com.au
	+ For Snail Mail users:  Mark Fitzpatrick,
				 10 Brandon Place,
				 St Ives, 2075
				 SYDNEY, NSW,
				 AUSTRALIA


2. OVERVIEW
   ~~~~~~~~
	FreeCell is a game of patience played with all 52
	cards of a deck. Unlike conventional patience, each
	game of FreeCell is thought to be solvable so it is
	more like a set of individually challenging puzzles.

	The object of the game is to put all cards in suit 
	and increasing numeric order (starting with the Ace)
	on the four home cells.

	Cards can be moved either to the free cells or on 
	top of another card that is of the opposite colour
	and next numeric value eg. 7 Hearts on 8 Spades or
	Queen Diamonds on King Clubs

	The system will automatically put cards on the home
	cells when they are available. It will not automatically
	move cards that could still be used in the deck.



3. REGISTRATION:
   ~~~~~~~~~~~~
	Freecell a is shareware game. If you like it and wish to 
	continue to use it you should register it. You may freely
	distribute FreeCell as long as all files in the package are
	included.

	An unregistered copy of Freecell will continue to remind you
	to register and will cease to work after 50 games have been 
	played.

	To register:

	1) For Compuserve users, "GO SWREG" and select the file with
	   ID 10674. The charge is US $10 which will be added to 
	   your normal Compuserve bill.

	2) Internet users can register via RegNet - The Registration
	   Network. The URL to reach the information / registration
	   page for FreeCell is: "http://www.swregnet.com/783p.htm"
	   RegNet can be reached on the World Wide Web at the 
	   following URL: "http://www.swregnet.com" or by calling
	   1 800 WWW2REG (1 800 999-2734) or (805) 288-1827
	   The charge is $15 US. RegNet take $5 + 10% for the
	   service, I get the rest.

	3) Send cash in the form of Australian dollars or an
	   International Money order for $15 Australian or a cheque
	   drawn on an Australian bank. Add a 25% premium over the
	   exchange rate if you send cash in any other currency
	   (US Dollars, Pounds, Marks, Francs, or Yen).

	   Send the money to:

	   Mark Fitzpatrick,
	   10 Brandon Place,
	   St Ives, 2075
	   SYDNEY, NSW,
	   AUSTRALIA

	When registering please make sure you send the following 
	information (if possible):
	+ Your name (as you would enter it)
	+ The version number
	+ The version build date and time

	This will enable me to make sure you receive the latest 
	version, no matter which version you currently happen to 
	have.

	Once you have registered, you will be sent a registration ID
	which you enter along with your name. This will turn off the
	nag screens and disable the expiry mechanism. Benefits of
	being a registered user include:

	* Notification of bug fixes and enhancement availability.
	* No charge for fixes and enhancements
	* First 100 registrations will receive a bonus extra game
	  including the OPL Source Code. (by email only)

4. INSTALLATION:
   ~~~~~~~~~~~~
	1) Copy Freecell.opa to the \APP\ directory on any drive on
	   your 3a.
	2) (Optional) Copy Freecell.rsc to an \APP\FREECELL\ 
	   directory on any drive of your 3a.
	3) Install the Freecell.opa game using Psion-I from the
	   System screen.

5. HISTORY
   ~~~~~~~
	27Mar96 V1.0    First Release
	30Mar96 V1.01   Crashed sometimes when moving from Deck to
			Free cells and then back to the Deck.
	05Apr96 V1.02   Reduced number of drawables kept open by 
			using one for ALL suits instead of one per
			suit. Keep "special" drawable open for 
			cursors use to fix hangup bug.
	07Apr96 V1.02   Fixed the "Exit Number 25" problem when the
			"\APP\FREECELL" directory doesn't exist. Now
			it creates the directory automatically if it
			doesnt exist.
	09Apr96 V1.03   Added Undo capability for last move, or
			moves if the computer made several automatic
			ones as a result of yours.
			Fixed bug in help. Don't ignore the drive
			info obtained when FREECELL.RSC is found on
			other than the default drive.
	20Apr96 V1.04   Fixed bug where valid moves become invalid
			after aborting a move from Freecell to Deck.
			Add "Smart" cursor movement when dropping a 
			card. The cursor stick's to valid choices
			only when appropriate.
	28May96 V1.05   Changed the crippling to a fixed number of
			games (50) instead of a one month trial.
	05Jun96 V1.06   Allow the FreeCells to participate in the 
			automatic movement to HomeCells. 
			Fixed a bug generating "Invalid Move"s when
			it shouldn't.
			Fixed a bug in the 50 games limit crippling
			that caused registration to be ignored!
	31Mar97 V1.07	Fixed a crash where an array gets overrun. 
			Changed the move behaviour when there are 
			Aces that will be auto-moved.
			Changed the auto place selection to be 
			"more sensible" when moving a card from a
			free cell back to the deck.
			Added new scoring features where best run,
			worst run and current run are recorded
			Added information on registering via the 
			Internet.