****  SYNCHRO  Version 3.2    ****
Copyright  Jean-Philippe PASQUIER  -   Tedco  1997



SYNCHRO is a file synchronization program for PSION.
It's purpose is to copy files back and forth between your PSION and a desktop computer (Mac or PC), but it can be used to synchronize files between any two disks or directories of the PSION.

It had been designed on PSION 3a, but should run on 3c and would'nt on Siena.

I - Shareware :

SYNCHRO is shareware. It means that :
	- If, after a period of test, you decide to keep it, you have to contribute to the author (see "Shareware" by typing W).
You can register by 3 differents ways :
	1 - via internet : $10, with a credit card : http://order.kagi.com/?XC
	2 - via mail : send 50 French Francs (checks or bills) and your name and adresss to : 	J.Ph. Pasquier
			32, rue des Fauvettes
			67380 LINGOLSHEIM
			FRANCE
	3 - via Compuserve : $10, GO SWREG. The reg ID is 15918

	- You can freely distribute this software unmodified and including every files of the package.


II - Installation

6 files are in the SYNCHRO package :
		- Synchro.opa
		- Synchro.rsc
		- Synchro.dat
		- Synchro2.dat
		- Synchro3.dat
		- Synchro4.dat
Installation steps :
	1 - copy Synchro.opa in the APP\ (M:\APP\) directory
	2 - Create a sub-directory in APP\ : SYN\
	3 - Copy Synchro.rsc in the APP\SYN\ directory
	4 - Copy Synchro.dat in the APP\SYN\ directory
	5 - Copy Synchro2.dat in the APP\SYN\ directory
	6 - Copy Synchro3.dat in the APP\SYN\ directory
	7 - Copy Synchro4.dat in the APP\SYN\ directory
	8 - Install Synchro.opa  (Psion-I)


III - Using SYNCHRO:

III-1 - Settings :

First, connect your Psion and run McLink on your Mac or PC (except if local and remote directories are on the Psion !)

SYNCHRO manage Batch Files wich contains 1 or more syncronization process. When you launch SYNCHRO for the first time, you will be asking for creating a batch file (name and description), then process(es) of this batch file.

Settings of a batch file :

1 - A filename
2 - A description (up to 25 car.)
3 - Process(es)

Settings of a process :

1- The first thing to do is to give a name to this process
2 - Then set up local and remote directories
3 - You need also to tell Synchro what is the remote system : Mac / PC / Psion
4 - You can tell SYNCHRO to quit all currents running programs. Usefull if you want to sync all the Psion : Agenda, for example, won't be copied if in use.
5 - The last thing to set up are the synchronization ways :
	* '2 ways' means that the process will work from Psion to Mac/PC and after from PC/Mac to Psion
	* 'Psion->Mac/PC only' means that it is a 1 way process.
	* 'Psion->Mac/PC only & delete old files' means that it is a 1 way process which will delete files and directories on the remote if only existing there.


III-2 - Synchronisation

1 - Run McLink on your Mac or PC (except if local and remote directories are on the Psion !)
2 - You can launch the synchronisation process. Type "L" (the 3-link management is automatic).


III-3 - General settings

In Settings/General, you can choose your language : english or french and the status of sound signal at the end of synchronization : on/off.


IV - How does it works ?

There is several case :
1 - File exists in local directory dut doesn't in remote one :
		=> copy file local -> remote
2 - File exists in remote directory dut doesn't in local one :
		=> copy file remote -> local
3 - File exists in local and remote directories :
	3.1 - local date > remote date AND remote date < last synchronization date
		=> copy file local -> remote
	3.2 - local date < remote date AND local date < last synchronization date
		=> copy file remote -> local
	3.3 - Both local and remote files had been modified since last synchronisation
		=> Problem. Synchro skip this file and tell you at the end of the process


V - Warning

* Synchro doesn't copy himself nor his directorie (APP\SYN\).
* English is not my mother tongue language, so be indulgent for translations !!



VI - Versions

1.1  -  03/97
	* At the first use, an error "invalid arg" was given ! . Bug fixed.

2.0  -  03/97
	* The detection of remote system was not sure, generating sometimes an connection error .
	* Synchro crashed when copying himself or another running programs.
	* The sort between directories and files was not good on remote system.

3.0  -  05/97
	* Allows a 1 way synchronisation if desired.
	* Management of batch file with multiple synchronization processes included. You can now synchronize your M:, A: and B: disks with a single key press !!

3.1  -  05/97
	* adding option : 1 way process which will delete files and directories on the remote if only existing there.

3.11  -  06/97
	* Little bug corrected

3.2  -  06/97
	* Another bug corrected



VII - Disclaimer

The author cannot be held responsible for any loss of data or damaged caused to you, your machine or anything else in near proximity caused directly or indirectly by SYNCHRO.




For any comments, bug reports, ...   jppasquier@kagi.com or jppasquier@calva.net


Jean-Philippe PASQUIER
web : www.kagi.com/jppasquier/

