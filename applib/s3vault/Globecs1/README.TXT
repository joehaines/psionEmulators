*** GlobeCs v1.0 - What is it? 
It is a 3D vector graph of the globe. The globe is turnable, the display is 
zoomable and scrollable. The program is small, relatively quick.
 
*** Installation and use, legal status 
Put files in the following directories: \APP\GLOBECS.OPA, \OPD\GLOBECS.ODB, 
with Psion-I (English version) install the OPA. It's all. I hope the menu is 
self-explanatory enough. 

GlobeCs is a Public Domain, if you use it, or its parts in your own programs, 
please credit me. Use it as is, there are no registration and support for. 
 
*** HW requirements 
Psion3a (but I guess it runs on 3c and Siena too), needs approx. 30-40 kB 
memory to run, plus the PIC size, which depends on globe radius (max. 51 kB). 
 
*** Data representation and vectorization 
The coastline of continents and islands and some big lakes is given like ODB 
file in binary form. Each point of the coastline represented by two integers 
(absolute longitude and latitude in degrees, West and South is negative), the 
end of a polyline indicated by two zeros. The usual step between the break-
points is not greater then 7 degree (i.e. 3 bit, in this way it would be 
possible to save components of a relative vector with their signs in 1 byte, 
but the unpacking is a surplus time consuming procedure). Some attempt made to 
keep continous line even in case of near islands. In this sense I am proud of 
the Sumatra, Java and Lesser Sundas, the British-Irish Islands as well as 
US-Canada Great Lakes but the Philippines turned out not very well. Presently 
the file consists about 1500 points. By now is missing some islands in Arctic 
Ocean, it means 300-400 additional points. 
 
*** Computation and speed 
The rotation of globe and calculation of visible/hidden points requires much 
trigonometry beyond the spherical-Cartesian transformation. For speed-up the 
code I used the following tricks: 
- almost all repeated calculations are in 2 or 4 byte integer arithmetic, 
- pre-tabulated SIN for range 1-89 degrees with step 1, normalized to 1024 as 
integer, look-up of SIN and COS for any whole number degree angle in this 
table, 
- procedure CACHEing.
All of this together resulted about 10-12 times faster run. 
I decided not to use gPOLY, and gUPDATE OFF because prior to draw a step is 
necessary to make a great many calculations, and the speed increase was not 
very significant. But it is very nice to see how 3a plots point-by-point! 
 
The initial preparation of SIN table and dark background takes 7 sec. The time
of plot recently is approx. 44 sec. This last value practically does not depend 
on the globe size, and the view point. 
 
*** Features in use 
The user can jump to any view point in the Earth. It is possible to move the 
view point by optional 1,2,...,45 degree steps in N, E, S and W directions too.
Thanks to vector graphics, the zooming is very easy. The optional globe radius
is from 80 pixels (you can see the whole globe in the display) to 320 pixels 
(wider, than the 3a display), varies with 40 pixels steps. The program 
calculates and plots the map outside the visible display too. After finish the
plot the actual map is scrollable by arrow keys. The map can be saved as a PIC 
file to the \PIC directory too. The filename is forming from the actual view 
point coordinates. 

*** Other programming remarks, reverse engineering
The 1024 SIN normalization factor comes from the following considerations:
- this value is enough big to get accurate x and y coordinates on display,
- if you multiply 3 such SIN or COS values, you are still in long integer
range, because (2^10)^3 < (2^8)^4.

I ENCOURAGE to make reverse tanslation. The majority of variables is GLOBAL, 
and the global names you can get back by e.g. REVTRAN. All variable and 
procedure names are transparent, or understandable by menu.
 
*** Future plans 
- to complete the missing islands' data, 
- to show the daylight and dark areas on the globe depending on actual date and 
time in form of ellipse or sickle at the grey layer of bitmap; this could be 
processed and updated separately from the black layer, 
- to stole city coordinates from ROM/World files, and put cities on the globe, 
- further probable speed increase by store instead of angles the normalized to 
1024 SIN and COS values of longitude and latitude for each point,  
- long-range fly display (input departure and arrival points and times, get 
great-circle path, with moving in time sign of the aircraft - pocket version 
of the big on-board screens), etc. 
 
*** Similar programs 
D. Rushall's Solun 3 in registred version is able to show the 3D globe map. I 
have seen only its display hardcopy, but as I understood, that globe is fixed 
size. Am I wrong? J. McAleey's W2 program does for 2D map case the daylight/
dark screen.  

*** Author 
I am engineer-physicist, my hobby is the handheld and personal computing. 
I live in Szekszard (18 42E, 46 20N :-), Hungary. 
A Psion3a belongs to me only since last spring (the earlier pocket pet was the
Atari Portfolio). Since I made some family photo galeries, animations with 
asinchrone sound, Hungarian phonemes and rules to A. Clapham PsionSpech 
program, etc. 
 
I welcome ideas and suggestions about ROM/World data handling as well as 
other comments. Enjoy our globe in 3D! 

Andras Cserhati 
cserhati@tea.npp.hu
