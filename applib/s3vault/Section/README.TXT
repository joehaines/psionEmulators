Section Properties for the Psion Series 3a/c
============================================
A useful utility for students of mechanical engineering and mechanical 
engineers.
Shareware Version 1.0, 27 September 1997


1. Summary
----------
The program provides basic properties of cross sections such as centroid 
location, area, moments of inertia about principal axes through centroid, 
plastic section moduli, shape factors and locations of plastic neutral 
axes. Locations of centroid, area and moments of inertia are basic 
properties needed in much of the analysis for stress and strain. Plastic 
section moduli and shape factors are useful in "plastic" or ultimate-
strength design.



2. Installation
----------------
1. Copy "Sectiond.OPA" into a \APP directory.
2. Select "Install application" (PSION-I) from the system screen and 
   select "Sectiond.OPA".



3. Notation
-----------
1-1, 2-2		principal axes through centroid

3-3			axis through centroid with orientation specified in the 
			figures for various cases 

l-l, m-m		rectangular axes parallel to horizontal and vertical 
			directions, respectively

A			area (length^2)

y1, y2, y3 etc.		refer to the distance to extreme fiber (length) (for 
			each case, these are clearly denoted in the figure)

I1, I2, I3 		moment of inertia about axes 1-1, 2-2 and 3-3,
			respectively (length^4)

Il, Im			moment of inertia about axes l-l and m-m, respectively
			(length^4) 

Hlm			product of inertia about rectangular axes l-l and m-m 
			(note: H12=0 since 1-1 and 2-2 are principal axes)
			(length^4) 

theta 			angle between the principal axis 1-1 and the horizontal
			axis l-l; also, the angle between the principal axis 
			2-2 and the vertical axis m-m (degrees)

r1,r2,r3,rl,rm		radius of gyration about axes 1-1, 2-2, 3-3, l-l and 
			m-m, respectively (length)

Z1, Z2			plastic section moduli  for principal axes 1-1 and 2-2 
			(length^3)
Plastic section modulus is the ratio of the "plastic moment" (Mp: bending 
moment that corresponds to plastic yielding throughout the section) to the 
lower yield strength of the material (sy, also frequently denoted as Sy).  
It is equivalent to the sum of the statical moments about the neutral axis 
of the parts of the cross section above and below that axis.

SF1, SF2		shape factors for principal axes 1-1 and 2-2 (no units)
Shape factor is the ratio of the "plastic moment" (see description for Z 
above) to the moment required to just stress the extreme fiber to the lower 
yield point. This is a cross-sectional property and equal to the ratio of 
plastic section modulus (Z) to the elastic section modulus (L/c). 

yp			 distance of plastic neutral axis from extreme fiber 
			(length)
The distance is either from the top, bottom, a corner or a vertex. The 
location of the distance to the neutral axis is indicated separately in 
each case. The neutral axis is the line of zero fiber stress in the section
subjected to bending. Plastic neutral axis refers to the neutral axis under
fully plastic, pure bending. This neutral axis does not, in general, pass 
through the centroid but instead divides the section into areas in tension 
and compression.



4. Registration
---------------
"Section" is shareware. If you find it useful, send SFr. 20.- or US$ 15.- or 
a equivalent amount of your currency together with your address to the 
address below.
For the small fee requested, you will receive a registered version of the
program that calculates all the sections listed in the menues. If you have
an e-mail address I will send you the program by e-mail, if not, I will send 
you the program on a 3« floppy disk.

If you provide an email address with your registration I will also inform
you about future versions of "Section".

Please let me know of any bugs or comments (good or bad):

	Chris Kunz
	Ammeracher 12
	5737 Menziken
	Switzerland

	E-mail: christopher@bluewin.ch



5. Disclaimer
-------------
The author of this software is not responsible for any damage due
to use of this program. This software is provided without warranty
of any kind.

