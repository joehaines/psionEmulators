********************************
* Maze3a V1.1c ALPHA  - README *
********************************

Maze3a V1.1c ALPHA, it's program code, graphical representation, sound 
effects, and game design are copyright (c) 1995 Stephen Roberson, All Rights 
Reserved.

Maze3a V1.1c ALPHA is FREEWARE - You may fully distribute it as long as all 
copies are identical to this version, that you make no attempt to reverse 
translate or modify the code, program or files in any way, and that 
all versions are accompanied by this unmodified README file.

Maze3a V1.1c ALPHA may NOT  be sold without the express written permission 
of the Author.

Program files that are part of Maze3a V1.1c ALPHA are:

Maze3a.opa
Mazeline.opo
Mazeshad.opo
Dinogfx.pic
Yell.wve
Roar.wve
Maze3a.txt      (This README file)


What is Maze3a?
---------------

Maze3a is a 3D Maze game written in OPL for the Psion Series 3a range of 
pocket computers.

The object is to escape the randomly generated 3D maze before the nasty 
dinosaur gets you. Comprehensive in-game help is provided, and the game 
has been written to be as user friendly as possible.

The game has been written on and off over 5 months on my daily train and 
tube journey to work! (Proves how portable and useful these Psion 3a's are!)

It was written entirely on a 512k 3a - no code was developed on the PC, 
though the graphics were drawn using a PC Graphics package.

The game icon was designed using my own custom icon editor - ICONEDIT3A,
which is also available on CIX and various Psion software libraries.

The game is currently at the ALPHA stage - it is about 95% complete. 
There are a couple of bugs yet to be fixed, and some more graphics to go in, 
but it is fully playable.

I'm releasing it as ALPHA so that I can get some feedback on the game before 
I finish it. Please let me know what you think of it. Either EMAIL me, or 
write to:

Stephen Robertson
106 All Saints Road,
Gravesend,
Kent,
DA11 8RN

Email: stephen@argonaut.com

Comments, suggestions, bug reports, large amounts of cash, welcome!

In fact if I don't hear anything from you, then it's unlikely I'll get round 
to finishing Maze3a, so please let me know what you think!


How do I install Maze3a?
------------------------

Maze3a requires approximately 45k of free disk space to install (including 
sound files), and requires approximately 85k of free memory to run.

It  is not fussy where it is installed, and  can be installed on any drive 
(Internal, Flash SSD, RAM SSD, or Remote), in any directory.

First copy the Maze3a.OPA file to the drive and directory you wish to 
install it, e.g. LOC::M:\APP

Maze3a looks for its datafiles in a subdirectory called MAZEDATA.  Create 
this subdirectory in the directory where Maze3a.opa is installed  
e.g.: LOC::M:\APP\MAZEDATA

Copy the following files into the MAZEDATA directory:

	Mazeline.opo            (Required)
	Mazeshad.opo            (Required)
	Dinogfx.pic             (Required)
	Yell.wve                (Optional)
	Roar.wve                (Optional)

The optional files are Psion sound samples (.WVE files). These can either be 
omitted to save space (they are fairly large) or you can replace them with 
your own sound effects, providing you use the same filenames.

Install the game as you would any other application, i.e. from the system 
screen APPS menu.

You should see the Maze3a Icon. Select this and press ENTER to run.

How do I play?
--------------

When you run Maze3a you will see a copyright screen. Press a key to quit 
this, and the game menu will be displayed. 
The game menu has the following options:

	Play Game
	Change Settings
	Help

In all menus press Enter to select, or Esc to quit.

Play Game:
^^^^^^^^^^
Launches the game with the current settings. The current game settings are 
displayed to the left of the menu.

You will see a grey box, in which the maze will miraculously create itself 
as you watch. You have as long as it takes to generate the maze to study it 
before you start playing the game.

When the game starts the map will disappear and the maze will be displayed.

In the top left hand corner of the screen is a grey box which shows your 
current direction.

To move forward press Cursor Up, to turn left or right press Cursor 
Left/Right.

To view the map press M - the map will be displayed, but areas you haven't  
explored yet won't be visible. A flashing black dot shows your position. 
North is towards the top of the screen.  Press any key to exit the map.

Wandering the maze for a short while will cause the monster to activate. 
Find the exit (a black square, always in a dead end) and enter it before he
has you for lunch.

If sound effects are turned on and the sound files are present you will hear 
the monster roar occasionally. The louder the roar, the closer he is.

In the bottom left hand corner of the screen messages will appear when the 
monster roars, so that even if you have no sound you still know how close 
the monster is.

In the game press Menu for the game menu, or Help for game help. Esc pressed 
in the game will quit back to the start menu.

Change Settings:
^^^^^^^^^^^^^^^^
Allows the user to change the game settings. These are:

Display         (Shaded, Line)
		This alters the graphics style of the maze. Line
		graphics are faster, but Shaded graphics look
		prettier.

Maze Size       (Small, Medium, Large or Gigantic)
		Selects the size of the maze. Larger mazes take
		longer to generate.

Level           (Easy, Normal, Hard, Very Hard)
		This is the maze difficulty level, and it affects 
		the number of routes through the maze.
		Easy mazes have many routes through to the exit, while
		Very Hard mazes have only one solution!

Monster         (None, Slow, Normal, Fast)
		Affects the speed the monster chases you. Select 'None'
		for no monster.

Maze Type       (Random, Specific)
		Random mazes are generated randomly, using a 'seed'
		number taken from the system time.
		Specific mazes are generated from a user-input seed     
		number. If specific is selected you will be asked to enter
		a seed number when you exit the options page.
		When you complete or fail to complete a maze you are given
		its seed number so that you can play it again if you wish.

Sound           (On, Off)
		Turns the sound on or off. If you have no sound files 
		installed then this option has no effect.

Help:
^^^^^
Activates the help menu, which gives full help for the game.

Known Bugs & Design Flaws
^^^^^^^^^^^^^^^^^^^^^^^^^
(These will hopefully be fixed in the final version)

1.      Occasionally it is possible to get a 'mirage' or frozen image of the
	monster on the screen. This is caused by the monster moving away
	round a corner just as you turn round and see him.
	Moving forward, or rotating left/right will cause the image to vanish.

2.      When the map is displayed the monster does not move, but the timer      
	still counts up.

Features to be added  (this depends on the feedback I get)
^^^^^^^^^^^^^^^^^^^^
1.      Better sound effects

2.      Game Over animation (Monster eating you!)

3.      Progressive game, where you start off on a small level and work your
	way upwards through the levels.

4.      High Score Table

5.      Saved preferences

6.      The exit to be drawn as a door, and not just a black square.

7.      Proper compass graphic

8.      Any good ideas you suggest!

-------------------------------------------------------------------------

Thanks for reading this README file. I hope you install Maze3a and 
enjoy playing it - please let me know what you think.

Best regards,

Stephen Robertson,
Author of Maze3a
June 1995

