License
-------
This program is copyright ¸1997 Berthold Daum.
You are allowed to use this program free of charge
for noncommercial purposes.
You are allowed to distribute this program
as long as no files of this package are omitted
or modified.

Contact
-------
lichtbild pty ltd
21 margate st
beaumaris, vic 3193
australia

email: lichtbild@compuserve.com
www:   http://ourworld.compuserve.com/homepages/lichtbild


Introduction
------------
This program is the result of heavy programming
during a JAL flight from Sydney to Tokyo.
The first fractal paintings were created during
the connecting flight from Tokyo to Frankfurt.
It was good fun - better than Solitaire. But then,
my PSION doesn't have Solitaire.


Basics
------
IFS (iterated function scheme) is based on the work
of Michael Barnsley.
Published in:
Fractals Everywhere by M.Barnsley, Academic Press, 1988.

IFS provides an iterative algorithm to produce 
fractal images of high complexity from a very small
set of parameters.

Each IFS image is defined by a set of contracting
functions (Fi)

Fi:  X(n+1) = Ai*X(n) + Bi

where X(n) is a point vector, Ai is a transfor-
mation matrix and Bi is a translation vector.

In geometrical terms each function (Fi) scales, 
rotates and translates.

The painting process starts by selecting a random
point in the unit square as an iteration point.
A new point is computed by randomly selecting
a function (Fi) according to its probability Pi and
applying this function to the iteration point.
Now the process is repeated by using the new point
as iteration point, and so on.

The process never stops.


This implementation
-------------------

IFSpaint supports up to 32 generating functions
per drawing (most drawings use not more thn 6 or 7 
functions. The drawing space is 2-dimensional, so
the X(n) and Bi are 2-tupels, and the Ai are 2x2
matrices.

The file format is compatible with the file
format of the popular FRACTINT program. However,
FRACTINT can store several images in one file,
but IFSpaint supports only one image per file.
When reading multi-image-files, IFSpaint will 
read only the first image in the file.
Also, 3D files are not supported.

Installation
------------
IFSpaint runs on S3a, S3c, Siena, S3a-Emulator

1. Create a directory \IFS\ on any disk.
2. Copy all files *.IFS to this directory.
3. Copy IFSPAINT.OPA into an \APP\ directory
   on any disk.
4. Install IFSPAINT.OPA to the system screen
   (PSION-I)
   
How to use
----------
The best way to learn about IFS is to open
an example file and to experiment with
the settings. 

Each parallelogram shown on the screen 
represents one function. The parallelogram shows
how the unit square is transformed by the function.
The unit square itself is shown as a box with
rounded corners.

To select a function press TAB. The selected 
function is displayed in black.
To modify a function, first select an operation 
(Scale, Rotate, Translate, Probabilty)
by pressing the diamond key or a letter (S,R,T,P).
Then use the cursor keys to perform the selected
operation.
To create a new function press "+".
To remove a function press "-" or DEL.

For more keyboard shortcuts press HELP.

An "Overflow" error indicates that you created
a non contracting function. IFSpaint will undo
the last operation.

Probabilities:
Probabilities control how often a function is
applied. IFSpaint will always enforce a sum of
1.0 over all function probabilities.
"Equalize" enforces identical probabilities for 
all functions.
"Normalize" enforces probabilities proportional 
to the area size of each function.

Preferences control screen layout and
computing mode: "integer" works faster,
"floating point" preciser.

IFSpaint can save images in IFS file format or
as bitmap (PIC).
To edit IFS files, you may want to install
IFSedit.ALS into an \APP\ directory. If you
choose to edit an IFS file with WORD you must
save the file as TEXT.

To print an image or to convert into other
file formats (GIF, BMP, PCX), save the image
as bitmap, and use products such as PocView or 
PicPoc to print or convert the image.

If you create interesting images, make the IFS
files available to others. You can also send us
these files - we will include the best into future
releases. You may want to include your name with
the title line.

Warning
-------
Because the iteration does not stop and continues
drawing, your PSION will not switch off, as long
as IFSpaint is active.

