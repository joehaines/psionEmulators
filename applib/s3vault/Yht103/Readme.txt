Yahtzee 1.03 for the Psion S3a/S3c
----------------------------------

History:
--------

1.00 	Aug 30 1996 	Initial release.
1.01 	Sep 3	 1996		Small bugfix in using keys A-M after game end.
1.02  Oct 21 1996    Removed grey lines in menu. Works on S3c now.
                     Checks if it is running on S3a/S3c. Does NOT run on
                     a Siena.
1.03  May 14 1997    Made it smaller and faster. Updated registration info.

Introduction/registering.
-------------------------
Yahtzee is a shareware program. So if you like it please register it
on compuserve (GO SWREG, ID=12827) or send cash to the address below.
The registration fee is only $10.
After registration you will receive a registration code by email.
Registration is valid for all coming versions of this program.
It removes the nag screens and enables the 'Undo' feature.
You can now also register on the Internet:
http://www.swregnet.com/384p.htm.

Lieuwe de Vries
Taling 11
9101 ZG Dokkum
Netherlands

Email   : 100121.3102@compuserve.com
Homepage: http://ourworld.compuserve.com/homepages/platodva/psion.htm

Installing:
-----------
1. Copy yahtzee.app to any \APP directory.
2. Use Psion-I to install it to the system screen.
3. Start & Play.

The program makes an ini file called yahtzee.ini in the \OPD directory
on the internal drive. Its only 141 bytes in size. No more files are
created or used.

What is Yahtzee:
----------------
Yahtzee is playing poker with dice (sort of).
You have to fill up 13 categories, 6 in the left column and 7 in the
right. The game ends when all the categories have been filled.

Scoring in the left column:
If you want to score the 'Five's', only the five's in the hand will count.
So, three five's will give 15 points in the 'Five's' category.
63 points or more in the left column gives you a bonus of 35 points.

Scoring in the right column:
Three of a kind	: Scores points in hand if you have three dice of the
                    same kind.
						  e.g. 5-5-5-4-2 scores 21 points.
Four of a kind    : Scores points in hand if you have four dice of the
                    same kind.
                    e.g. 6-6-6-6-3 scores 27 points.
Small straight    : Scores a fixed 30 points if you have 4 dice on a row.
                    e.g. 1-2-3-4-6.
Large straight    : Scores a fixed 40 points if you have 5 dice on a row.
                    e.g. 2-3-4-5-6.
Full house        : Scores a fixed 25 points if you have 3 of a kind AND
                    2 of a kind.
                    e.g. 3-3-3-5-5
Yahztee           : Scores a fixed 50 points if you have 5 of a kind.
                    e.g. 4-4-4-4-4
                    This is the only category to which you can score
                    more than once.
Chance            : Any combination is valid. Scores points in hand.

Playing:
--------
After startup you see an about box, and some information about
registering the program if it is unregistered.
After that the game is ready to play, the first roll already done.

I made the use of keys as simple as possible, so you can play it with one
hand! No use of cursor-keys.

Mark the dice you want to keep with the keys 1-5. Than roll again the
unmarked dice with the ENTER key. There is a maximum of three rolls.
Than you have to assign the score to a category. Use the keys A-M to do
the allocating.

If you score to an invalid category it will enter 0 points (after
confirmation)
e.g. allocate to the 'Five's' category, without having any five's or to
the 'Yahtzee' without having a yahtzee.

If all categories are filled, the game ends. If you have a highscore, it
will ask for your name, else it will will tell you the game ended.
Use Psion-N to start a new game.

Using Psion-O offers you 2 options:
1. Hints
   If set to ON, it shows you hints on score allocation.
   In the left column it marks only 1 category if you have >= 3 of a kind.
   In the right column it might mark more than one category. If you have
   e.g. 3-3-3-4-4 it marks 'Three of a kind' AND 'Full house'.
   It hints only on unused categories.
   If it doesn't hint on anything, you are on your own in choosing the
   category.
2. Animation
   It animates the dice a little.

Use undo the undo the last score allocation. Works only on registered
versions. If used smartly it can boost your high scores.

Information on screen:
----------------------
Points in hand shows you the total number of points in the current hand,
so you don't have to add them up yourself.

Left deviation shows you information about the left column.
If you enter three 'Ones', three 'Twos', three 'Threes' etc. you will
end up having 63 points in the left column, which gives you 35
points bonus. But, if you only entered e.g. two 'Threes' you deviate
three points. Left deviation will then show '-3'.
(You need e.g. four 'Fours' to compensate, or loose the bonus)


Nag screens:
-----------
If you did not register the program, you will see a nag screen after
startup, and before you enter a high score.

About the program:
------------------
This program is written in C.
If you find any bugs or have any remarks, please let me know.


Have fun,
Lieuwe.

