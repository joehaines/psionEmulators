****  LETPRINT  Version 1.0    ****
Copyright  Jean-Philippe PASQUIER  -   Tedco  1996


================
=  English version  =     (Français, voir plus loin)
================


PSION softwares are not able to handle graphical prints. For some applications printing graphics could be usefull.

LETPRINT has been writen for that purpose. 

It had been designed on PSION 3a, but should run on 3c and would'nt on Siena.

I - Shareware :
LETPRINT is a shareware. It means that :
	- If, after a period of test, you decide to keep it, you have to contribute to the author (see "Shareware" in settings menu).
	- You can freely distribute this software unmodified and including every files of the package.

II - Installation
3 files are in the LETPRINT package :
		- Letprint.opa
		- Letprint.rsc
		- Letprint.dat
Installation steps :
	1 - copy Letprint.opa in the APP\ (M:\APP\) directory
	2 - Create a sub-directory in APP\ : LPR\
	3 - Copy Letprint.rsc in the APP\LPR\ directory
	4 - Copy Letprint.dat in the APP\LPR\ directory
	5 - Install Letprint.opa  (Psion-I)

III - Using LETPRINT:
III-1 - Projects
A project is made of : general settings (port, printer, ...) and objects to be print.
It is possible to store on disk many projects. The "File" menu allows you to manage it : "Open", "Save", "Save as".
III-2 - General settings
The "Settings" menu allows you to set up a few parameters : paper size, orientation, port, printer.
III-3 - Objects to be print
With the "Edit" menu you can add, edit or delete an object. An object is characterized by : an internal name, the picture file name (.PIC format, pictures standard format on Psion), his position en X and Y (reference=top and left of the page), his scale in X and Y (maxi 1000% -> x10) and his teint in %.
III-4 - Printing
By default, LETPRINT is waiting, ready to print. You just have to setup LETPRINT and leave it open, printing will be launch by the original application (text, ...).
To print using LETPRINT, in the original apllication you have to print in a file named "P.lis" in the root directory (LOC::M\P.lis). That's all !

IV - Restrictions, others, ...

1 - Actually, only HPLaserJet and HPDeskJet are working.
2 - Teint setting is only for LaserJet.
3 - Both grey and black pictures of the .PIC files are printed : grey at 50% of the teint and black at 100%.
4 - The amount of printing time is a fonction of the size of the objects. Be careful !
5 - In case of problem with an old LaserJet, try the DekJet driver.
6 - The DekJet driver use the PSION's disk as a spooler.
7 - It's impossible to print graphics in landscape orientation on a DeskJet ! Sorry, HP hardware limitation.


For any comments, encouragements, bugs reports, ... : jppasquier@kagi.com  or  jppasquier@calva.net

JP PASQUIER  -  TEDCO
web : www.kagi.com/jppasquier/









==================
=   version Française = 
==================


Les applications du Psion ne savent imprimer que du texte seul. Pour certaines applications, professionnelles notamment, une impression graphique serait utile.

LETPRINT a été conçu dans ce but. Il permet d'ajouter dessins et logos de votre choix à n'importe quelle impression standard Psion. Parmis les applications : lettre avec papier à en-tête de votre société, devis imprimé chez un client avec votre signaletique, ...

Conçu sur PSION 3a, il devrait fonctionné sous 3c mais ne fonctionne pas sous Siena.

I - Shareware :
LETPRINT est diffusé sous la forme du shareware. Si vous désirez l'utiliser vous devez acheter le logiciel auprès du créateur. Modalités : voir dans l'application :  menu règlages/shareware. En attendant que votre version soit enregistrée, le mot "shareware" sera ajouté lors de l'impression. Vous pourrez néanmoins tester toutes les fonctionnalités et la compatibilité avec votre imprimante.

II - Installation
Le package LETPRINT comprend 3 fichiers :
		- Letprint.opa
		- Letprint.rsc
		- Letprint.dat
Suivre les étapes suivantes :
	1 - copier Letprint.opa dans le répertoire APP\ (M:\APP\)
	2 - Créer un répertoire LPR dans APP -> APP\LPR\
	3 - Copier Letprint.rsc dans le répertoire APP\LPR\
	4 - Copier Letprint.dat dans le répertoire APP\LPR\
	5 - Installer Letprint sur l'écran système (menu : Appli/Installer)

III - Utilisation:
III-1 - Editions
Une édition est caractérisée par des réglages généraux (port, imprimante, ...) et des objets à imprimer.
Il est possible de conserver sur disque plusieurs éditions. Le menu "fichier" permet de les gèrer avec "ouvrir", "sauver", "sauver sous".
III-2 - Réglages généraux
Le menu "réglages" permet de règler les paramètres suivants : format papier, orientation, port, imprimante.
III-3 - Objets à imprimer
Le menu "éditer" permet d'ajouter un nouvel objet, d'éditer un existant ou de supprimer. Chaque objet est déterminé par : un nom interne, le nom du fichier desin (format .PIC, format des fichiers dessins sur PSION), sa position en X et Y par rapport à l'angle en haut à gauche de la feuille, l'échelle en X et Y (maxi 1000%, soit x10) et la teinte en %.
III-4 - Impression
Par défaut, LETPRINT est en attente, pret à imprimer. Une fois l'ensemble des réglages efectués, vos interventions sur LETPRINT sont terminées, l'impression s'effectuera à partir de l'application source (texte, tableur, ...). LETPRINT doit simplement être laissé en fonctionnement.
Pour imprimer en l'activant, dans l'application source il faut imprimer dans un fichier nommé : "P.lis" dans le répertoire racine. Par exemple, dans l'application "texte" : menu spécial / config imprimante / périphérique d'impression:taper tab / sélectionner fichier / nom:taper tab / selectionner la racine du disque / entrée / spécifier comme nom : "P.lis" (LOC::M:\P.lis). Pour lancer l'impression, faire menu spécial / imprimer. LETPRINT s'active automatiquement.

IV - Divers

1 - Actuellement, seules les imprimantes HP LaserJet et DeskJet sont pilotées.
2 - Le réglage de la teinte fonctionne seulement pour les Laser.
3 - Les images noires et grises des fichiers .PIC sont imprimées : le noir à 100% de la teinte définie et le gris à 50%.
4 - Le temps d'impression est directement fonction des tailles des objets à imprimer. Attention ...
5 - En cas de problème avec une vielle LaserJet, essayer le driver de la DeskJet. 
6 - Le driver DeskJet utilise le disque Psion comme spooler.
7 - L'imprimante DeskJet n'autorise pas l'impression de graphiques en orientation "paysage". Désolé.


Pour commentaires, encouragements, bugs reports, ... : jppasquier@kagi.com  or  jppasquier@calva.net

JP PASQUIER  -  TEDCO
web : www.kagi.com/jppasquier/

