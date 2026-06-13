
        Readme for WordCrypt v2.0
        Copyright 1999 UK Adam Brinley Codd


Introduction
============

  WordCrypt is a program for people who have forgotton the password
  on their Psion Word files.

  This version asks for the first 9 letters of the document, and
  uses that to decrypt the document. Unlike the password, it is case
  sensitive, but you don't need to get it exactly right to decrypt
  the document, therefore you can guess it.

  It might be the title of the document for example.

  You must not use WordCrypt on someone else's documents unless they
  give you permission.


Installing and Running
======================

  Copy the WORDCYPT.OPA file into your APP directory (you may need to
  create one), then go the system screen. Position the cursor to the
  right of where you want the icon and press <PSION> and <I>. Choose
  WORDCYPT.OPA from the list.

  An icon will appear, similar to the Word icon, and underneath it
  will be a list of files in your WRD directory. Highlight one of
  them and press <ENTER>.

  Next, you will be asked to enter the first 9 letters of the
  document. It will then decrypt the first 500 letters, and will ask
  you if it is correct. If not, you may choose another 9 letters.

  When you get the right 9 letters, you can decrypt the entire
  document either into a plain text file (which takes quite a long
  time) or as a Word document, which means the formatting will be
  kept.


Uninstalling
============

  Position the cursor underneath the WordCrypt icon in the system
  screen, and press <PSION> and </>.

  Then delete the WORDCYPT.OPA file from your APP directory.


Issues
======

  Known problems and bugs:

  I don't know this for sure because I haven't tried it out, but I
  think if you have a paragraph longer than 16k (just over 16
  thousand characters) and try to save it as a text file, WordCrypt
  will crash with an 'Overflow' error.

  To get around this, I recommend you save it as a Word file, and
  then from within Word save it as a text file.

  Also, sometimes it may say 'There is not enough space to store the
  file in memory'.  Usually just quitting the program and trying
  again solves this problem.  I don't know why it occurs though.




Misc Information
================

  Details on how this program works are available from my web site
  (see below).

  You must not use this program to see other people's word documents
  without their permission.

  WWW Site: http://w3.to/adamjbc
  E-Mail  : adamjbc@hotmail.com

  If you want to see the source code to this program, don't bother
  RevTranning it because most of the variables are LOCAL so it will
  be hard to understand. E-mail me and I'll give you the source.


History
=======

  Version 1.0
    Initial release, fully working but very slow converting to plain
    text

  Version 1.1
    Speeded up convertion to plain text after a lot of fiddling
    trying to get the buffer OS call to work :-)

  Version 2.0
    Fixed the problem of it not working on files over about 20k by
    changing the variable the length of the data is to a long (rather
    than a short integer).

    It also meant I had to change a whole lot of other stuff which I
    hope made the program better and faster.


  Here's a brief description of what I did leading up to Version 1.0:

  I started by investigating the encryption commands built into the
  Psion.

  The password is used to produce a 16 bit encryption key, and then
  something is done to this key, and it is placed inside the word
  file. I couldn't work out how to get the original key from the one
  inside a word file, so I took a different approach.

  I realised that I could reverse the encryption process, and if I
  knew the original text, I could get the key, and use then to
  decrypt the rest of the document.

  I wrote a small program to test this theory, but I came accross a
  problem, but it was sloved easily.

  Next, I wrote a program which decrypted the text from a word file
  using the first 9 letters and then wrote it to a text file.

  However, the text in word files isn't stored as plain text, e.g.
  carrige returns are replaced with 0x00 and there are breakable and
  non breaking hyphens and spaces to deal with, so I changed the
  program to take the decrypted text, and convert it to normal ASCII
  as it wrote it to the file.

  Then, I added the option for saving it as a Word File which was
  very easy, compared to converting it to plain text, as I just
  inserted the decrypted data back into the document, and changed the
  flags within the document which say it's encrypted.

  Finally, I added a load of UI code to make it look nicer and easier
  to use, and added better error handling. I also spent quite a while
  making the icon, it's the normal Word icon, but with a padlock
  (taken from the password dialog box) instead of the fancy Q. If you
  can make a better icon, please do so and e-mail it to me.


Future
======

  In future versions, if there is demand, I will add the following
  features:

  o Better routine for guessing the first 9 letters (i.e. showing
    you which letters you got wrong)
  o Maybe automatic guessing of the first 9 letters

  If you can think of anything else, e-mail me.

