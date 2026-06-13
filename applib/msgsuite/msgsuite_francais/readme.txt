Psion Series 5 Message Suite (1.10F)
====================================
Version française (No 029)
juin 1998

TRES IMPORTANT. Message Suite n'est pas compatible avec le logiciel 
"EasyFax". Si une version d'EasyFax est installée sur votre Series 5, 
vous devez impérativement la supprimer AVANT d'installer Message Suite. 
Pour obtenir les instructions nécessaires, voir la section "Comment 
procéder si EasyFax est installé sur le disque".

L'installation des logiciels de Message Suite ajoutera aussi de nouvelles 
icônes dans le Panneau de configuration du Series 5 (Accès distant, Modem). 
Vous devez impérativement modifier les paramètres dans le Panneau de 
configuration et au sein de l'application Email avant de pouvoir utiliser 
les applications Message Suite. Pour en savoir plus, voir le guide 
d'utilisation Message Suite.

Si vous avez déjà installé une version antérieure de Message Suite...
=====================================================================

Si vous avez installé la version 1.0 de Message Suite sur le Series 5, 
vous n'avez pas besoin de la supprimer avant d'installer cette nouvelle 
version. L'installation remplacera automatiquement toute version antérieure 
de Message Suite. Notez que votre configuration et les données de votre 
fournisseur d'accès ne seront en aucun cas affectées par l'installation 
de la nouvelle version. Vous n'avez donc pas besoin de configurer à 
nouveau Message Suite.

Notez qu'il est recommandé de sauvegarder le contenu du disque du Series 5 
avant d'installer des logiciels supplémentaires ou de modifier des paramètres 
d'importance significative.

Installation de Message Suite à l'aide d'un ordinateur PC et Win 95/NT
======================================================================

Il faut que PsiWin v2.X soit déjà installé sur votre PC.

Message Suite, PsiWin 2.1 et les autres programmes Psion comprennent 
désormais l'utilitaire "EPOC Install" que vous pouvez utiliser sur votre 
PC pour installer les programmes sur le Series 5. 

Pour vérifier si EPOC Install figure déjà sur le disque, cliquez à l'aide 
du bouton droit de la souris sur l'icône "Mon Psion" placée sur le bureau 
et vérifiez si l'option "Installer un nouveau programme" existe. Si ce 
n'est pas le cas, vous devez d'abord lancer SETUP.EXE pour installer le 
programme "EPOC install". 

Vous pouvez désormais soit vous servir de l'option "Installer un nouveau programme", 
soit cliquer deux fois dans l'Explorateur Windows sur un fichier .SIS pour 
installer Message Suite, les fichiers modèles du fournisseur d'accès ou d'autres 
programmes sur le Series 5. 

Lors de la première installation d'un programme ou fichier à l'aide 
d'EPOC install, une icône Ajout./suppr. est ajoutée dans le Panneau de 
configuration du Series 5. Pour en savoir plus à ce sujet, voir le guide 
d'utilisation Message Suite.

Installation de Message Suite à l'aide d'autres ordinateurs
===========================================================

Vous devez disposer sur votre ordinateur du logiciel adéquat pour copier des 
fichiers sur le Series 5, ainsi qu'un câble approprié pour relier l'ordinateur 
au Series 5.
 
Si vous disposez d'un ordinateur PC avec Windows 3.xx, vous pouvez utiliser 
PsiWin 1.xx. Si vous avez un ordinateur Macintosh, vous pouvez vous servir 
de PsiMac ou Psion MacConnect.

Si vous n'avez pas encore installé de logiciels sur le Series 5 :
Connectez le Series 5 et copiez le fichier instexe.exe sur le Series 5. Il est 
recommandé de copier le fichier dans le dossier "\" (racine). Localisez ensuite 
le fichier dans l'écran Système, sélectionnez-le et appuyez sur Entrée pour 
exécuter le fichier. Après quelques instants, le fichier disparaît de l'écran. 
L'icône Ajout./suppr. est alors installée dans le Panneau de configuration du Series 5.

