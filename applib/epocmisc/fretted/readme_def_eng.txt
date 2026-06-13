How to write an instrument definition file

Table of contents:
0: Introduction
1: Instrument name
2: Number of the strings
3: Open string note and its beginning fret
3-1: Pitch
3-2: Open sting's beginning fret
4: Position mark

----------------------------------------------------------------------
0: Introduction
An "instrument definition file" defines the tuning etc. of a specific instrument.
It has a ".def" extension and is placed in
\system\apps\frets\.

It is actually a plain text file with the following items (in the
following order):

1 Instrument name (1 line)
2 Number of the strings (1 line)
3 Open string note and the beginning of the open string (n lines)
4 Position mark (2 lines)

1: Instrument name (1 line)
An arbitrary string.
This doesn't need to be the same with the file name.
For example, in the default definition file, gtr_std.def, Name string
is "Guitar (Standard tuning)".

2: Number of the strings (1 line)
Decimal number from 1 to 10.

3: Open string note and its beginning fret (n lines)
When the number of the strings is n, you have to specify n lines
from the lowest to the highest (e.g. low E to high E if it's a standard
tuning guitar).
Each line should look like the following:
Pitch,Fret number of the open string

ex)
A3,0

Both of the two parameters are described below.

3-1: Pitch
This program uses a MIDI-like notation for pitch. To specify the frequency
of a note (see Note1), you have to use a note  name and a number that
represents how high the note is.

Actually I don't know anything about MIDI, all my knowledge is the
following:
In some MIDI systems the "middle C" is notated as C3, the same note in
some systems C4. I simply take the latter.
I even don't know if a note a half step lower than C4 is B4 or B3.

Anyway, in this program, starting from C4 (the middle C),
do re mi fa so la si
is denoted as
C4 D4 E4 F4 G4 A4 B4.
Notes that are n-octaves higher than C4D4E4F4G4A4B4 are denoted as
C4+n D4+n E4+n F4+n G4+n A4+n B4+n.
n can be a minus integer.

ex)
C that is 2 octaves higher than the middle C is C6.
A that is an octave lower than A4 is A3.

+ and - mean sharp and flat, respectively.

ex)
A1+, B3-

Note1)
Actually, this notation only specifies the frequency of the note in 
relative to A4. So, that's more or less "interval notation" than "pitch
notation", but I think that's OK for most of us.
By the way, the frequency of A4 is specified by the user within the
program.

Note2)
EPOC devices are in general small, so are their speakers. They are
good for producing higher notes, but sometimes they are not suitable
for imitating the real pitch of your instrument.
For example, a guitar in standard tuning has the open notes
E2 A2 D3 G3 B3 E4.
When trying to generate these notes on Psion Series 5mx , you will notice
that the lowest 3 or 4 notes or something are hard to recognize.
Therefore, in the default guitar definition file, gtr_std.def, I assigned
the notes that are actually two octaves higher than the real notes:
E4 A4 D5 G5 B5 E6.
This probably makes some confusion (especially in "mini quiz" mode).

3-2: Open string's beginning fret
It is zero in most of the cases (e.g. all of the six strings of guitar),
and you'll rarely have to use anything other than "0".
However, in some cases (e.g. 5th string of banjo) you have to write
the number of the fret other than zero for the open string (e.g. "5").

"Minus frets" (as in the lower-than-E string of some of the guitars
where you can actually fret on -1 fret) are not supported.

4: Position mark (2 lines)
position/number definition line
positionmark drawer module name

Position marks are defined by two lines: One is to specify the position
and the number (e.g. two marks between 11 and 12 fret),
and the other is to specify the shape of the mark (e.g. dot).

Position/number definition is a line containing 12 digit number.
The first digit represents the number of position marker between 0 and 1
fret, ...
Ex)
For Guitars I assigned 001010101002.
This means 1 dot on 3/5/7/9 fret, 2 dots on 12.

Positionmark drawer module specifies the actual shape of the position
markers.
There are three default definitions, i.e. dot, block and no (nothing).
The standard values I assigned are:

For guitar
001010101002
block

For banjo
101010100102
block

For ukulele
000010100201
dot

You can write your own module to draw position markers other than block
and dot.
However, for the moment I don't have time to write a detailed description
of how to write positionmark drawer module (but I'd like to do that).

If you write a nice instrument definition and/or position mark definition,
and are willing to share your definition with others, please send me
the file and I'll include it in the Fretted distribution package.
Unless it's unusable (i.e. Fretted cannot read it), I'll not discriminate
it. Please feel free to send in the most exotic definition.

Keita KAWABE
ktkawabe@hi-ho.ne.jp