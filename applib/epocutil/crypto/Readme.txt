Introduction
------------

Welcome to Crypto!

Crypto is a encryption utility which may wipe out source
files, launch decrypted files and automatically re-encrypt
them after modification.

After I've read 'Murgan's Musings' in Palmtop's issue 18
I've wondered how difficult it is to write an encryption
utility for the EPOC platform. - I know that such a
program already exists, but I was more interested in the
making than in the result.

I'm using Crypto myself for managing a simple database
with tons of logins, passwords and other figures I'm
supposed to know by heart. A perfect job for my little
brain extension - my beloved Series 5.


Release Information
-------------------

V1.14 Production - February 14, 2000

What's new?

- Files are launched asynchronously so Crypto is not 
  blocked anymore and offers a 'go to file' button
  
- Most important buttons are arranged to the right

- Crypto recognises newer Crypto files and the need of a
  software update
  
- Folders and files containing space characters are fully 
  supported
  
- Dense packed dialogs to improve the appearance on smaller 
  machines, e.g. the Revo
  
- A missing password entry does not cause a crash anymore

- Pressing the test button does not cause a crash anymore


Compatibility to prior releases

- This release is fully compatible to prior releases

- The Crypto file structure has not been changed

- Files encrypted with Crypto V1.14 may be decrypted with
  prior releases too


Features

- File encryption and decryption using the secure
  RC4 algorithm (256-bit)

- Wipe out source files to make sure an undelete is useless

- Cancel and resume during encryption, decryption and wipe
  file

- Launch support for all file types (for files without a
  EPOC header - like zip archives, plain text files or
  graphics - any installed program may be selected at
  encryption time to open the file)

- Compacting data and agenda files before encrypting

- Automatic re-encryption

- Detects the need of compacting and re-encryption

- General preferences for encryption mode, source file
  treatment after encryption and statistics.

- Runs on 
    - EPOC Release 3 (e.g. Series 5)
    - EPOC Release 5 (e.g. Series 5mx)
    - EPOC Emulator Release 5 (WINS)
    
- Available in German, English, French, Italian and 
  Brazilian Portuguese


Installation
------------

Crypto is distributed as .zip file containing the following
.sis installation file:

- Crypto.sis          for EPOC Computer (e.g. Serie 5)
- CryptoEmulator.sis  for EPOC Emulator R5 on a PC

To install Crypto from your 32-bit MS-Windows PC, double
click on the .sis file and follow the instructions.

To install Crypto from your EPOC machine or the EPOC
Emulator, launch the .sis file and follow the 
instructions.

After the installation a new icon should appear in your
extras bar.

Grap a copy of the appropriate installation program from my
web site http://salvis.com/ when your PsiWin release or
your EPOC machine does not support .sis files.

Important: The old EPOC Emulator Release 3 is not supported 
*********  anymore. Please download the new Emulator from 
           http://www.epocworld.com/ - the OPL or JAVA SDK
           are relatively 'small' and include a full working
           Emulator. They may be downloaded for free by
           EpocWorld members and the membership is free too.
           The new emulator is definitely better than the
           old one and is highly recommended!


Translation
-----------

The following persons have translated Crypto for free.
Thank you very much!

  - Gérald Aubard - French. 
    Visit Gérald's web site at http://www.psionist.com/
    
  - Matthias Neisser - Brazilian Portuguese. 
    Visit Matthias' web site at
    http://www.geocities.com/SiliconValley/Platform/3613/
    
  - Salvo Micciché und Filippo Zerboni - Italian. 
    Visit Salvo's web site at 
    http://www.scicli.com/psion/ und Filippo's 
    web site at http://utenti.tripod.it/sit5/
    
The German and English versions are maintained by myself.


Feedback
--------

Do you have bug reports, enhancement requests or comments?

Your Feedback is very appreciated.

Thank you!

Philipp Salvisberg

e-mail: philipp@salvis.com
www:    http://salvis.com/
