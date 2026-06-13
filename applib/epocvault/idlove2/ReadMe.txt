ReadMe File for I'd Love 2 But ... (Version 2.0)
================================================

0. Intro
--------

Congratulations on choosing I'd Love 2 But ..., the random excuse generator,
and now also fortune cookie generator, this program will still give you that
all important excuse for just about anything, but it now also contains nuggets
of useless information and pithy sayings that you can amaze your friends with,
and it still has the facility to allow you to add your own excuses and fortunes
to the already provided versions. Also available at no extra cost (since I'd Love
2 But ... is free) is a program that will convert UNIX style fortune cookie files
into I'd Love 2 But ... fortune cookie files, see the Fortune update section for
details.

There are now two flavours of I'd Love 2 But ..., the full and the lite version,
the lite version is for those with little diskspace, and simply contains fewer
fortunes (though with over 600 in the lite version that is still a lot) to conserve
your precious memory.

1. Installation
---------------

Installing I'd Love 2 But ... couldn't be easier (well it could, if I could afford
the Development Kit and could create .SIS files) simply create a folder in your
\System\Apps folder called IdLove2 and copy all the files from the archive (you
don't actually need this one if your short on space). When you next check your
extra's bar, you'll find a nice shiny IdLove2 Icon sitting ready for use.

2. Using I'd Love 2 But ...
---------------------------

The operation has changed slightly, the program is designed to stay resident (but
only the OPL part, not the fortunes), when you click on the Icon if it is running
already it is brought to the foreground and displays either an excuse or a fortune
in a standard dialog.

If you want to change it's behaviour for future call ups simply click on the
Config button and you'll be allowed to select the behaviour of your choice, there 
are two options:

	On Startup - When the machine is turned on you can choose I'd Love 2 But ...
		     to do one of three things, either Nothing, or display either an
		     excuse or a fortune.
	On Open    - When you open I'd Love 2 But ... (click on it's extra's bar icon,
		     or select if from the task list) the program can display either
		     an excuse or a fortune.

The default options are to display a Fortune Cookie on Startup and an Excuse on Open,
but you may prefer it the other way around, or even nothing to happen when you startup
the choice is, as they say, yours.

Clicking on the Quit button removes I'd Love 2 But ... from the task list, it will
no longer bring up a fortune or excuse on startup, however since the program uses very
little in the way of memory (and processing time) you may prefer to leave it running in
the background, to pop up whenever you need inspiration or an excuse.

3. Uninstalling I'd Love 2 But ...
----------------------------------

If you really want to simply delete the folder IdLove2 from your System\Apps folder,
but makesure you've shut down I'd Love 2 But ... first, since the .APP file wont delete
unless it's closed, and it's easy to leave it running and not realise.

4. Makeing extra excuses and fortunes
-------------------------------------

4.1 Extra excuses
-----------------

To add excuses to the database simply open the \System\Apps\IdLove2 folder and open the
database file Excuses, make new entries in the database, delete ones you don't like and
exit, the database will automatically be updated.

4.2 Extra fortunes
------------------

You could simply open the Fortunes file (also in \System\Apps\IdLove2 folder) in data and
enter new excuses here, there are three lines of data line1,line2 and line3 (imaginative
titles I know) but there is an easier and faster method, simply find a UNIX style fortune
file (these are text files, using a % sign as a seperator) and run the DOS program ConvFort 
on them (ConvFort is downloadable from my Website at http://www.dudleysoft.com/psion.html) 
using the following command:

	ConvFort <inputfile> <outputfile> [<skip>]

where skip is the number of fortunes to skip, if you leave this off every fortune will be
concidered, the default value for this 0. For example if your fortune file is called
SCENE.DAT and you wanted to create a new file called SCENE.TSV (Tab seperated value)
concidering every other fortune simply type:

	ConvFort SCENE.DAT SCENE.TSV 1

for every fourth fortune type

	ConvFort SCENE.DAT SCENE.TSV 3

you will now create a fortune file that contains fortunes of 3 lines or less from the
original file, now to get those into I'd Love 2 But ..., firstly open the original 
fortune file into Data (click on it in the System\Apps\IdLove2 folder) then create a
new Data file, this will use the same format as the Fortune file, now select More>Import
from the File menu. Select the file you've created using ConvFort, select options and
select None for Text Qualifier, and Tab for Label Seperator, leave Entry Seperator as
End Of Line. Now import this into your new database. Once you've done this simply copy
your new fortunes file over the existing fortunes file (\System\Apps\IdLove2\Fortunes)
and your new fortunes will be ready to use.

5. Contacting DudleySoft
------------------------

The easiest way to contact me is via e-mail, my e-mail address is:

	dudley@dudleysoft.com

As you can see, it's really easy to remember (infact at the moment <anything>@dudleysoft.com)
will make it's way to me, but I don't recommend this action since this may change in the
future), you can also visit my website at http://www.dudleysoft.com.

If you have any good excuses, or wonderful fortune files then don't hesitate to send
them to me (MIME encoded, or UUencoded attachments only) and they may well end
up included in a future release, with suitable credits given to whoever sent them in.