Réservé aux utilisateurs MacConnect :
Vous pouvez installer Message Suite à partir du fichier .SIS en utilisant la fonction 
d'installation MacConnect. Sélectionnez PsiTools dans le menu Psion, appuyez sur 
le bouton Installer, puis placez-vous dans le dossier dans lequel vous avez enregistré 
Msgsuite.sis. Sélectionnez ce fichier et cliquez sur Ouvrir. Suivez les instructions 
qui s'affichent à l'écran pour installer les programmes de Message Suite sur le Series 5.

Autres utilisateurs :
Copiez le fichier msgsuite.sis sur le Series 5 (il est recommandé de le copier dans le 
dossier "\"). Sélectionnez le fichier dans l'écran Système et appuyez sur Entrée. 
L'installation des programmes de Message Suite est alors lancée. Suivez les instructions 
qui s'affichent à l'écran, en veillant à bien utiliser le même disque pour 
l'installation de chaque programme de Message Suite. Une fois l'installation terminée, 
le fichier .SIS sera supprimé pour économiser l'espace disque.


Utilisation de la liaison distante
==================================
Vous devez utiliser la liaison distante sur le Series 5 pour pouvoir installer des 
programmes à l'aide d'EPOC Install. Vérifiez que la liaison est paramétrée sur 
"Câble" et que le Series 5 est bien connecté au PC.

Pour pouvoir fonctionner, les programmes de Message Suite exigent que la liaison 
distante soit configurée sur "Désactivée". Si la liaison est en cours 
d'utilisation (par ex. si elle est configurée sur "Câble"), elle sera automatiquement 
réglée sur le paramètre "Désactivée" dès que vous vous connectez à l'Internet. 
Veillez à bien réactiver la liaison quand vous souhaitez établir la connexion avec le PC.



Comment procéder si EasyFax est installé sur le disque
======================================================
Si vous avez installé EasyFax auparavant sur votre machine, il vous faut alors 
supprimer les fichiers suivants dans l'ordre indiqué pour garantir le bon 
fonctionnement des fonctions d'envoi et de réception des fax :

   \SYSTEM\APPS\EASYFAX\EASYFAX.AIF
   \SYSTEM\APPS\EASYFAX\EASYFAX.APP
   \SYSTEM\APPS\EASYFAX\EASYFAX.MBM
   \SYSTEM\APPS\EASYFAX\EASYFAX.R01
   \SYSTEM\APPS\EASYFAX\EASYFAX.R10
   \SYSTEM\APPS\EASYFAX\EASYFAX.HLP
   \SYSTEM\LIBS\FAXSTR.DLL
   \SYSTEM\LIBS\FAXTRANS.DLL
   \SYSTEM\LIBS\FAXVIEW.DLL
   \SYSTEM\PRINTERS\FAXPRINT.PDL
   \SYSTEM\PRINTERS\FAXPRINT.PDR
   \SYSTEM\PRINTERS\FAXPRINT.UDL

Il est également recommandé de supprimer les fax reçus dont vous n'avez plus besoin. 
En général, ceux-ci se trouvent dans le dossier "\Fax\", mais il se peut qu'ils 
soient stockés ailleurs. Ils sont toujours dotés d'un nom de fichier du type "nom.fax". 
Notez que les fax, une fois supprimés, ne peuvent être restaurés et qu'il est 
donc important de les imprimer si vous souhaitez en conserver une copie.


Informations supplémentaires
============================

Vous pouvez utiliser une grande variété de modems standards ou bien définir un 
nouveau modem dont les paramètres correspondent au vôtre. 

Message Suite compte des entrées prédéfinies correspondant aux modems ci-dessous :

	- modem compatible Hayes (à utiliser pour un modem dont le nom ne 
	  figure pas dans cette liste)
	- Psion Travel Modem
	- Psion Dacom Modem (par ex. Meteor, Surfer)
   	- US Robotics Sportster
	- connexion directe par câble (par ex. pour une connexion câblée avec NT RAS)

Notez que pour les modems externes, il vous faut utiliser un adaptateur/câble 
pour modem ainsi que le câble de liaison Psion.

Pour obtenir des informations complémentaires, consultez le site http://www.psion.com/international
