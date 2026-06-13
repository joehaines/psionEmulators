Psion Speech
Version 0.11

Freeware by Andy Clapham

-----------------------------

Hi! Welcome to psion speech!

Contents

1) Installing
2) Using
3) Configuring
4) About the Author
5) Future plans
6) Word of warning to would be shareware developers

1) Installing
-------------

Copy speak.opa to the app directory
Create a subdirectory off internal called phoneme
Copy the sounds and rules to this directory (*.wve, rules.txt)
Copy the record files called speak1.wve and speak2.wve into the 
wve directory
Install speak.opa from the system menu of your psion

2) Using
--------

The program should hopefully be self explanatory.
It takes about three seconds to parse rules.txt for the phoneme rules.
Use Speech->Input to input something to say
Use Speech->Exit to Quit
Use Settings->Speed to speed up the speech 
(note that very fast settings are rather unintelligable!)
Use Settings->Volume to set the volume
Use Settings->Reload Rules to reload rules.txt
Use Settings->About to get an about box

3) Configuring
--------------

You can configure the rules fairly easily.
Open up rules.txt in word (note that speak will not work while this file is open)
Add phoneme rules in the appropriate place in the file
Reload the rules in the speach app

How the rules work
------------------
Okay, Okay, I didn't spend hours working it all out, it's actually just
a version of an old unix program generally available on the net.
Here's the info as to how it works from the unix program:
**
**	Derived from: 
**
**	     AUTOMATIC TRANSLATION OF ENGLISH TEXT TO PHONETICS
**	            BY MEANS OF LETTER-TO-SOUND RULES
**
**			NRL Report 7948
**
**		      January 21st, 1976
**	    Naval Research Laboratory, Washington, D.C.
**
**
**	Published by the National Technical Information Service as
**	document "AD/A021 929".
**
**
**
**	The Phoneme codes:
**
**		IY	bEEt		IH	bIt
**		EY	gAte		EH	gEt
**		AE	fAt		AA	fAther
**		AO	lAWn		OW	lOne
**		UH	fUll		UW	fOOl
**		ER	mURdER		AX	About
**		AH	bUt		AY	hIde
**		AW	hOW		OY	tOY
**	
**		p	Pack		b	Back
**		t	Time		d	Dime
**		k	Coat		g	Goat
**		f	Fault		v	Vault
**		TH	eTHer		DH	eiTHer
**		s	Sue		z	Zoo
**		SH	leaSH		ZH	leiSure
**		H 	How		m	suM
**		n	suN		NG	suNG
**		l	Laugh		w	Wear
**		y	Young		r	Rate
**		CH	CHar		j	Jar
**		WH	WHere
**
**
**	Rules are made up of four parts:
**	
**		The left context.
**		The text to match.
**		The right context.
**		The phonemes to substitute for the matched text.
**
**	Procedure:
**
**		Seperate each block of letters 
**		For each unmatched 
**		letter in the word, look through the rules where the 
**		text to match starts with the letter in the word.  If 
**		the text to match is found and the right and left 
**		context patterns also match, output the phonemes for 
**		that rule and skip to the next unmatched letter.
**
**
**	Special Context Symbols:
**
**		#	One or more vowels
**		:	Zero or more consonants
**		^	One consonant.
**		.	One of B, D, V, G, J, L, M, N, R, W or Z (voiced 
**			consonants)
**		%	One of ER, E, ES, ED, ING, ELY (a suffix)
**			(Found in right context only)
**		+	One of E, I or Y (a "front" vowel)
**
*/

4) About the Author
-------------------
I'm a freelance software developer specialising in Client Server Systems
(VB3 + Sybase + NT) But I've got a Psion 3a which I like to mess about
with in my spare time.
I'm currently living in Frankfurt, Germany, but hope to move back to
the UK in October.
You can find my homepage at http://www.rhein-main.netsurf.de/~aclapham
I usually use my UK POP account for Email - AndyC@Easynet.co.uk

5) Future Plans
---------------
I'm reeeeeealy busy at the minute. The life of a freelance developer is
a busy one (12 hour days are de rigeur). But when I get time I'm hoping to:
* Add an alarm facility
* Add a read file facility with bookmarks
* Get some better phonemes! The one's I've got were borrowed from the net,
  I've tried recording my own, but they sounded terrible! If you are really
  patient, and have managed to record some better ones, let me know...
* Put some error handling in there - absolutely none at the minute!
* Write a psion shoot-em-up ... Update - this is going OK, expect to see 
  something around July time.
* Learn to program Java

6) Word of Warning to would-be shareware developers
---------------------------------------------------
TEST YOUR INSTALLATION!
Delete everything to do with your program from your psion and do 
a clean install (dont forget to back up first!)
After two fateful releases (this is the third) I think I finally 
got the message (or was it the 100 emails???)

----------------------------------------------------------------------------
Anyway, thanks for your interest. I know the app is not really world-shaking,
but it's the sillicon equivalent of a hula-hoop: fun for a few minutes!

Enjoy

Andy Clapham
8th June 1996