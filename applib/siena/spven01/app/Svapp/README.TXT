 /--------------------------------------------------------------\
( S p a c e V e n t u r e - D e a l  w i t h  t h e  g a l a x y )
 \______________________________________________________________/

                   Guide notes for version 1.3
        a game by Kirk Bowe for the Psion 3c/3mx and Siena
    Copyright (C)1998, 1999 by Kirk Bowe.  All rights reserved.
             This document was updated on 18 Jan 1999

      Web site: http://www.kirkboweassociates.com/spaceventure
                Email the author at: kirk@kirk.net
          Support queries: svsupport@kirkboweassociates.com
            Sales queries: svsales@kirkboweassociates.com
                All comments & suggestions welcome

NOTE: The shareware version can be upgraded to the registered
version by simply typing in your registration code - there is
no need to download any additional software.  See the end of
this document for the simple registration options.  Your code
is also good for all future 1.x versions of SpaceVenture.
Although every care is taken, there is no guarantee that game
positions saved in earlier versions will work correctly in
later versions.  You are advised to keep backups of your
saved games file (playfile.odb) and your registered key file
(svlic.odb - only applicable if you have registered).


What's new in this version
==========================

** Create your own add-ons to SpaceVenture through the SVAPI
(SpaceVenture Application Programmers Interface)!  See the end
of this document for further details.

** Small performance tweaks and a few glitch fixes

Introduction
============

SpaceVenture is a space trading game. In it, you take on the role 
of a lone space pilot whose desire it is to make some serious 
money. In fact, it has no real objective as such -- your aim is 
simply to travel the length and breadth of the 10 galaxies in the 
system (if galaxies have lengths and breadths!), soak up the 
atmosphere, and trade in the right goods to make a profit. With that 
profit, you can afford better craft, which can hold more cargo and 
travel further in one go.

The central concept of the game is of trading. Each planet has, like 
the markets on Earth, markets dealing in a wide variety of items, 
ranging from computers to military weaponry. Planets too, of 
course, vary in their socio-politico-economic makeup, and so 
different products are in demand on different worlds. Your view on 
all this is made simple by the TradCom, or Trading Computer. 
Whenever you arrive at a planet, your on-board computer loads up 
with the current prices available on that planet. You simply need to 
decide what's worth buying and - equally importantly - where you 
can sell what you've bought for a tidy profit. The planet information 
screen, accessible when you press the Enter key over a planet in 
your NavCom screen, will give you a full analysis of the planets in 
your current galaxy.

SpaceVenture is currently under continual development, and as such
benefits greatly from your comments and suggestions (and
registrations!).

Getting started - installation
==============================

SpaceVenture is normally supplied as a single .ZIP file, called 
spven01.zip. Note that the "01" part may be higher (e.g. "02", etc.) 
in your version. The .ZIP contains all the files you need to run 
SpaceVenture on your Psion. It has the following structure and 
components:

\APP\sv1.opa (The main program itself for the Series 3c and 3mx)
\APP\sv1sien.opa (The main program file for the Siena)
\APP\SVAPP\svovl*.opo (Additional support modules)
\APP\SVAPP\*.pic (many picture files)
\APP\SVAPP\readme.txt (a copy of the text of this page)

Ensure that the sv1.opa file goes in your \app folder and that all the 
other files go in your \app\svapp folder (which you should create if 
necessary).  If you have a Siena, make sure the sv1sien.opa file
is in your \app folder, and delete the "sv1.opa" file.

Once the files have been copied to the \app and \app\svapp folders 
on your Psion, install SpaceVenture from the System screen by 
choosing Apps/Install from the menu (or by pressing Psion+I).  
Select "sv1.opa" and press Enter.  The SpaceVenture icon should 
now appear in your system screen.

If you are using a Psion Siena, choose "sv1sien.opa" instead of
"sv1.opa".  This will ensure that SpaceVenture corrects itself
for your Siena's screen.  Also make sure that you delete (or
move to another directory) the "sv1.opa" file.

