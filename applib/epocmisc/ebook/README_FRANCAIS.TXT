EBook v2.3 pour EPOC
--------------------
EBook est un visualiseur de livres électroniques (e-text/e-book). Il accepte les documents DOC Palm (Aportis), qui sont généralement utilisés sur les machines Palm Computing, les documents TCR documents ainsi que les fichiers texte standards (c.a.d les fichiers ".txt" générés par tous traitements de texte).

Caractéristiques :
* Supporte les fichiers DOC Palm compressés ou non (.prc & .pdb)
* Supporte les documents TCR
* Suporte les fichiers texte (.txt)
* Défilement automatique
* Navigation par défilement
* Copie vers Presse-papier et vers fichier
* Signets étendus
* 80 signets par document
* Système de reconnaissance de fichiers DOC Palm & TCR sur les machines ER5 (Revo/Mako/MC218/S5mx/S7/netBook)
* Signets style Palm DOC
* Support du Code page 850
* Support des polices EPOC (.gdr) - qui autorisent l'utilisation de polices
  "non-latin" comme le Russe, etc.
* Nécessite peu de mémoire, quelle que soit la taille du document affiché
* Ecrit en C++ pour une vitesse optimum

  
Veuillez lire le fichier LICENSE.txt avant utilisation.


Installation
------------
Pour installer l'application, merci de lire le document INSTALLATION.TXT

Instructions d'utilisation
--------------------------
Une aide et fournie avec l'application.

Enregistrement
--------------
EBook pour EPOC est shareware. L'enregistrement coûte £10 UK Pounds. 


Création de fichiers DOC Palm
-----------------------------
J'ai porté l'utilitaire populaire MAKEDOC pour EPOC qui est disponible sur le site de EBook.

Il nécessite eshell.exe et STDLIB, disponibles sur le même site, pour tourner.

Il existe plusieurs autres utilitaires analogues sur différentes plate-formes pour créer des fichiers Aportis.


Problèmes
---------
* EBook a été conçu pour une lecture de haut en bas. La mise en forme peut devenir un peu désordonnée lorsque vous revenez en arrière. 
* L'ouverture de fichiers EXE, DLL, etc. peuvent planter le programme. La solution... evitez !
* L'information sur la position de document peut être info may be imprécise avec des documents TCR, à cause d'un manque d'information sur la taille du document original dans ce format.
* Les fichiers TCR corrompus peuvent planter EBook lors de la première ouverture.
* La justification verticale du texte est impossible à cause d'un bug
d'EPOC.


Changements depuis la v2.2
--------------------------
Nouveautés:
* Vitesse x4 sur Series7 et netBook.
* Texte justifié.
* Marge définissable.
* Dialogues améliorés sur Series 5 et Series 7/netBook.
* Hot key évitant les entêtes Guttenberg et etext. Pressez Maj+Ctrl+F pour éviter les entêtes.
* Navigation depuis la dernière position.

Changements depuis la v2.1
--------------------------
Nouveautés:
* Une version unicode supportant le Japonais, le Coréen et le Chinois est également disponible dans un fichier SIS à part. Merci de vous rendre sur le site de EBook.
* Le menu derniers fichiers rappelle les quatre derniers documents ouverts.
* En laissant le "Dossier Livres" vide dans les préférences, le dernier dossier ouvert sera maintenant mémorisé.
* Ajout de la date de modification de fichier au dialogue Inofs Document, et retrait des infos d'enregistrement.

Changements depuis la v2.0
--------------------------
Nouveautés:
* renomme les signets
* le raccourci de navigation des signets a changé en une seule touche
* la tempo de défilement utomatique est de .5 secs

Améliorations:
* ajout d'un espace manquant entre les paragraphes lors du formatage "Grouper par paragraphe"
* suppression de la ligne vide lors de l'affichage "Respecter original"

Changements depuis la v1.2
--------------------------
Nouveautés: 
* amélioration de la navigation dans les signets
* lecture des signets Palm DOC
* barre de défilement
* copie vers presse-papiers et fichier
* défilement automatique
* support du "code page 850"
* Suport EPOC des polices (.gdr) - qui autorisent l'utilisation de polices "non-latin" comme le Russe, etc.
* préférences d'affichage améliorées

Améliorations: 
* perfectionnement de la recherche
* les documents sont affichés depuis le haut de l'écran
* ne rescanne pas les signets si un signet est invalide
* suppression de "trouver suivant" si aucune chaîne n'est spécifiée
* plantage occasionnel avec les documents TCR


Bugs
----
Merci d'envoyer toute remarque et rapport à ebooksupp@bigfoot.com

http://www.geocities.com/ebookepoc/


Cheers
Simon Quinn, UK
