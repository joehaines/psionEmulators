

This is not a manual, I am not writing one because apart from most
people being too lazy to read it, I am too lazy to write one.



The program Geocalc or Sirvay is primarily a geodetic computation
program with a few hydrographic survey routines and a simple GPS
position logging routine.



Most of the routines were originally written for the old 2 line
Psions and as I am a surveyor, not a programmer, please forgive the
lack of flash graphics. I was originally trying to save space on the
old 32k Psions



The motivations for writing this were the errors found in some
software sampled, too stingy to buy commercial software and boredom
on offshore construction jobs.

The motivation for distributing it was the protective attitude to
knowledge distribution encountered. Please feel free to distribute
it. 

Even if you do send $20, I accept no liability for ANY errors leading
to loss of data, money or jobs.



Anyway, TO INSTALL THE PROGRAM:

Copy GeoCalc.OPA to any directory and install it wherever you like.
This is the only file needed. 

SphCnfg.ODB will be automatically created in the OPD directory on the
internal drive.

Included in the zip file is a SPHCNFG.ODB file which may be installed
with some sample spheroid/datum/projection configurations.



Some notes on FAQ's:

a)The rotation convention used in the datum transformations is the
BURSA WOOLFE convention. Test this on known coordinate translations
first if you are using rotations or scale to make sure that you get
the expected results.



b)To carry out a datum transformation, first input the coordinates in
the selected system, then run the translation routine to transform
them to the system required.



c) The GPS routine assumes that the data output by the GPS receiver
is in WGS 84. It will then transform the output (both to screen and
to log) to the selected system.

If the GPS is not outputting on WGS 84, then simply set up the same
Spheroid that the GPS receiver is using with no datum shift.



d) The SA Lo system is the transverse Mercator (Gauss conformal) grid
used in South Africa with 0 degrees direction being south, Y or
eastings increasing to the west and X or northings increasing to the
south. Logical huh?



I will attempt to answer any questions when I have access to my
email. (I am offshore a lot of the time.)

My email address is rossa@mweb.co.za