NOTE FOR SIENA USERS: you can save space by deleting all the
".pic" files that do not have a "s" immediately before the ".pic"
part of the filename.  For example, you can delete "navcom.pic"
(but ensure that you keep "navcoms.pic").  The reverse is also
true for Series 3c and 3mx users if you're running out of space.

NOTE TO ALL USERS: when upgrading to the latest version, please
ensure that you install all the new files from the new .zip.
You will of course wish to keep the following two files from
your existing installation:

	playfile.odb		This file contains your saved games
	svlic.odb				Your license file (registered users only)
	
You should therefore copy these two files to a temporary
directory before installing the new version.  Once the new
version is installed, simply copy back your saved game and
license files to \app\svapp\.


Getting started - running the game
==================================

Select the SpaceVenture icon on your system screen and press 
Enter. SpaceVenture will now display a logo screen which will also 
inform you whether this is a registered or unregistered version. The 
logo screen will disappear and be replaced by a dialog box 
welcoming you to the game. The dialog will ask you whether you 
want to start a new game or load up a previous game save. Since 
this is your first play, you'll want to press N for New.

You must choose a name for your new captain and a difficulty
level for the game.  There are currently three levels of difficulty.
The more difficult you select, the more expensive are the
space craft you can buy and there should be less variation in
prices between worlds (so that easy profiteering is more difficult).
It is recommended that first time players choose the "Easy" level
of difficulty.

Type in the name of your captain (up to 20 letters) and press Enter. 
The screen will now show the "OnBoard" window. This is your main 
screen which gives you most of the important information you'll 
need to know, such as how much money you have left, which 
planet you're on and which one you've selected as your destination, 
your cargo hold weight, etc.


Using the menu system
=====================

At almost any time during the game you can hit the Menu key. 
This will bring up the SpaceVenture menu. The menu is divided into 
2 main areas:
    Views: The various screens that your on-board computer has
    Actions: Commands you will need to carry out at some point

Views - OnBoard
---------------

As mentioned above, this view provides you with general status 
information. Use this screen to check: your current planet and your 
chosen destination (if any), your current cash situation (shown in 
GCUs - Galactic Credit Units), your ship model, its maximum and 
current holds, and the amount of fuel left in the tank (shown in LY - 
Light Years). Several other screens also return you to the OnBoard 
screen when they've completed their action.

Views - TradCom
---------------

This is your trading computer screen. This gives you easy access 
to the markets that go on in the space stations orbiting the planet 
and on the surface below. The main screen divides into four 
columns: item, units, current price and quantity available. Generally 
speaking, the prices will not fluctuate much - unless you hear 
about it through an "Incoming Message" (see below). The quantity 
availble is likely to be much less stable, however.

If you see an item you want, press the "B" key (buy). This brings 
up a dialog box which you must use to select the item you want 
(alloys, driods, etc.) and the quantity. Note that the quantity is 
usually in tonnes. This is because almost all items (including gold) 
are shipped within heavy containers designed to withstand deep 
space pressure. Therefore, although it may say "tonne" next to 
gold, you're really only paying by the gramme.

The amount of money and available cargo space you have are also
displayed in a grey box underneath the Buy and Sell buttons.

If you have enough money to cover the transaction, enough cargo 
space and if the space station's container service has enough of 
the item to sell, the transaction will be complete and your account 
and cargo hold will be altered accordingly.

NOTE: only type numeric characters into the quantity box (i.e.,
type 1000, do not type 1,000).

Press the "S" key if you'd like to sell any of your existing cargo. 
You do not need to sell all of any item in one transaction (e.g., you 
could just sell half your alloys here).

NOTE: your cargo bay stores cargo in 1 tonne units, so you should
only attempt to sell in whole units - e.g., you can sell 5 tonnes
of your artefacts, but not 5.22 tonnes.


