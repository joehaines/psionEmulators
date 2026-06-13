* HOL - calculate holidays and write to agenda file

** Files in the distribution

This distribution should contain:

  HOL5.SIS			The application
  HOL5DEF.SIS			The holiday definition files
  HOL5SRC.SIS			The source code of HOL5
  Agenda2.SIS
  SystInfo.SIS
  Changes.txt			Revision History
  License.txt			Licensing Conditions
  Readme/LiesMich/LisezMoi.txt	This file in English, German and French


This distribution of Hol5 contains a set of holiday definition files for 
different countries, regions and religions.

The holiday definition files are text files. They may be adapted to
specific needs by using a text editor (e.g. RMRText from
www.rmrsoft.com) or by importing to an exporting from the S5
word application. See the help file for details.

The following holiday definition files are contained in HOL5.SIS:

  Ar.hol		Argentina
  ArUy.hol		South American mix
  CiWc.hol		Church in Wales, Welsh/Cymraeg
  CiWe.hol		Church in Wales, English
  CofI.hol		Church of Ireland
  De.hol		Germany
  Dk.hol		Denmark
  Example.hol		Example holiday file
  Fi.hol		Finland
  Fr.hol		France
  Hebrew.hol		Jewish holidays
  Hebrew Months.hol	Jewish Months
  Islamic.hol		Islamic holidays
  It.hol		Italy
  Moon.hol		Moon Phases
  Nl.hol		Netherlands
  No.hol		Norway
  Se.hol		Sweden
  Sf.hol		Switzerland (French)
  Sg.hol		Switzerland (German)
  Si.hol		Switzerland (Italian)
  Uk.hol		UK
  Us.hol		USA
  Uy.hol		Uruguay

You can delete those that you never use anyway. The holiday
definition files are installed to /Systems/App/Hol5/Hol/ on drive C: or D:.

** Translations and Enhancements

Hol5 has been translated to several languages. There is an 
easy-to-use translation kit that you may get on request.

NEW TRANSLATIONS, CORRECTIONS, ENHANCEMENTS, 
AND NEW HOLIDAY FILE DEFINITIONS ARE HIGHLY APPRECIATED.
PLEASE CONTACT ME.

** Installation

Before anything else, IF you modified a holiday definition file from a
previous version of Hol5, back it up or verify that it's name does not clash
with any other file in this distribution that would overwrite it !

If you unzipped Hol5 OK, you should have ended up with
the files listed at the beginning of this file.

(Experts)
   If you know what a .SIS file is, just double-click on each one in your    
   Windows Explorer or tap on it on the Series 5 and you'll be installed 
   in no time.
   Note that if you've already installed any of the .SIS upgrades, you
   don't need to do so again. The "Hol5.SIS" is the program itself and
   *must* be installed. The holiday definition files are included in
   "Hol5Def.SIS" which is invoked by "Hol5.SIS" during the installation
   process.

(Beginners)
   If you *don't* know what a .SIS file is, this must be one of the very 
   first third party applications you've ever installed. Don't worry, it's all 
   quite painless:

   IF you have the 'older' PsiWin 2.01 or if you've downloaded Hol5 
   directly onto the Psion 5, the best way to proceed is to copy the 
   "Hol5.SIS" file into a suitable place on your Psion. If the file appears 
   on the system screen with a question mark beside it, this just means 
   you haven't yet upgraded your Series 5 to accept EPOC install files. 
   Grab a copy of the special INSTEXE.EXE program from 
   http://3lib.ukonline.co.uk/find/instexe.exe and tap on it on your Psion.
   You should now see a nice little icon beside the "Hol5.SIS" file and
   can tap on it to complete your installation. *Repeat* the process for 
   "Agenda2.SIS" and SistInfo.SIS, if you haven't already done so, as 
   these extra files contain the magic ingredients needed to make Hol5
   work.

   IF you have PsiWin 2.1, just double-click on the "Hol5.SIS" file to
   launch EPOC Install on the PC itself and to complete the installation   
   of Hol5. *Repeat* the process for "Agenda2.SIS" and SistInfo.SIS, if 
   you haven't already done so

   Hol5Src.SIS is not needed for the program to work - but maybe you
   are interested in the source code?

   If all went well, you should now have a new "Hol5" icon on your 
   Extras bar.

** Removal

As any program installed by .SIS files, Hol5 is best removed by 
selecting the Add/remove option of the Control panel. Then select 
the program name, and hit the remove button.

** Upgrading from Psion 3 to Psion 5
 
The holiday definition file format of Hol5 for the Series 5 is 
backward compatible to the format of Hol on the Series 3, 
BUT the character codes used by the Series 5 differ from those 
used by the Series 3 for all accented or "special" characters.
So if you want to keep using the same holiday definition files 
you had on your S3, remember to edit them.
(e.g. import them as text into the S5 Word application, 
edit and export again in text format)


** License and acknowledgements

See the help file.

** Feedback

No registration is necessary, but I would appreciate if you send me a
mail or postcard if you use the program. If you try the program and
*don't* use it, please send me an e-mail and tell me why.

If you write or modify a holiday file, please send it to me and I will
include it in the distribution so more people can benefit from your
effort. Before you start writing a file, I suggest you contact me to
check that nobody else have already started writing the same file.

You can reach me at:

  E-mail:
                Reto.Beeler@Switzerland.org
  Snail-mail:  
                Reto Beeler
                alte Bernstrasse 41
                CH-3075 Rüfenacht
                SWITZERLAND

The latest version of HOL can be found at:
  http://www.planetepoc.com/hosted/jldamnet/
On this site you can find also intermediate files, containing new or 
updated definition files received between Hol5 releases.

** Configuration

Holidays are defined by writing a holiday definition file. For details open
the help dialog of Hol5.


** Limits

(See also the constant definitions in the source code)
Line length in holiday file: 255 characters
Number of holiday definitions: 101
Number of aliases: 10
Number of characters in an alias: 33
