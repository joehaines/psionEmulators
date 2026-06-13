Readme file for Jabp version 1.12
=================================


Computers which can run Jabp
----------------------------

Here is a list of platforms on which Jabp has been tested:

Windows PCs (640 x 480 screensize)
Apple Mac (640 x 480)
Solaris workstations (640 x 480)
Compaq IPAQ running Jeode Java (240 x 320)
Sharp Zaurus running Jeode Java (240 x 320)
Nokia 9210/9290 (640 x 200)
Sony Ericsson P800, P900, P910 (218 x 320)
Psion netBook / Series 7 (640 x 480)
Psion 5mx (640 x 240)

Jabp does not run on smartphones running the MIDP version of Java.


Installing Java
---------------

In order to run Jabp, your computer has to have a Java runtime which conforms to or exceeds the Personal Java standard.  Brief details are as follows:

PCs running MS Windows: some PCs may already have Java installed.  To check, try double-clicking the file Jabp.jar in Windows Explorer.  If Jabp does not run then go to the Java homepage (http://java.sun.com) and download the Java Standard Edition (J2SE).  You only need the JRE runtime environment, not the full SDK.  It is free.

Symbian computers: Older Symbian computers may have the Java runtime distributed on a CD supplied with the machine.  Follow the installation instructions.  Smartphones such as the P800/P900 have a Java runtime pre-installed in ROM.

iPAQ with Pocket PC 2002: Install the Jeode Java runtime, supplied with the machine.

Sharp Zaurus: The Jeode Java runtime is already installed.


Contents of distribution zip
----------------------------

The distribution zip should contain the following files:

* Jabp.jar (this is the main program)
* JabpPsion.zip (a "skeleton" installation for Psion computers which creates the appropriate directories and an icon in the Extras section - you must also copy Jabp.jar as explained below)
* JabpCommunicator.zip (a "skeleton" installation for "Communicator" computers such as the Nokia 9210/9290 which creates the appropriate directories and an icon in the Extras section - you must also  copy Jabp.jar as explained below)
* JabpUIQ.zip (a full installation for "UIQ" computers such as the Sony Ericsson P800 which creates the appropriate directories and an icon in the Applications section
* JabpZaurus.zip (a "skeleton" installation for the Sharp Zaurus machines which creates the appropriate directories and an icon in the Applications section - you must also copy Jabp.jar as explained below)
* JabpIPAQ.zip (a shortcut for the HP iPAQ which will create an entry in the Programs>Jeode section - you must also copy Jabp.jar as explained below)
* JabpUserGuide.htm (the user guide in html format)
* JabpChanges.txt (recent changes to Jabp)
* ReadMe.txt (this file)
* P800_ReadMe.txt (supplementary notes for the P800)


Installing Jabp
---------------

Once you have a Java runtime on your machine, you can install Jabp as follows:

PCs running MS Windows: copy the file Jabp.jar to any directory.  Double-clicking on this file in Windows Explorer will launch Jabp.  You can set up a Windows shortcut to launch the program from the main screen.

Symbian computers: if you have a Psion machine (netBook, Series 7, Series 5mx, Revo Plus) then unzip JabpPsion.zip.  If you have a Communicator machine (Nokia 9210/9290) then unzip JabpCommunicator.zip.  Install the file Jabp.sis and then copy Jabp.jar to the directory \system\apps\Jabp\ on the c: or d: drive where you installed the sis file.  Note: once Jabp has been installed, then for subsequent upgrades you only need to copy the new Jabp.jar file.  You do not need to re-install the Jabp.sis file.

Special note for UIQ machines (for example the P800): I have included a full implementation since there is no easy way to manually copy Jabp.jar to the correct directory.

iPAQ running Pocket PC 2002 with Jeode Java runtime: Create a directory c:\Java\ on your Pocket PC and copy Jabp.jar there.  Follow the instructions supplied with the Jeode runtime to create a shortcut that can launch the program.  The command line for the shortcut is as follows:

18#"\Windows\evm.exe" -cp \java\Jabp.jar Jabp.Jabp

You can place Jabp.jar in a different location than c:\Java\.  If you do, then change the command line accordingly.  Alternatively, just unzip JabpIPAQ.zip and copy Jabp.lnk to the IPAQ in the c:\Java directory.  Note: once Jabp has been installed, then for subsequent upgrades you only need to copy the new Jabp.jar file.  You do not need to re-install the Jabp.lnk file.

Sharp Zaurus with Jeode Java runtime: unzip the file JabpZaurus.zip and install the file Jabp.ipk.  Then using File Manager copy Jabp.jar to the machine in the directory:

/opt/QtPalmtop/java/jabp

or

/mnt/cf/opt/QtPalmtop/java/jabp  (if you installed to Flash disk)

or

/mnt/card/opt/QtPalmtop/java/jabp  (if you installed to SD card)

Note: once Jabp has been installed, then for subsequent upgrades you only need to copy the new Jabp.jar file.  You do not need to re-install the Jabp.ipk file.


System requirements
-------------------

Jabp requires about 700K of free memory to run, in addition to the memory required for the Java runtime.  For example on a Nokia 9210, a total of about 3 megabytes is needed.


Known problems
--------------

There are a number of minor problems mainly due to differences and/or bugs in the Java runtimes on the different machines.  Please refer to the User Guide for details.


File compatibility
------------------

Jabp data files can be freely moved between different machines - the file formats are identical.  For further details, see the User Guide.


Questions or comments
---------------------

I can be contacted at the following email address:

malcolm@freepoc.org


Enjoy the program :-)


Malcolm Bryant
August 2002