Views -- NavCom
---------------

Probably the most complex view to work with is the NavCom 
(Navigation Computer). Use NavCom to select your next planetary 
destination. The NavCom comprises two screens - the galaxy view 
and planet information view. In the galaxy view, use the cursor keys 
(and the shift key for speed) to move around the planets. As you 
near each planet, its name will appear next to it, and the distance 
(in Light Years) between your current planet and this new planet 
will be displayed in the bottom right hand corner. Obviously you 
need to check that you have enough fuel for the trip. Once you've 
decided on your destination, pressing the space bar will indicate it 
to the computer.

When over a planet in the Galactic screen, pressing the Enter key 
will give you the Planetary Information screen. This is designed to 
brief you on the social, economic and political nature of the planet 
and its people. It is wise to heed this information as it normally 
reflects the market prices available at that planet. For example, an 
agricultural world is likely to produce food and oil but consume 
technology. The prices usually reflect this.

Views -- Cargo Bay
------------------

This view simply lists all the items you're currently carrying on 
board, along with the quantity of each. To buy and sell items, go to 
the TradCom view.

Views -- Pilots' Lounge
-----------------------

Almost every space station has a pilots' lounge area where pilots 
can sit back and relax. More often, however, pilots are more willing 
to trade and haggle with their fellow pilots. By choosing the Pilots' 
Lounge view, you can partake in all of this.
Depending on when you arrive, there could be up to five other pilots 
in the lounge. Pressing the number button below the pilot will alert 
his/her/its attention. The pilot will then attempt to sell you an item 
of their cargo. You have the option of saying "no thanks", of 
accepting outright or of haggling with the pilot. If you accept 
outright, you are presented with a dialog box just like the "buy" 
dialog in the TradCom view.

If you try to haggle, enter an offer price, i.e. your bid. The pilot will 
consider your bid and either accept or reject. When deciding how 
much to offer, try and get into the other pilot's mind: if you make a 
low bid and there' four other pilots in the room, he/she/it is far less 
likely to sell to you.

NOTE: only type numeric characters into the quantity box (i.e.,
type 1000, do not type 1,000).


Actions -- Check inbox
----------------------

During the game you will receive a large number of messages, whether
they be notifications of stock price changes on planets or other
things.  These can be difficult to keep track off, so version 1.1a
introduces a special email inbox where all these messages are
stored.  Choosing this menu option takes you to the Inbox screen.
Use the up and down arrow keys to list through the messages that
you have received so far.  The messages are individually date and
time stamped for your convenience.

As the messages are currently only applicable while SpaceVenture is
running, the inbox is deleted whenever a new game is started or a
previously saved game is loaded.  NOTE: you may periodically wish
to delete the inbox file - inbox.odb - in the interests of disc
space.


Actions -- Launch
-----------------

If you have selected a destination, this menu item engages your 
craft's hyperdrive. Of course, you can only do the trip if you have 
the fuel to cover it - i.e., your fuel (shown in Light Years in the 
OnBoard view) must be greater than or equal to the Distance to 
travel.

Actions -- Buy new ship
-----------------------

If you've got the money (and the registered version) then you can 
obtain a better ship. They may seem amazingly expensive at first, 
but once you're in the swing of things you're cash-flow situation 
should improve dramatically! Each of the craft offer progressively 
larger cargo bays and bigger fuel tanks. That means you can carry 
more cargo and travel much further in one go.

Actions -- GalDrive (REGISTERED VERSION ONLY)
-------------------

The good news: all the craft featured in SpaceVenture are capable 
of intergalactic travel. The bad news: the ISF (Inter-stella 
Federation) tax on any intergalactic travel is 1000 GCU. The even 
worse news: the current temporary upgrades to your ship are still in 
their technical infancy and cannot predict or dictate exactly which 
galaxy you'll end up in! Still, sparing use of GalDrive is a great way 
of changing the scenery - nothing like another 100 or so planets to 
trade on! And remember that SpaceVenture features 10 such 
galaxies!

