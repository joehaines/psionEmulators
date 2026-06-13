Readme file for jCompile version 1.02
=====================================


Disclaimer
----------

jCompile is a Personal Java compiler for the P800 smartphone and Psion computers such as the netBook / Series 7 / Series 5mx.  It will not compile any other Java variant such as MIDP Java.  This software is intended for experienced users.  You use the software entirely at your own risk.


Acknowledgements
----------------

We acknowledge the trademarks and rights of Sun Microsystems and Symbian Ltd. A special word of thanks to Frank Bodmann, who wrote Javac for the Psion Series 5 on which this program is based.


Contents of distribution zip
----------------------------

The distribution zip should contain the following files:

* qfilemanP800.sis - File Manager by Symbian Ltd (P800 only)
* redirectP800.sis - Redirect by Symbian Ltd (P800 only)
* sweeperPsion.exe - Java Sweeper by Symbian Ltd (Psion only)
* jTextP800.sis - jText text editor by FreEPOC (P800 version)
* jTextPsion.sis - jText text editor by FreEPOC (Psion version)
* jCompileP800.sis - Java compiler by FreEPOC (P800 version)
* jCompilePsion.sis - Java compiler by FreEPOC (Psion version)
* jRunP800.sis - Java wrapper program by FreEPOC (P800 version)
* jRunPsion.sis - Java wrapper program by FreEPOC (Psion version)
* jCompileUserGuide.htm (the user guide in html format)
* jCompileUserGuide.pdf (the user guide in pdf format)
* ReadMe.txt (this file)


Installing jCompile
-------------------

Please install all the SIS files above.  As an experienced user, we assume you know how to do this :-)  For instructions on sweeperPsion.exe please refer to the User Guide.


Important - before using the jCompile program
---------------------------------------------

Before using the jCompile program, you must obtain the necessary Java class files from Sun's Java website.  I would have included these directly in my distribution, but I do not want to contravene Sun's Java license agreement.  So I'm sorry, but you will have to do a little bit of work yourself ;-)

Firstly, get the Java 1.1.8 SDK from the following address:

	http://java.sun.com/products/archive/jdk/1.1.8_010/index.html

and install it on your PC.  You will find that it has created the directory c:\jdk1.1.8.  Now find the file c:\jdk1.1.8\lib\classes.zip.  Open this file with a standard zip tool such as Winzip.  Sort the contents of the zip by pathname.

Secondly, extract all files which match the following:

	sun/tools/asm/*.class
	sun/tools/java/*.class
	sun/tools/javac/*.class
	sun/tools/javac/resources/javac.properties
	sun/tools/tree/*.class

to an empty folder on your PC.  Now create a new zip file named compiler.zip and move the entries into this zip, taking care to preserve the original directory structure and filenames.

Thirdly, extract all files which match the following:

	java/awt/*.class
	java/io/*.class
	java/lang/*.class
	java/net/*.class
	java/security/*.class
	java/text/*.class
	java/util/*.class

to an empty folder on your PC.  Now create a new zip file named missing.zip and move the entries into this zip, taking care to preserve the original directory structure and filenames.  Rename compiler.zip and missing.zip to compiler.jar and missing.jar respectively.

Now you need to transfer compiler.jar and missing.jar to your P800.  Use File Manager to locate them as follows:

* compiler.jar goes in directory \system\java\ext\ on either c: or d: (create this directory if it doesn't already exist)
* missing.jar goes in directory \system\apps\jCOmpile\ on the disk where you installed jCompile.

You can now uninstall the JDK 1.1.8 from your PC - we don't need it any more.


Using the jCompile program
--------------------------

Please refer to the User Guide in the distribution zip


Questions or comments
---------------------

I can be contacted at the following email address:

malcolm@freepoc.org


Enjoy the program :-)


Malcolm Bryant
August 2003
www.freepoc.org