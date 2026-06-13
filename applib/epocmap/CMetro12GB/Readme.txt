=======================================
METRO     Version 1.2 GB   Juillet 1998
         pour PSION Series 5
=======================================

© Gabriel DARDENNE



INTRODUCTION :
------------

This software gives you a lot of information about public conveyances. 
It includes a search-engine which draws up the Metro- or RER-routes 
(farestages 1 and 2) and tram routes (lines T1 and T2).

This software is shareware.

Although the realisation has been done with great care, the author does 
not take over responsibility concerning information, especially the ones 
dealing with time and fares. In fact the different sources of 
information (plans, guides, Minitel and so on) contain contradictory 
information. On the other hand not all of them could be tested because 
of the high number of possible routes. Nevertheless the reliability of 
the data is acceptable because several corrections have already been 
made. It would be nice if you can tell me about mistakes (there 
certainly are still some...) to improve following versions. Thanks for 
your help.



WARNING :
-------

A few pogrammers will be shocked and others will be disappointed. 
Indeed, this version for S5 relies on version 4.1 of S3a/S3c and Siena 
palmtops. The conversion took me about two months. So, the specific 
features of the S5, especially the touchscreen, are not supported 
(except the menu). But it is promised that the following versions will 
bit by bit support the functions.



FICHIERS :
--------

METRO_GB.APP
METRO_GB.AIF
METRO.DA1
METRO.DA2
METRO.DA3
METRO.DA4
METRO.HLP
METRO.UTI
and
README.TXT (this document)
REGISTRATION.TXT



INSTALLATION :
------------

Copy the files listed below into the directory  \SYSTEM\APP\METRO_GB of 
your disk C or D.

This directory will also contain your registration file (for those 
people who have already registered).
The icon METRO will automatically be added into one of the menubar 
called EXTRAS.



LIST OF FUNCTIONS :
-----------------

1) METRO :

- ROUTE :

The search and the detailed presentation of routes (text, detail and 
plan) is possible if you enter a starting- and an arrival-station. This 
is the main function of the program.
This function can be used for the 13 lines of the Metro, the 4 current 
lines of the RER (farestages 1 and 2) and the lines T1 and T2 of the 
tram.
It would not be wise to enter the whole name of the station, but only a 
few letters. Then the program proposes a choice of all the names which 
contain the entered list of letters. The spelling of the names is not 
always identical (hyphens, abbreviations...). If only one station 
belongs to the entered list of letters, the program continues without 
demanding confirmation.

* If it is a direct line, you will get the answer very fast. If you 
enter for example LYON (for gare de Lyon) and CHATEL (For Châtelet Les 
Halles) Search-time: 3 seconds.

* Also if a change has to be made the search-time remains short. Try OPE 
(Opéra) and NORD (Gare du Nord) Duration: 9 seconds.
For ORL (Porte d'Orléans) to ITAL  (Place d'Italie) 4 seconds are 
sufficient. The graphical display of the shortest route enables you to 
repeat the search with 2 connections (key D). This is only of interest 
if  the route shown makes a detour. In this case there are probably 
shorter routes. Maybe the search takes more time: e.g.: OPERA - GARE DU 
NORD, 27 seconds.

* Routes which needs two connections: e.g.: BOTZ (Botzaris) and PLA 
(Laplace), searchtime 6 seconds. If the number of the found solutions 
increases, the searchtime gets longer. The bar shows the progress of the 
search.

* In the case of more than two changes the automatical search is not 
possible and a message indicates you that no route has been found. The 
program proposes you to cut your route into two pieces with one more 
connection. Don’t forget that 95% of the routes are possible with less 
than 3 changes. Try PELLEPORT and BROCHANT. The program proposes you to 
search successively for PELLEPORT-JAURES than for JAURES-BROCHANT.

If the progam finds solutions, ALL the possible routes will be given 
(with a maximum of 49 solutions) in estimated increasing duration time. 
The shortest route (time) will be dislayed. You have the possibility to 
navigate through the given solutions and view them on the map of Paris 
(key P) or view the list of the crossed stations (key S). You can save 
one or more solutions (key E) to consult them later.
Direct access to a solution, called by its number, is possible with key 
N.