Incoming messages
-----------------

Occasionally you will receive news flashes from the Intergalactic 
Monetary Union's watchdog arm. These reports often indicate that 
there has been a sharp rise or fall in a particular price on a 
particular planet. If it looks interesting to you, you may like to 
re-think your plans and head in that direction.

As from version 1.1a, Incoming Messages are automaically added to
your inbox - which you can check from the Actions/Check inbox
menu option.


Police raids
------------

The more civilised the planet (e.g. democracies), the more likely
you are to be inspected at regular intervals by the local
law enforcement teams.  If you carry contraband goods (mostly
slaves, narcotics or firearms) you run the risk of having them
confiscated.  You may be lucky, as sometimes they do not take
all your goods -- only what they can find.


Missions (REGISTERED VERSION ONLY)
--------

It seems like everyone has a cause these days, and the galaxies
of SpaceVenture are no exception.  Occasionally you will receive
messages from planets offering handsome rewards in return for
quantities of certain goods.  Copies of these messages will appear
in your inbox for future reference.  You may only accept one
mission at a time, although if a better one comes along you
may override your previous choice.


HINTS AND TIPS
==============

1. If you've seen the name of a planet but can't find it anywhere, 
press F while in the NavCom and you'll be able to type the name in. 
If found, NavCom will automatically place the cursor over the planet.
2. Pilots will almost never accept a really low bid if there are four 
other pilots in the lounge. Try a more realistic offer.
3. If you've seen an interesting planet far away and are making your 
way there in short hops, keep an eye on your fuel level. It's often 
better to get in to the habit of spending a few credits on fuel at each 
stop, which probably means you'll have to do some minimal "safe" 
trading too.
4. The big profits are likely to be made from highly polarised 
trading: computers to agricultural planets, oil to post-tech planets, 
etc.


SUPPORT
=======

The SpaceVenture web site contains all the latest information and 
bug fixes. If you encounter any problems that cannot be answered 
by the web site, please send an email to 
svsupport@kirkboweassociates.com (registered users please 
supply your full name when emailing).


FEATURES SLATED FOR FUTURE 1.x VERSIONS
=======================================

Siena & S5 versions ** IR beaming interaction for two players **
Missions.  The S5 version is likely by the end of April 1999.


VERSION INFORMATION
===================

Details of major changes and bug fixes to this version are given at
the end of this document.  Please ensure that you have the latest
version of SpaceVenture!  Either check the web site or email
svsupport@kirkboweassociates.com for the latest version number.


CREDITS & THANKS
================

Concept, development, design & programming: Kirk Bowe
Many thanks of course to the many SpaceVenture players who have
supported the development of the game with their constructive
comments and suggestions.


Please register your copy of SpaceVenture!
******************************************

SpaceVenture represents many hours of thought and design, and is
regularly updated on the basis of your many requests. By 
registering SpaceVenture, you will get access to 10 galaxies, each 
with around 100 planets to trade on. You will also be able to 
upgrade your ship to ones which can carry more cargo and hold 
more fuel. On registration you will receive a unique code which you 
type in under Special/Enter Key.  Registration also help fund
further development, and incorporation of your many suggestions!

You can register in two main ways: direct to the author or on-line 
with a credit or debit card:

REGISTER DIRECT WITH THE AUTHOR: the cost is 12 GBP 
(twelve pounds sterling). You can send this by cheque (UK banks 
only), by UK Postal Order or by International Money Order. Make 
cheques etc. payable to Kirk Bowe and address to Kirk Bowe, 20 
Watts Lane, Teddington, TW11 8HQ, United Kingdom. Please 
state YOUR FULL NAME AND ADDRESS clearly and state 
whether you would like your registration code by email, fax or post.

