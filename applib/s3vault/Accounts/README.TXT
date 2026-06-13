Accounts version 1 for the Psion Series 3a/3c designed and written by Michael Shields, aged 16, July 1998.

Thankyou for downloading this ZIP file!

IF YOU AREN'T INTO READING TEXT FILES PLEASE AT LEAST READ THE LAST FEW PARAGRAPHS!

Accounts.zip should contain:-

Accounts.opa - the main Accounts program.
User.rec - The file containing name and password information.
Readme.txt - this text file.

NB: If this ZIP file is named the same as another program already in the same area as this one you can rename it:-

Accmick.zip or...
Mickacc.zip or...
accver1.zip

Introduction
------------

Accounts is a small banking utility that basically keeps track of your bank account/s. It can only handle basic credit/debit transactions at the moment as I wanted a program that would simply keep track of the money I had without all the advanced features of most of the other money-related programs available. I created it because I had just opened a new account and I wanted a SMALL program that kept track of the money in the account and a good feature of it is its small size (for those who want a basic accounts program but can't fit the big ones on their machines), about 7K! Having said how basic it is I hope you won't be put off because it works well and looks good and I plan to put some more features into it in the future if their is sufficient interest.

Installing
----------

IF YOU ARE NOT COMFORTABLE WITH DIRECTORIES AND COPYING PLEASE READ YOUR USER MANUAL SO YOU UNDERSTAND THE FOLLOWING INSTRUCTIONS!

You can use any utilities that allow copying from your desktop computer to the Psion such as PsiWin, RCOM, Maclink, SYLINK etc...

1. Create a directory on your Psion's Internal disk called \ACC (meaning it will be M:\ACC\). This is where the various accounts will be saved with the extension .acc.

2. Create a sub-directory from your newly created \ACC directory called \DAT (meaning it will be M:\ACC\DAT\)

3. Copy Accounts.opa from your desktop computer to the \APP (M:\APP\) directory on your Psion's Internal disk.

4. Copy User.rec from your desktop computer to the newly created \ACC\DAT (M:\ACC\DAT\) directory. This file contains the name and password you will have to type in when starting Accounts to gain access. The name and password are specified by you.

5. Install Accounts.opa in the normal way (from the "Apps" menu in the system screen).

Using Accounts
--------------

When you first run Accounts you will be asked to enter a user name and password before you can continue. These details are stored in a small file called user.rec in your \ACC\DAT directory. This file will have to be edited (in Data) by you so you can specify your name (first name) and your password. You should use your first name as the name and maybe your PIN number, part of your account number etc as the password or you can make one up. If the details are submitted and don't match exactly (case sensitive) the ones in user.rec then you will be told "Invalid Identification" and you will have to type them in again (the file initially has the name "John" and the password "Accounts" as an example)

The next screen after this greets you warmly with Good  morning/afternoon/evening followed by your name (for that personal touch) and gives you 3 options, open an existing account, create a new account or delete an existing account. If you choose to create or delete an account after you've done that you will return to this screen, however if you open an account you will be taken to the main statement screen.

The main statement screen shows a mini-statement of your last 8 transactions with your account number and sort code shown at the top and the time and date at the bottom. If their are no entries in the account it will obviously be blank but if entries are in the account they will be shown with the different fields of the transaction:-

DATE - the date the transaction occurred (max 10 characters).

DETAILS - the details of the transaction (you can also make notes here [max 20 characters]).

CREDIT - the amount credited (shows 0.00 if the transaction was a withdrawel [max 999999.99]).

DEBIT - same as credit but obviously shows the amount debited.

BALANCE - shows the current balance in the account.

Pressing <MENU> will bring up the options in Accounts (NB: nothing happens when you press <HELP> because I decided against a help system in a pretty self-explanatry program). Their are three menus in Accounts:-

The "Account" menu:-

OPEN - opens an existing account (no error is raised if you open an account that is already the open account)

CREATE - creates a new account but doesn't make it the current account. You obviously can't use a name that has already been used as an account name.

The "Transaction" menu:-

ADD ENTRY - adds a new credit/debit transaction to your statement and the new balance is shown. If the statement is "full" ie. it has all 8 lines filled the first entry is deleted to make room for the new entry so the statement always shows the last 8 entries.

REMOVE LAST ENTRY - removes the last entry in the statement (an entry isn't regained at the top of the statemant as it has already been deleted). This can be used to correct mistakes or to clear the whole statement.

The "Other" menu:

PROGRAM INFO - displays a copyright notice and a few notes.

EXIT - exits Accounts (all updated accounts are automatically saved)

About the author and program info (for anyone who cares!)
---------------------------------------------------------

My name is Michael Shields and I'm 16 years old. I won my Series 3a 512K in a competition about a year ago. I bought the OPL Programming manual about six months ago and I have been learning slowly since then. I wrote this program during my extra-long summer holidays because I have been on study leave and I've just finished my GCSE's. I've got until september before I go back to do A-Levels (I keep asking myself why I am subjecting myself to another 2 years of school on top of the 11 I've already done!). Incidentally I want to study physics, maths, and biology. I'm also taking a course in BASIC programming so I will probably improve my OPL with that as well.

This program is the first I have attempted for the Psion and I am very pleased with the result. Although it is small it took me a long time to get it going how I wanted (I'm no OPL whiz!). Because of this I have made the program DONATIONWARE which means I would be extremely grateful for any donations that you would like to send. With your donation I would also appreciate a small note saying where you obtained the ZIP file and any ideas/bugs you have about the program.

My snail-mail address is:-

Michael Shields,
2 Lancaster Court,
Kingston Park,
Newcastle upon-Tyne,
NE3 2YX,
England

As well as asking for a small donation I also have a few questions! If anyone can send me any answers/solutions to these as well I would be very grateful!

1. Is there a relatively simple way of printing a .PIC file from the Psion in OPL?

2. I can't grasp how to play .WVE files from OPL with the Programming Manual. If I wanted to simply play a .WVE files as it was recorded how would I do it (say one called sound.wve)?

3. Does anyone have the Hitchhiker's Guide To The Galaxy for use with the Infocom Interpreter in .IDF format that they could send instead of a donation?

4. Does anyone have the URL's of any sites that have infocom games that work on the Psion (.IDF format)? 

5. Could anyone tell me if their is a special way of naming the versions of a program i.e. if a major upgrade has been done do you jump up one whole version number etc...

Thanks to Phil Creed for providing an icon for Accounts in his icons.zip file. In return I will print his details below. If you would like any design work done for you contact Phil:-

100302.1314@compuserve.com

21 Seymour Road,
Wollescote,
Stourbridge,
West Midlands,
DY9 8TB

Thanks also to Steve Litchfield for creating a great Psion area on his Psion Web pages. Visit the 3-Lib Shareware library site at:-

http://3lib.ukonline.co.uk/index.html

If interest is descent I might incorporate printing and standing orders etc or your own suggestion to make this utility better.

I prohibit any form of reverse translation on this program. This program can be freely distributed (please do distribute it anywhere you want!) as long as all the files that come with the original Accounts.zip (Accounts.opa, readme.txt, and user.rec) remain within it, unmodified in anyway!

I (MICHAEL SHIELDS) RETAIN THE COPYRIGHT!

If you do want to know anything about the program or you would like samples of the source code please mail me and I will probably send them to you. Please don't be disgusted with any code that you think you could have done better, I tried and it works doesn't it? :-)

Thanks again and in advance for all your kind donations which will undoubtedly pour through my letter box with letters of praise! :-)

Disclaimer
----------

This program has been tested on a PSION SERIES 3A 512K (ROM version 3.25F) on which it is designed to run on and has worked fine on it. BE AWARE THOUGH THAT YOU USE THIS SOFTWARE AT YOUR OWN RISK AND I CANNOT AND WILL NOT TAKE ANY RESPONSIBILITY IF THIS SOFTWARE CAUSES ANY ADVERSE EFFECTS TO YOU (?) OR YOUR MACHINE (I don't see how it could but it's better to be safe!). I PRESUME THAT THIS PROGRAM WILL RUN AND WORK FINE ON THE SERIES 3C AND OTHER 3A COMPATIBLES BUT AS SAID ABOVE IF ANYTHING GOES WRONG (DATA LOSS ETC...) DUE TO THE USE OR MISUSE OF THIS PROGRAM I WILL NOT TAKE RESPONSIBILITY FOR THOSE PROBLEMS. READING THIS AND/OR USING THE PROGRAM CONSTITUTES ACCEPTANCE OF THE ABOVE!

PLEASE ALSO BE AWARE THAT I PROMISE NO FUTURE VERSIONS OF THIS PROGRAM. I'LL DECIDE IF I WILL RELEASE ANY UPGRADES.

Sorry about that but you have to be careful these days!

That should prevent any lawsuits or attempts on my life.

I'll shut up now and let you use Accounts! :-)