Why ALL the solutions are proposed ? The answer is quite simple: the 
computer examines anyway ALL the possibilities to find the time of 
duration. But nothing forces you to consult absurd duration-
times...(e.g. the 23rd solution to go from the GARE DE L'EST to NATION, 
trough DENFERT. It makes even sense: when the RATP announces that 
traffic is interrupted on line "such and such". Then you just have to 
press the arrow-down key until you see the fastest route which does not 
use this line !! Elemetary, my dear Watson ! To go directly to the 
fastest or the longest route, use Fn+arrow (up or down).


- REVIEW :

This function makes it possible to review a route, put in memory before 
(key E, see before). The advantage of this search is the quicker access. 
Use the keys S and P (see before).
This gives you the possibility to strore frequently used routes and to 
review them as often as you want (the maximum of routes is 28), without 
a long search. You can also delete a route.


- LINES :

Detail of all stations of one line, with the indication of possible 
connections. When you push the key P, the screen shows you the route on 
the map of Paris. You have the choice between all the metro lines as 
well as the sections of the RER-lines (Zones 1 and 2) and the tram-lines 
T1 and T2.


- STATIONS :

Informationsabout the 343 stations known to the program: all the metro 
lines from Paris as well as all the RER-lines (Zones 1 and 2) and the 
tram-lines T1 and T2.
You will know the available connections.
The asterisk after some station-names indicates a special feature 
(circle-line, double-line, etc...)
The served places and monumets are shown as well.
When you press the key P, the screen shows you the position of the 
station on the map of Paris.
The stations on the outskirts are a little bit "pushed" vertically...the 
screen of the PSION is to small....!!
A management of your personal notes added to the stations is possible 
with the key N. These notes are then displayed in the information of the 
stations.


- MONUMENT, LIEU :

MONUMENT, PLACE:
A few hundred key words have been entered to make it possible to search 
the nearest possible station. One example: to go to the funiculaire de 
Montmartre enter FUNI (or NICULAI...) and the information of the nearest 
station ANVERS are displayed. You can see this place on the map of Paris 
when you press P.
If more than one solution is possible (e.g.: MUSEE), they will be shown 
successively (Space-bar) and all the places will be visible on the map.
Some stations have the name of the nearby monument. In this case start 
your search in STATION, because it is possible that the name is not 
repeated in the file of the mouments.


- NOTES :

A search by the content of the notes added by you to the stations (max. 
28 stations) is possible with this point.


2) BUS :

- LINES 20 TO 96 :

All the lines from Paris are detailed with their features (time-table 
and days). Please pay attention, not all the numbers are used by the 
RATP...
The majority of the lines are using one-ways and a lot of the stations 
are only served in one direction. Maybe there are a few errors in the 
given data, but the consulted plans were not very clear (your 
corrections are welcome for the future versions....).


- PETITE CEINTURE (PC) :

This particular line makes the turn from Paris on the external 
boulevards, the line works in both directions. The drawing is very 
simplified.


- NOCTAMBUS :

These are the 18 nocturnal lines. It is the same principle of display as 
for the normal lines.


- MONTMARTROBUS :

Very small, nice and anecdotal line but very useful to avoid a long way 
up.


- BALABUS :

Normal line with special fares, the line works an Sundays and public 
holidays.


- STATIONS :

The principle of this menu is a little bit different than METRO, because 
after you have entered a string of characters, all the concerned 
stations are displayed inclisive the used lines. It is in deed very 
common, that nearby stations have different names in the busses from 
Paris. That’s why the search for routes like in METRO is impossible for 
busses. There are few connections with absolutely same names.
Try FRIED (for FRIEDLAND) for example. You will get FRIELAND-HAUSSMANN 
(line 22) or CH. DE GAULLE-ETOILE-VICTOR HUGO-FRIEDLAND (line 83).
Also "ZA" will give you "Gare Saint-Lazare" and "Botzaris".


3) OTHER MEANS OF CONVEYANCE :

- RER :

This menu gives you the representation of the lines A to D, like for the 
busses, with farestages and connections with metro-lines.


- STADE DE FRANCE :

In this year of the World Cup, this information are certainly useful for 
foreigners.


- PARIS-ORLY and PARIS-ROISSY :

Text-screen mode, which gives you precious information about conveying 
possibilites to the airports West and South. Please pay attention if you 
have to take a plane, fares and times are given without guarantee 
(traffic jam, etc...).


- TOURISM :

Some ideas of walks concerning the theme of means of conveyances.


- PLANS :

What we still can expect in regard to the means  conveyance of Paris.


- USEFUL PHONENUMBERS :

No comment.



LIMITS OF THE PROGRAM :
---------------------

- Certain lines are composed of features which made it complicated to 
program (loops and Y-junctions).
The loops have been arbitrariliy eliminated and all the stations have 
been brought to the same line. These stations are marked by an asterisk 
which shows a special feature (only served in one direction, for 
example).
The Y-junctions are treated as two different lines:
a mean-line and a secondary-line which starts by the station where both 
lines are separated, this station will then be a connection.
These two modifications don’t really change the reality of the things (a 
few stations and minutes more for a route or a change which in fact will 
not be one if you have chosen the right train).
- Special feature of certain stations on circle-lines or on lines with 
Y-junctions (see above).
Certain stations are a little bit complicated:
connections with certain lines, but not with others.
That is the case for "LES HALLES", CHATELET LES HALLES" and "CHATELET" 
which communicate all with different means... It’s also the case for 
"St-MICHEL", "St-MICHEL NOTRE-DAME" and "CLUNY" but also for "GARE DU 
NORD" and "LA CHAPELLE" or even "HAVRE CAUMARTIN", "AUBER" and "OPERA". 
These groups of stations are brought together in one station, even if 
you will have to do a longer change.