REGISTER ONLINE: you can pay by credit or debit card by going 
to this Internet address: http://www.reg.net/product.asp?ID=2710 . 
The cost in US dollars is $25 and this is inclusive of RegNet's 
handling charge.
In both cases, orders are normally acted on within 24 hours of 
receipt in London. Registration codes requested by email or fax are 
normally sent out within those 24 hours; registrations requested by 
post are typically sent out by first class mail within 48 hours of 
receipt in London.

IMPORTANT COPYRIGHT NOTICE: SpaceVenture, its program 
code and graphic design is copyright (C)1998 by Kirk Bowe. All 
rights reserved. Unauthorised use is prohibited by applicable laws.

DISCLAIMER: SpaceVenture is provided as-is without any warranty 
or guarantee of quality or fitness for any particular purpose. The 
author cannot accept any responsibility for any loss or damage 
caused as a result of installing or operating SpaceVenture.  Bugs
may, however, be reported to svsupport@kirkboweassociates.com, and
every effort will be made to attend to them.  As in all situations,
you are strongly advised to make regular back-ups of all data
stored on handheld devices before installing new software.

CHANGES IN VERSION 1.2a
=======================

Fixed a minor issue which meant that completing certain missions
would cause the program to crash

CHANGES IN VERSION 1.2
======================

Fixed number of small bugs ** Police raids ** simple missions


CHANGES IN VERSION 1.1b
=======================

Added automatic upgrading of player game files ** Further work
on getting the Siena screen right ** Fixed some currency
display issues


CHANGES IN VERSION 1.1a
=======================

Added first version of the "inbox" concept ** Added 3 levels
of difficulty concept ** Corrected issue with GalDrive


CHANGES IN VERSION 1.1
=======================

This is the first version to work with the Siena's screen size.
Some minor keyboard control issues resolved ** TradCom now
updates properly when you repeatedly buy more of the same item
on the same planet

CHANGES IN VERSION 1.0f
=======================

TradCom now visibly updates quantities available when buying
and selling ** Fixed some inconsistencies in the Pilots' Lounge
** Loading a saved game fixed ** Some text tidied up ** General
game stability increased

CHANGES IN VERSION 1.0e
=======================

Fixed bugs in NavCom/TradCom modules which meant that trading
information would update with the destination's info without
having to travel there

Added handy status (GCU, cargo bay) info to the TradCom screen
to avoid having to go back to the Status screen to refresh your
memory!

Fixed several other minor issues


CREATE YOUR OWN SPACEVENTURE ADD-ONS WITH THE SVAPI! FREE!!
===========================================================

As promised in earlier versions, v1.3 now allows you to create
your own add-on routines in the OPL language.  In order to do
this you must be competent at writing in the Series 3/Siena OPL
programming language.  Please see Psion's web site for more
information on programming with OPL.

What will I be able to do?
==========================

With the information supplied in the SVAPI documentation, you
will be able to create your own routines which can look as
though they are a seemless part of SpaceVenture.  This is a
brief summary of what you could do:

 ** Customize the items available to buy on planets (change
 their names, make them cheaper or more expensive)

 ** Customize the available craft (names, cargo sizes, etc.)

 ** Create your own launch routine (perhaps add an arcade-like
 aspect to the game!)

 ** Have your own menu item on the main menu

In all, the possibilities are endless.  And best of all, you're
free to distribute your efforts to other SpaceVenture users!
All registered users (please make sure you download v1.3!) will
be able to take a copy of your file and add it to the
SpaceVenture folder.  You must abide by the distribution
instructions which are supplied as part of the SVAPI.


I'm hooked!  What do I have to do to get the SVAPI??
====================================================

It's free, so don't worry!  The documentation isn't supplied
with the standard distribution purely to save disc space.  To
obtain your copy, simply email svapi@kirkboweassociates.com.
In reply you will receive the SVAPI Guide (plain text).
