HEARTS
======

A spreadsheet to keep the score whilst playing the card game 'Hearts' (also
known as 'Find the Lady' or 'Dirty Lizzie').

Spreadsheets can be knocked together to keep track of score in various games.
Using them allows the score keeper to enjoy the game more.

Please feel free to distribute the following files unchanged - 'hearts.spr'
and 'readme.txt'. It is hard to envisage any problem/financial loss resulting
from the use of the spreadsheet - you can be sure that I won't accept any
responsibility should the circumstance arise!

The 'readme.txt' file (the one you are reading!) gives a little background
for the spreadsheet and it gives installation/usage instructions.

'Hearts.spr' should be loaded and run from the S3a spreadsheet. It will be
most convenient to load 'hearts.spr' into a '\SPR\' subdirectory (on any
drive).


Hearts - the game.
------------------

The card game is based upon whist and is for three or more players (I think
four is the optimum number). The pack should be dealt evenly between the
players for each hand (remove cards in advance if necessary - eg. one card
when there are three players). Players select two cards to pass on before
play begins (the Queen of Spades can be passed on - you can refuse to
receive it though).

Each trick is played without 'trumps', the winner of the trick takes the
cards. At the end of the hand, each player checks the cards they won to
determine their score. Cards which contribute to your score are summarised
below:-

-	Queen of Spades - 35 against.
-	Jack of Diamonds - 10 for.
-	All hearts score against:-
	-	Ace - 15.
	-	King - 13.
	-	Queen - 12.
	-	Jack - 11.
	-	The rest score their face value.

Bear in mind the following additional rules:-

-	If a player takes all the cards that score against then they get no score -
	all other players get the score instead.
-	The game ends when a player breaks a pre-arranged limit (eg. 250).
-	The winner is the player with the lowest score at the end of the game.
-	Hearts cannot be lead until at least one heart has been thrown away on a
	previous trick (you cannot 'break hearts').


Hearts - the spreadsheet.
-------------------------

The spreadsheet keeps track of the score for up to 8 people. For illustration
it comes set up with three players - Player1 to Player3.

It will have to be set up before the first game:-

-	If player names need changing then:-
	-	Switch off 'Titles' (press PSION t).
	-	Enter player names in cells from B3 up to I3.
	-	Ensure unused player names are cleared. Highlight up to I3 then press
		DELETE (use PSION w to set the width to 0 of the unused columns).
	-	Cell Q1 should have the correct number of players.
	-	Cell Q7 will display the number of cards you need. If necessary, remove
		cards from the pack in the following order 2 of Clubs, 2 of Diamonds, 2
		of Spades then 2 of Hearts.
	-	Cell Q6 will show the total number of points expected per round. It
		assumes the 2 of hearts is removed for less than 50 cards in play. S4 to
		S18 give the scores for each card - you can change them if you play
		different scoring rules.
	-	Move the cursor to cell B4. Switch titles back on via PSION t.


Before each game:-

	-	Clear the scores from the last game (highlight them all, then press
		DELETE).
	-	Set the player number who will lead first in the game in Q4 (from 1 up to
		the number of players).
	-	Set the burst score in Q5. The spreadsheet comes with 250 in by default.


Column 'A' from row 4 onwards displays the name of the player whose turn it
is to lead first for each hand.

Recording the score:-

	-	Record score in the next free row (starting at B4).
	-	After the first score is entered, the score remaining for the hand
		replaces the player name in column A (it should show 0 when all score
		for the hand has been entered).
	-	The row from B2 onwards shows each players total score so far.
	-	The row from B1 shows current position of each player.
	-	If a player bursts then 'Winner' is displayed as the positon for the
		players in first place. All 'player to lead' names in column A are
		replaced with 'Winner' as well.


I would be pleased to hear any comments, suggestions or problems. Please send
them to:-

	Andy Scales
	'Braeside'
	Upleatham Street
	Saltburn
	Cleveland
	TS12 1LR
	England

Phone 01287 624811

