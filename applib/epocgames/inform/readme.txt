Inform v6.21 for EPOC
---------------------

This is the EPOC port of Inform v6.21


Installation
------------
To install you will need the Inform EPOC binary, the Symbian Standard 
Library SIS and the Inform libraries. All of which can be downloaded 
from http://www.palmtime.com/inform.html

First install the Symbian Standard Library SIS. You will then need to 
create a directory on your EPOC machine, such as \INFORM. Put the 
Inform EXE into this directory along with the Inform Library files. 

To compile your .INF file, put it into this directory and double 
tap INFORM.EXE, you will then be prompted for various parameters. The 
compiled .Z file will then be in this directory ready to be run by 
an interpreter and is totally platform independent.


Known Limitations/Problems
--------------------------
This port uses the standard library for input/output to the EPOC machine
which means it uses the Console output. Any output is not buffered and will
not pause so you cannot scroll back to see text that has gone off the 
top of the screen. This may cause difficulty when debugging your code.

To run Inform you will need at least 1 MBytes of RAM free, this will increase
depending on the size of your adventure code.


simon.quinn@bigfoot.com
http://www.palmtime.com/
