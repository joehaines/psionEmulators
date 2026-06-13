    TCR COMPRESSOR AND DECOMPRESSOR  v1.02

Here's C source and (Atari) executables for creating and extracting TCR files.

TCR is a text compression format that's especially useful with Psion Series 3 
and Series 5 machines: there are text viewers for both which can display TCR-
compressed files directly (VReader for the 5, Reader for the 3).


    SOURCE  (Those funny-looking files ending in .c)

The source code is included here so you can compile it for other machines.  
It's written in C for the GCC compiler, and should be easily portable to 
other compilers.

See the comments in the code for details on the format and algorithms.


    EXECUTABLES  (Those funny-looking files ending in .ttp)

These are for the Atari ST range.  I've only tested them on my Falcon under 
MagiC, but they should work on other machines, and with TOS, MiNT etc.  In 
particular, they handle long filenames correctly.

A TCR-encoded version of this file is included for you to practice on :)


    USAGE  (What do I with them?)

The programs have a simple text interface, for versatility and portability, 
and also because I've no experience of writing GUIs on the Atari :)

Each one takes one or more filenames on the command line, and de/compresses 
those files, creating appropriately named ones with a different 
filename extension - .tcr for compressed files, .txt for uncompressed ones.

In addition, if the decompressor is launched with an empty command lines, it 
decompresses the standard input to the standard output, so it can be used in 
shell pipelines etc.

You can use them from a GUI fairly easily too, e.g. by creating icons on the 
desktop to which you can drag-and-drop files, or by installing applications 
for file types .tcr and/or .txt.


    VERSION  (What's this version number thingy about?)

v1.00
  First release.

v1.01
  Fixed two rare bugs in the compressor.
  Decompressor now handles zero codes properly.

v1.02
  Various improvements.  The compressor is now slightly faster.  The 
  decompressor is about four times as fast, and handles zero codes.


    HISTORY  (Where's this `TCR' format from anyway?)

Ian Young originally designed a format called ZVR for a Psion text file 
viewer - see http://ww.rats.demon.co.uk/zvr/, where there's also a program 
called ZVRZ for the de/compression (including source).  Unlike most 
compression formats, which need to be decompressed right from the beginning 
each time, with ZVR you can restart anywhere, which is obviously much better 
for a text viewer.  Compression is still quite good though - average saving 
is about 50%, as compared with (very roughly) 65% for ZIP.

When Barry Childress (73510.1420@compuserve.com) came to write a text file 
viewer for the Psion Series 3, Reader3 (available on 
http://3lib.ukonline.co.uk), he adapted the ZVR format slightly to make TCR 
(Text Compression for Reader). He also included a compression program called 
TCReader - this is very much faster than Ian's ZVRZ, but only runs on a PC.

The format has become quite popular in the Psion world, so that when Jean-Luc 
Damnet (jldamnet@pca.fr) wrote his VReader5 viewer for the Series 5, he also 
included TCR support.  (Currently at http://ady.net/psion/files/vr201.zip)

Meanwhile, although you could use TCR files on a Psion, you needed a PC to 
create them, which was very awkward me as I've an Atari instead.  Barry 
Childress wasn't keen on letting me have his source code, so in desperation I 
decided to write my own!  (And don't I wish I hadn't :)  Although this 
version uses some ideas from Ian's original ZVRZ, and some from interesting 
discussions with pmaud@cix.co.uk, it's all my own work, and (in my very 
biased opinion) is better than previous ones :)  So, with thanks to Ian and 
Peter, and apologies to Barry, here's a version you can compile for your 
machine.

But why, I hear you ask, didn't I write a Psion version?  Speed and 
convenience, but mainly coz I haven't got a C compiler for my Psion!  
(And of course, I can't use the SDK as I haven't got a PC... which is where 
we came in.  Does some enterprising soul with a C SDK want to try porting it?)


    RESTRICTIONS  (What can I do with this?)

The code and executables are freeware - use and share them freely.  I 
_encourage_ you to compile versions for your own machine, read them for 
ideas, let me know of any bugs or improvements, etc.  However: you're not 
allowed to make any money from them (except for reasonable distribution 
costs, etc.), and if you do use my source code you must credit me and let me  
know!  Reasonable?  (Monetary appreciation would be nice, too :)


    CONTACT  (Who do I send money to?)

Questions, comments, bugs, ideas, etc. to:

        Andrew Giddings

        gidds@cix.co.uk

        1 Pavilion Close
        Southend-on-Sea
        Essex  SS2 4TZ
        UK


    GRATUITOUS GREETINGS

Thanks to Ian Young, Barry Childress and Jean-Luc Damnet for their programs.

Thanks and Hi to Peter Maud for explaining ZVRZ to me, Steve Litchfield for 
putting this on 3-Lib, and Stephan Hradek for testing and ideas.


        Andrew Giddings 20/Feb/1999.