NO-REGISTRED PROGRAMS:
---------------------

 As registered programs have advantages in contrast to non-registered 
versions (it’s a question of morality), the restrictions are listed up 
below:
 - the access to certain functions is not a permanent but a random 
access. This is the case for the search for the routes with 2 
connections (Probability of access:1/4), also for the information about 
the lines and the stations (p.o.a.: 1/2) and the saving of your own 
routes.
 Also the metrolines and the RER have some restrictions.
- a message will be displayed to remind you of the support of the 
development of shareware.



SHAREWARE :
---------

 Let’s talk about....
 The development of this program took me several months of research and 
tests. The conversion for the series 5 needed 2 months more. So, for the 
price of 2 ticket-books (48 FF a ticket-book, state: 1.2.98) you can use 
unrestrainedly all the functions and possibilities and also without the 
reminder-messages. Don’t lose your time hacking the code, it isn’t worth 
it and it’s forbidden anyway !!
 
 Send me a Euro-cheque of 100 FF (or cash), or the value of 130 FF in 
your currency (cash or as Euro-cheque) and you will receive your 
registration-code which eliminites all restrictions.
 
 This code works with all present versions (and future versions, if 
nothing oppeses it technically)
 
To all those who trusted me and had themselves registrated for the 
Series 3a/3c or Siena, the registration-code is for free until 31st. 
May. for those who have now the Series 5. Please contact me ! 



 DISTRIBUTION OF THIS PROGRAM :
----------------------------

 It is allowed and even recommended to duplicate and distribute this 
program ! All the original and not modified files have to be distributed 
together.
 For registrated people: do NOT copy the file CODE_METRO !! This is your 
registration-file which contains your personal code.
Because of my small technical equipment, I am just able to copy the 
program an normal PC floppydisks or flashdisks (everything to your 
charges).



HISTORICAL OVERVIEW :
---------------------

- Version 1.1 (for s3a/s3c) has been distributed in September 96. It 
included only information about metro and RER (Zones 1 and 2). The 
actual version is based on the same kernel. This program has been 
completed and improved until December 97 (Version 4.1).

- This version 1.2 for Series 5 is adapted from previous versions and 
some details have been corrceted. The ergonomics of older versions, 
which a lot of people seemed to like, have been preserved.



SOURCES :
-------

A lot of maps, guides, minitel, books and various other documents (not 
to talk about the number of hours spent in public conveyances..). 
Unfortunately there still are contradictions between the different 
sources. The net as they are being presented will be updated in February 
98. For METERO, you still have to wait for a new version...

A few books for those who are interested:
- "NOTRE METRO" by Jean Robert : "the bible", by the historian of the 
métro and the founder of the museum of "Transports urbains de Saint-
Mandé". About this book, which I’ve read a few years ago : I am a 
potential buyer of a used copy, because it is out of print. (Please 
inform me if you know somebody who would sell it. Thank you...)
-  "LE PATRIMOINE DE LA RATP", FLOHIC Editions, very detailed and full 
of little anecdotes.
- "LES AUTOBUS PARISIENS", by René Bellu, Jean-Pierre Delville, Editor
- "L'AUTOBUS PARISIEN 1905-1991", by           
- Dimitri Van Boque, Editions Alcine.



SPECIAL THANKS :
--------------

Thank you very much to you, who use this program (especially to the 
numerous registered for series 3a/3c and SIENA, who encouraged me to go 
on). The constructive criticism helped me again and again to eliminate 
errors and to make approvements (Michel Boudry, Jérôme Fresnay, Franz 
Eberhard,...).
Its distribution by the different servers (Didier Cabuzel, Christophe 
Cordonnier, Gérald Aubard, Steve Litchfield, ...), but also on CD-ROM of 
PC TEAM (Frédéric Botton) and UNIVERS MAC and MICRO-MOBILE(articles by 
Pierre-Alain Buino) which have helped a lot to make this program known. 
Not to forget the encouragements (Laurent Plomb and many others).
The belgian firm MICRO-AZUR, which helped by their consulting to 
accelerate the search for the routes a lot.
I don’t forget my wife who supported me in my numerous talks about the 
subject of the metro...

This program seems to be of interest for you, but you found one or more 
mistakes (not all possible routes could be tested !). So, please send me 
a little description of it or them. I will try to eliminate the mistakes 
with regard to the frequency of appearance.

Nice journey (in thes metro, bus or RER of course...).



Sorry, I don't speak english !


Translation done by Romain LIMPACH

----------------
Gabriel DARDENNE
21/55, rue Voltaire
92240 - MALAKOFF (FRANCE)

e-mail : 106367.3112@compuserve.com





