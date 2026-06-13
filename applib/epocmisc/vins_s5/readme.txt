VINS Series5  -  Version 3.01

Jean-Philippe PASQUIER


	(for English translation, see below ...)


Présentation & fonctionnalités :
=========================

VINS est un logiciel sur le vin et la gestion d'une cave à vins.

VINS comprend de nombreuses bases de données pour son fonctionnement, en voici la description :

	* Une BASE DES APPELLATIONS : plus de 1150 appellations (toutes les AOC et
AOVDQS Françaises, Suisses, Portugaises, Italiennes, Algériennes  pour le moment ...) avec les caractéristiques suivantes :
		- nom de l'appellation
		- pays
		- région
		- Vinification : rouge, blanc ou rosé
		- type de vin (caractéristique classique de l'appellation) :
			- Vins rouges :
				- Léger et souple
				- Corpulent et fruité
				- Généreux mais tendre
				- Ferme et puissant
				- Effervescent
				- Doux
			- Vins blancs :
				- Léger et fruité
				- Sec
				- Corpulent
				- Moelleux
				- Effervescent
				- Doux
			- Vins rosés :
				- Léger
				- Corpulent et fruité
				- Effervescent
				- Doux
		- Cépages de l'appellation
		- Apogée mini
		- Apogée maxi
		- Carte graphique de localisation de l'appellation

Cette base est modifiable par l'utilisateur et il est possible de rajouter des vins.

	* Une base MILLESIMES qui comprend une cotation qualité des années de 1950 à 1996 (si possible) par pays et région. La base est renseignée avec des données sur de nombreux pays, dont : France, Espagne, Italie, Allemane, Suisse, USA, Chili, ... Cette base est modifiable par l'utilisateur.

	* Une base PAYS, avec les principaux pays producteurs. Cette base est modifiable par l'utilisateur.

	* Une base REGIONS, avec pour chaque pays, les régions viticoles associées. Cette base est modifiable par l'utilisateur.

	* Une base CEPAGES avec près de 1000 cépages, qui couvrent la production viticole de tous les continents.

	* Une base METS & VINS divisée en 5 types de mets :
		- Desserts
		- Entrées
		- Fromages
		- Légumes
		- Produits mer & rivière
		- Viandes
avec pour chaque type des divisions et sous-divisions pour plusieurs centaines de préparations culinaires. Pour chacune de ces préparations, la base propose un (ou des) vin  principal et un (ou des) vin alternatif (utile si votre cave est réduite ...).

	* Une (ou plusieurs)  base(s) CAVE qui comprend vos propres vins. Les caractéristiques suivantes peuvent être gérées :
		- Toutes les caratéristiques de l'appellation extraite de la BASE DES
APPELLATIONS (nom appellation, vinification, ...)
		- Nom du vin
		- Année
		- Cru
		- Quantité
		- Taille bouteille
		- Emplacement
		- Coordonnées récoltant / revendeur
		- Notes de dégustation
		- Prix et date d'achat
		- Prix et date révisés
		- 2 champs utilisateurs
L'apogée des vins de la cave est recalculée en fonction de l'apogée propre à l'appellation et de la qualité du millésime.


Fonctionnalités :
=============

Quelques unes des fonctionnalités de VINS : 
* Il est possible de gérer plusieurs caves (jusqu'à 5).
* Fonction copier / coller / couper pour les fiches des caves.
* Fonctions d'analyses poussées de la cave : statistiques par type/ pays/ région/ valeurs; analyse graphique de l'apogée; analyse graphique du viellissement.
* Aide complète et détaillée.
* Conseils de service.
* Association mets / vins.
* Exportation vers fichier texte

Utilisation :
=========

* Faire menu/spécial/aide pour une aide en ligne complète.	


Divers :
=======

* De nombreuses appellations existent en plusieurs vinifications et éventuellement  plusieurs types de vin peuvent exister pour une même appellation et vinification. Par exemple, dans l'appellation GAILLAC, il existe du rouge, du rosé et 2 types de blancs : Moelleux et Sec.

* Les données incluses dans VINS sont fournies sans garanties d'aucunes sortes. Si vous constatez des erreurs, merci de m'en faire part pour que je les corrige.


INSTALLATION :
================================================================

L'archive .ZIP comprend les 2 fichiers suivants :

	- VINS_S5.SIS
	- UPGRADE.TXT  -> A LIRE ABSOLUMENT EN CAS DE MISE A JOUR
	- UPDAT_V3.OPO  (voir Upgrade.txt)
	- README.TXT
	- README.DOC (Psion Word )

CAS 1 : Vous disposez de PsiWin ou de MacConnect :
================================================================

Double-cliquez sir l'icône VINS_S5.SIS sur votre PC ou votre Mac. L'installation (ou mise à
jour) se fera automatiquement.
En cas de problème, voir le cas 2.

CAS 2 : Vous ne disposez pas de PsiWin ou de MacConnect :
================================================================

Copiez le fichier VINS_S5.SIS sur votre Series 5 (chargement direct depuis internet, liaison série,
...), puis éxécutez le. L'installation (ou mise à jour) se fera automatiquement.


* Cas particulier : Mise à jour depuis la version 2.0 :
Une fois l'installation terminée, et AVANT de lancer la nouvelle version, il faut aller dans le répertoire C:\SYSTEM\APPS\VINS\ et lancer le programme : UPDATE CAVE_21.OPO, il effectuera la mise à jour du fichier de cave à vins. (attention : pour accèder au répertoire 'system', vous devez cocher l'option 'Afficher dossier 'Systeme' dans les préférences (Maj+Ctrl+F))

* Mise à jour base de données : 
Lors du premier lancement, VINS effectue une mise à jour automatique des bases de données : un message et un dialogue de progression apparaissent a l'ecran. Cette operation peut durer plusieurs minutes, soyez patient !!

* Espace disque :
Dans le cas d'une mise à jour, des fichiers sont devenus inutiles. Vous pouvez les détruire pour libérer de l'espace disque : VINS3.DAT ; VINS4.DAT ; VINS.HLP et UPDATE CAVE_21.OPO
(après utilisation ! voir ci-dessus).


Shareware :
===========

VINS est  un shareware. Cela signifie que :
	- Si, après une période de test, vous décidez de le garder, vous devez vous enregistrer auprès de l'auteur (voir menu Spécial/shareware) .

Vous pouvez vous enregistrer de 2 manières différentes :
	1 - via internet : $25, avec une carte de crédit : http://order.kagi.com/?XC
	2 - par courrier : envoyez 120 Francs Français ou 18,30 Euros (cheque ou billets) et vos coordonnées à :
			J.Ph. Pasquier
			17, rue du Général Leclerc
			67380 LINGOLSHEIM
			FRANCE

Dans tous les cas vous devrez me communiquer le numéro de série de votre Psion (il est rappelé dans menu Spécial/shareware).

	- Vous pouvez distribuer librement ce logiciel non-modifié et avec l'ensemble des fichiers inclus dans ce package.



Versions :
========

1.3	-	février 1998
			1ére version béta pour le Series 5.

2.0	-	avril 1998
			1ère version publique, avec notamment les cartes géographiques des
appellations.

2.1	-	mai 1998
			Ajoute des fonctionnalités importantes pour la gestion de cave :
				* Rubriques (coordonnées récoltant/revendeur, notes de
dégustation, prix et date d'achat, prix et date de révision)
				* Analyses : graphique apogé et viellissement

2.2	-	Juin 1998
			Quelques corrections et améliorations.

2.5	-	Février 1999
			Bilingue Français / Anglais et de nombreuses améliorations : Notamment :
				* gestion multi caves
				* copier coller de vins, y compris entre cave
				* gestion de la monnaie : francs, dollars, ...
				* statistiques sur valeurs
				* Distribution sous forme de .SIS
				* ...

3.0 -	Décembre 1999			
			Version majeure : apporte principalement une gestion homogène entre les vins Français et les vins non-Français, plus quelques améliorations dans l'utilisation.
			Améliorations bases de données :
				* La structure des bases a été refondue et intègre de nouvelles bases : pays et régions.
				* La base des millésimes n'est plus la même et permet maintenant à l'utilisateur de modifier / créer des entrées.
				* La base des appellations est surtout enrichie avec des données de nouveaux pays : Suisse, Italie, Portugal, Algérie, avec les cartes correspondantes (sauf Algérié).
				* La base des cépages a explosé : elle comprend désormais près de 1000 cépages !!
			Améliorations autres :
				* Les statistiques sont disponibles pour les vins non-Français
				* Mise en place d'un "ascenseur" en remplacement des flèches pour la navigation
				* En analyse apogée, il est possible de se déplacer avec le stylet
				* Ajout d'une case d'iconisation de la fenêtre de status
				* Ajout du nom de la cave courante dans la fenêtre de status
				* Ajout du type de vin dans les écrans de résultat de recherche (évite confusion)
				* Ajout de nouvelles contenances bouteilles : 50 cl, 62 cl, 70 cl, 1 l
				* Bug en exportation si Vins installlé sur D: corrigé

3.01 -	Janvier 2000
			Quelques bugs corrigés




Pour tous commentaires, remarques, idées, critiques, ........ :

jppasquier@calva.net     ou     jppasquier@kagi.com

Web : www.kagi.com/jppasquier/




***************************************************************************
***************************************************************************

VINS Version 3.01

Jean-Philippe PASQUIER



Generalities :
==============

VINS is a software about wine and wine-cellar management. 'VINS' means wines in french.

A large part of VINS are databases. Hereafter, you'll find their descriptions :

	* A database of APPELLATIONS : more than 1150 appellations are included (every french
'AOC' and 'AOVDQS', Swiss, Italians, Portugese and Algerians for instance ...) :

		- Appellation name
		- Country
		- Region
		- Type : red, white or rose
		- Wine taste :
			- Red wines :
				- Light and smooth
				- Rich and fruity
				- Generous but tender
				- Strong and powerfull
				- Sparkling
				- Sweet
			- White wines :
				- Light and fruity
				- Dry
				- Strong
				- Soft
				- Sparkling
				- Sweet
			- Rosy wines :
				- Light
				- Rich and fruity
				- Sparkling
				- Sweet
		- Grapes
		- Best years mini and maxi
		- Map of the appellation area

This database is updatable by user and it's possible to add some new appellations.

	* A database of YEARS QUALITY with quality cotation by country / region / type, for each year since 1950 (when available ...). A lot of countries can be found in that base, all over the world. Updatable by user.

	* A COUNTRIES database, with the main wine producer countries in the world. Updatable by user.

	* A REGIONS database, with wine region attached to a country. regions of main countries are preset in that base. Updatable by user.

	* A GRAPPES database, with nearly 1000 differents grappes from the entire world.

	* A database of FOODS AND WINES divided in 5 parts :

		- Desserts
		- Starters
		- Cheeses
		- Vegetables
		- Sea & river products
		- Meats
for a total of more than 110 recipes. For each one, VINS is able to give you a main choice and an alternative one (interesting for small cellar ...)

	* A database WINE CELLAR with your own wines (up to 5 differents cellar files). You can register and manage :
		- Every fields of the APPELLATION database, copied directly from this base when creating a new wine in the  CELLAR
		- Wine name
		- Year
		- Vintage
		- Quantity
		- Bottle size
		- Location
		- Address of wine grower or retailer
		- Notes on tasting
		- Purchase price
		- Date of purchase
		- Price revised
		-  Date revision
		- User notes 1
		- User notes 2

The Best years of wines are recalculated automaticaly, from standard best years for the appellation and year quality.


Functionnalities ?
=============

Some of the functionnalities of the program :
* Ability to manage up to 5 differents cellar.
* Copy / Cut / Paste function for cellr entries.
* Analysis functions : statistics by type / countries / region / finance; graph analysis of best year for cellar wines; graph analysis of ageing for cellar wines.
* Full help file.
* Services tips (temperature, time between opening and drinking, ...).
* Foods / wines suggestions.
* Exportation of the cellar entries to a text file.

How to use ?
=============

A complete help is available on line : Menu / Special / Help


A few things ...
=============

* A lot of appellations exist in more than one type et sometimes, for a same appellation and type, you can have several taste. For example, the appellation 'GAILLAC' exist in Red, Rose and 2 types of White : Dry or Soft.

* Datas included in VINS are given without garanty of any kind. If you see some mistakes, please tell me.

* About translations :
	* English : made mainly by Mr Gareth LOCK (thanks again...) and for a little part by myself. You may find some mistakes. Please report me, I will correct.



Installation :
=============

The .ZIP file includes :

	- VINS_S5.SIS
	- UPGRADE.TXT  -> VERY IMPORTANT : TO READ BEFORE UPGRADING
	- UPDATE_TO_V3.OPO  (voir Upgrade.txt)
	- README.TXT
	- README.DOC (Psion Word File)

CASE 1 : You own PsiWin or MacConnect :
================================================================

Double-click on the VINS_S5.SIS file on your PC or your Mac. Installation (or update, see upgrading) will be done automatically.
In case of problem, see case 2.

CASE 2 : You do not own PsiWin or MacConnect :
================================================================

Copy the VINS_S5.SIS file to your Series 5 (direct download from internet, serial link,
...), then launch it. Installation (or update, see upgrading) will be done automatically.


* Upgrade from version 2.0 :
At the end of the installation and before launching Vins , launch the program UPDATE CAVE_21.OPO in the directory C:\SYSTEM\APPS\VINS\.  This will update the cellar files to V2.n or V3.n.   If you have Vins 2.1 or 2.2 do not run this .opo it's of no use.

*  Upgrading:
In all cases, Vins  will upgrade databases.  The first screen that will be shown when Vins is launched is the 'Upgrading...' dialogue and a progress window.  This shows how far the program is in updating the cellar .dbf file.  This can take a few mins to complete.  Please be patient.

You can delete the following files if you have upgraded from V2.x to free up disk space VINS3.DAT, VINS4.DAT, VINS.HLP and UPDATE CAVE_21.OPO (after use).

Shareware :
===========

VINS is shareware. That means :
	- If, after a trial period, you decide to keep it, you must register to the author (See menu / Special / Shareware)

You can register in 2 differents ways :
	1 - via internet : $25, with a credit card : http://order.kagi.com/?XC
	2 - by mail : send 120 French Francs or 18.30 Euros (check or bills) and your details to :

			J.Ph. Pasquier
			17, rue du Général Leclerc
			67380 LINGOLSHEIM
			FRANCE

In all case, you'll have to give me the internal number of your Psion (recalled in menu / Special / Shareware)

	- You can freely distribute this software unmodified and with every files included in this package.



Versions :
========

1.3	-	02/1998
			First beta version for Series 5.

2.0	-	04/1998
			First public version.

2.1	-	05/ 1998
			Adding some main functionnalities :
				* Fields (address of wine grower or retailer, notes of tasting, price and date of purchase, price and date of revision)
				* Analysis : Best year et ageing graphs

2.2	-	06/1998
			Some bugs corrected.

2.5	-	02/1999
			Bilingual version French / English and a lot of new things :
				* Multi cellar management
				* copy / paste of wine entry
				* currency management : francs, dollars, ...
				* valorisation of the cellar
				*.SIS distribution
				* ...

3.0 - 	December 1999
			Main version : mainly a better management of non-french wines and some enhancements in program.
			Databases enhancements :
				* New data structure, with 2 new base : contries and regions
				* The 'year quality' database changed : now user is ble to modify or add entries
				* The appellations database is twice bigger, with wines from new countries : Swiss, Italia, portugal, Algeria, with corresponding maps (except Algeria)
				* The grappes database exploded ! Now there is around 1000 entries !!
			Others enhancements :
				* Stat  are available for non-french wines
				* A 'lift' replace the arrows navigation buttons
				* In 'best years' analyse, you can navigate with stylus
				* Bug corrected in Ageing analyse for huge cellar
				* Status window is now iconisable
				* The nae of the current cellar have been added to the status window
				* Some bottle size added : 50 cl; 62 cl; 70 cl; 1 l
				* The exportation bug when installed on D: corrected.

3.01 -	January 2000
			A few bugs corrected


For any comments, bugs reports,  ........ :

jppasquier@calva.net     ou  jppasquier@kagi.com

Web : www.kagi.com/jppasquier/








