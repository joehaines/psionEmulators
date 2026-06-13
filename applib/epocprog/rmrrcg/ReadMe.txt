RMRRCG  - Resource Code Generator
----------------------------------------------
© RMR Software 1999
http://www.rmrsoft.com/

Whilst I was building international versions of my programs I found there was a great deal of importing, exporting, copying and moving of files required, as well as a large number of E-mails with my translators.  So I designed RMRRCG to try and make my life simple.  The principle behind this program is to be a 'shell' from which all the separate actions can be carried out to produce different language versions of a program.  It is also usable by non-developers to produce international versions without help from the original developer.

There is a small Help file with the program, but to save room, this text file should covers everything you need to know and provides a tutorial, using RMREvent as an example, (also installed when you run the RMRRCG.sis file),  When you are happy with the program's operation you can uninstall RMREvent.

Note: Non-developers can now jump straight to Part II - Installing RMRRCG


Part I - 'Internationalising a Program'
---------------------------------------

Basically, for those who haven't done it yet, the procedure for 'internationalising' a program is as follows (again using RMREvent as an example) :

1.    You convert ALL text in the program to RSC variable calls.

    i.e. PRINT "Hello" becomes PRINT ReadRsc$:(Hello_&)

(It isn't essential to add an underscore, I just tend to do it to differentiate these variables from other 'program' variables)

Note also that in RMREvent V1.8, for simplicities sake I have just converted the eight gPRINT lines for the screen display,  this still demonstrates the principle.

2. You add a line to the top of the program "INCLUDE RMREvent.osg". When we have produced it (see later) this OSG file will contain a list of all the RSC definitions (Hello_&...) and their associated addresses.

3. You add a line to the initialisation routine in the program

    RSCId&=LoadRsc$:("<...pathname...>\RMREvent.rsc")

This tells the program to load the actual resource file itself which contains the text to be applied to the definitions. Therefore in the UK RSC file, Hello_&="Hello", in the French file Hello_&="Bonjour" ....etc

4. You then produce the single RMREvent.osg and the multiple international RMREvent.rsc files. (This is all done from within RMRRCG as shown below)


Part II - Installing RMRRCG
---------------------------

(Note that most of the following has already been done for you in the example, using RMREvent. This is what you need to do if you want to use RMRRCG for another program)

For non-developers :  If you wish to force the program into 'Translation mode' simply rename the \Development\RMREvent.opl file to something else (RMREvent.$$$ ?).  This will simulate the normal case when you don't have the program source code.

You should install RMRRCG in the normal way by tapping on the SIS file,   Then, in the development folder you use for each program (for authors, this the one where the OPL/OPP file is stored, for non-developers it can be anywhere), you should create a sub-folder called \Languages\ and then, inside that folder, create sub-folders for each nation using the Psion standard 2-letter designators. So as you see from the example RMREvent, that gives you a file/folder structure something along the lines of :

\Development\RMREvent.opl
\Development\RMREvent.icn
\Development\\Languages\UK\ (the only essential one)
\Development\\Languages\FR\

and so on, depending on which languages you are developing for. At the moment the program supports

\Languages\AM\    (American English)
\Languages\DU\    (Dutch)
\Languages\FR\    (French)
\Languages\GE\    (German)
\Languages\IT\    (Italian)
\Languages\NO\    (Norwegian)
\Languages\SP\    (Spanish)
\Languages\RU\    (Russian)
\Languages\UK\    (English)

Then in each 'national' folder (\UK\, \FR\ etc) you put the text file which contains the variable definitions. So that RMRRCG will recognise these files they have to have a specific format to the filename. I have decided to use <Program Name> + <Two-letter Code > + ".rss". So, as you will see,  the UK definition text file for RMREvent is in \Development\Languages\UK\ and is called RMREventUK.rss, the French is in \Development\Languages\FR\ and is called RMREventFR.rss

The text in the RSS file has to be of a specific format and I suggest you always use the example RMREventUK.rss, suitably renamed, as a template for your program's RSS files, as this maintains compatibility with the official Psion system. Please note that the NAME you allocate to the file in the first line can not be more than 4 characters.

In addition, I also use these same 'national' folders to hold translated versions of the help files, instructions, ReadMe files etc. Useful to keep them segregated from the rest of the program.  RMRRCG can also be used to edit these files from within the 'shell', provided they are named according to the following convention : 

    Help files can be called <Program Name>+".hlp"
        or <Program Name>+Country Code÷".hlp"
        (eg.  RMREvent.hlp or RMREventUK.hlp)

    Readme (Text only) files named as follows:
        UK                Readme.txt
        Dutch             Leesmij.txt
        French            Lisezmoi.txt
        German            Liesmich.txt
        Italian           Leggimi.txt
        Norwegian         Lesmeg.txt
        Russian           Citimne.txt  (actually Czech)
        Spanish           Leame.txt

The next thing to do is start RMRRCG by tapping on the Extras bar.  First thing you need to do is add the first (and maybe the only) program.   For this tutorial type in the name RMREvent and in the development folder highlight the "\Development\" folder.  (Using whichever drive you installed it on)

Again, non-developers can skip the next 2 Parts and jump straight to Part V - Editing Definitions


Part III - Development Cycle
----------------------------

Now let's now go through the whole design cycle using RMREvent as the vehicle.

1.    Select the 'Generate definition list file' option to build the RMREvent.osg file, and the answer 'Yes' when it asks if you wish to translate your source code. This wil make the program 'Resource aware'.

2.    Now use the 'Generate all RSC files' option. This produces 'national' rsc files for all the languages it finds, and stores them in the national folder. So, in this case, it uses RMREventUK.rss to produce RMREvent.ruk, which is stored in \UK\ and uses RMREventFR.rss to produce RMREvent.rfr and store it in \FR\.

3.     You can now use the 'Run RMREvent' option and check each language in turn. The program just copies the appropriate rsc file from the national folder you select (Renaming it to RMREvent.rsc) and then runs  RMREvent.APP.   You should see the screen instructions appear in English and French as appropriate. (Note that when you close RMREvent it takes about 5 seconds for RMRRCG to re-appear, while the Operating System sorts itself out. Nothing I can do about that I'm afraid)   

Part IV -  Adding Definitions
-----------------------------

Now let's look at the way you add extra text definitions. Let us suppose we want to make the word "Find" on the second Toolbar button truly international.

1.    Select the 'Edit OPL/OPP Source Code' option, go to the end of PROC Init: and replace the text string "Find" in the definition of the second toolbar button, with the words ReadRsc$:(Find_&)

2.     Close down the source code (<Ctrl+E>).  There is no need to translate it at this stage as we first need a new RMREvent.osg file with the new defintion included.

3.     When the system returns to  RMRRCG  (Takes about 5 seconds again) select the "Edit RSS file" option and then the "UK" RSS file.

4.     Add a new line RESOURCE TBUF Find_ {buf="Find";} (I find the quickest way is to copy the first line with <Ctrl+C>, insert a copy at the end using <Ctrl+V> and then edit it). Then press <Ctrl+S> to save it.

5.     Now repeat the same for the French file but use "Trouver"

(You will see that the program automatically recreates the revised rsc file when you have finished editing)

6.     Having added a new definition we now need to tell the RMREvent Source code of its existence, so we use the "Generate variable list file" option again to produce a new RMREvent.osg file. When complete select 'Yes" to re-translate the RMREvent source code file.   (If you want to take a quick look at the RMREvent.osg file you will see that the new variable Find_ has been added at the bottom with an address of &6120E00B.)  (Note that if the program fails to compile because of an error, that is not trapped and you will probably not notice until you try and run it and you are told the APP does not exist)

7.     Select the 'Run RMREvent' option and try the UK version first. You will see the ToolBar button has the word "Find" on it. Exit the program, (wait 5 seconds for RMRRCG to re-appear) and run the French version. The Toolbar has now changed to 'Trouver'.  Amazing :-)

Part V - Editing Definitions
----------------------------

Now lets look at how you change the variable definition.  This is where non-developers can amend translations. Suppose you find you have made a mistake, perhaps misspelt a word in one of the lines.

1.     Go to the "Edit RSS file' option and select the French RSS file. Just change one of the lines to something different.

2.     When you exit with the 'Save File' option, the program will automatically recompile the RSC file for you and, as you didn't add or delete any variables, there is no need to run the 'Generate variable file' option again, you can simply use the 'Run RMREvent' option to check if it has changed. So you can see that making small translation changes is now very easy.

That completes the tutorial.  Just a few more words :

Extra guidance for authors:

Once you have finalised the variable list, you can send your translators the RMRRCG app along with the UK RSS file and an untranslated RSS file for their language and they can actually do all the translation work for you, and present you with the final product without you doing anything else. Wonderful :-))  (Note in this case that the lack of an OPL/OPP file is sensed and only the translation options appear on the Menu)

As a final step, you can select the 'Prepare for release' option.  This will datestamp all the files associated with the program and, if it finds one with an extension of .hlp it will compact it as well.  Tideis it up nicely.

You can easily add your own programs to RMRRCG usng the menu option.   It will even create the \Languages\UK\ sub-folder in the development folder for you if you haven't done it yet.


Extra guidance for non-developers

You actually only need a single \Languages\ folder, somewhere on your system, with a \UK\ sub-folder and a sub-folder for your language.   You can then keep multiple RSS files in the folders and the appropriate ones will be used depending on the program you select.

Any problems or suggestions E-Mail me at support@rmrsoft.com

Al Richey