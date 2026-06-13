Aim

GARPSI allows you to transfer GARMIN GPS data onto your PSION and to edit and store them. Position data transmitted or received by the GPS are in WGS 84 map datum. So, GARPSI allows you to edit them in any map datum supported by the GARMIN GPS. GARPSI was tested with a PSION Series 5 and a GARMIN 45. It may work with 38, 40 and II models. It don't work with a GARMIN 90.


Installation

The different files must be saved on the same logical unit X ( A or B or M ) in the following way :
X:\System\Apps\GarPsi5\GarPsi5.aif
X:\System\Apps\GarPsi5\GarPsi5.app
X:\System\Apps\GarPsi5\GarPsi5.mbm
X:\System\Apps\GarPsi5\Systems
for the english version
X:\System\Apps\GarPsi5\GarPsi501.hlp
X:\System\Apps\GarPsi5\GarPsi501.rsc
for the french version
X:\System\Apps\GarPsi5\GarPsi502.hlp
X:\System\Apps\GarPsi5\GarPsi501.rsc
Other files are examples of data files. Their using is under your responsability. They must be in the folder X:\System\Apps\GarPsi5\Data
At the initialization, the program creates there a GARPSI.INI file.
WARNING ! Erasing this file needs to register again the code if your are a registered user.


Registration

GARPSI is a shareware program. Without registration you can't transmit files to the GPS. Registration allows you to benefit updating free of charge. You register on line by http://www.regsoft.com/ on page EPOC. Payment of $30 by international credit card or US check. You also must complete and send me LICENCEe.TXT with payment to be registered. I'll send your code by return of post (and email if you have it). Payment: 200FF or UKP20 or DM60 or $40 on a french bank or cash. 


Preferences

At the first use and by the "Setup" menu option, you can fix your preferences :
- the UTC time offset ( O in UK in winter) ,
- the map datum (to have the same position on the GPS and the PSION needs to have the same map datum),
- the position format.
Only degrees are used with different subdivisions because other UTM and British grid are unknown to me. But I can add them if you explain me the directions for use !


GPS link

With a PC I made up the DB9 male end to the GARMIN lead.
The wires are :
white ( data in )  - pin 2
brown ( data out ) - pin 3
black ( ground ) - pin 5
red ( external power ) - not needed
Pins are numbered from the right to the left and from top to bottoms when you look at the soldering pins. After then you can connect the GARMIN lead to the PSION serial link.
With a Mac miniDIN 8 map
   X  X  X    6
 X  O  X  X 3
   X  X  X    1
Pins are numbered from the right to the left and from bottoms to the top (O is the plastic stud).
The wires are :
white ( data in )  - pin 5
brown ( data out ) - pin 3
black ( ground ) - pin 4
red ( external power ) - not needed
WARNING ! Don't connect your GPS unit to a RS232 port because it delivers +25V to -25V signal. You can destroy your GPS !


Position data entering

You have to use degrees but use of character " ° " or " ' " isn't necessary. The software gives conventionally a negative value to the southern latitudes and western longitudes. So, " - " or " s " or " w " (upper or lower case) like first character  will give a minus sign to the entering data . Then this one will be read like a southern latitude or western longitude according to the field. After then a point will be read like a decimal point and any other not numeric character like a subdivision change (from degrees to minutes or from minutes to seconds). Example: if you use " ddd°mm.mmm " format, " 45.5 ", "n51-25.3 ", " w2/8/5 ", " -56 15 90 "will be read like " N45°30.000' ", " N51°25.300' ", " N02°08.083' ", " S56°16.500' "in a latitude field and " E045°30.000' ", " E051°25.300' ", "W002°08.083' ", " W056°16.500' "in a longitude field.


GPS data saving and restoring

The frames transmitted by the GPS are saved without reduction in files with a " ALM " or " ROU " or " TRA " or " WAY " extension corresponding to satellite almanac, routes, track log and waypoints.  Before return to the GPS, data must  be formated in this way.


Data reducing Frames received from the GPS can be reduced except the almanac. Almanac reducing isn't interesting. Yet, its saving could avoid a long and dull "cold start" of GPS if the battery fails. Reduced data are stored in files with " RBF ", " TBF ", " WBF " extensions. Before return to the GPS, DBF files must be converted in data frames in " ROU ", " TRA " or" WAY " files.


File edition

You can create a new file in this way. This option shows position data in selected Map datum and position format. You also can transmit the current data to the GPS unit, search a point by characters chain and sort waypoints by increasing distances from the current waypoint. "file Split" allows you to save a part of a file in an other file. So you only select  the closest waypoints.


Other facilities

Psion clock setup must be used with care. It is always 1 s to 10 s late and I didn't find the reason. "add Waypoints" option allows you to add waypoints directly onto your GPS in a more convenient way. You can enter the current position automatically like the "Mark" button on your GPS or any other position. Note that these waypoints aren't saved on your PSION.


Updating history

Version 1.0a - 05/01/1999 - First alpha release unregistrable


Acknowledgements

Thanks to Jonathan Duff, PSIGAR author. His program is avalaible on http://ourworld.compuserve.com/homepages/Jon_Duff/ homepage.htm It incites me to work out a more universal program. Indeed, PSIGAR seems suitable to walkers because it gives a great place to their curious Map datum ! I choosed the same principle to obtain DBF files in two steps. That allows to have speedy transmission between the GPS and the PSION. Thanks to William Soley. He wrote a GARMIN/GARMIN protocol document available on http://playground.sun.com/pub/soley/ garmin.txt


Responsability

The author don't accept any responsability in consequences of this program's use.


Author

J.L.HOCHART 9ter rue Gambetta 50130 OCTEVILLE FRANCE (33) 2 33  93 53 34 email: molpetrus@geocities.com URL: http://www.geocities.com/Colosseum/Dugout/7981/
