                        UNDELETE ver. 1.00F
                        -------------------
               Copyright (c) Konstantin Saliy, 1996
                                 
HOW TO USE IT
-------------
This is the first release of UNDELETE utility for Flash SSD disks.
It must be run on Psion Series 3a only.

UNDELETE  is  not  undelete  in general  sense  but  file  recover
application  -  it  will copy data from deleted files  to  another
file.  Anyway it can be useful while this the only (IMHO) tool  to
restore deleted data from your Flash SSD.

To  install  it  simply copy it into any \OPP\ directory  on  your
Psion.  The  name Undelete will appear under RunOpl icon  (if  you
have  no  RunOpl installed on your System screen,  refer  to  User
Manual how to install RunOpl).

UNDELETE is easy to use: run it, select SSD drive and press Enter.
You'll  have  a list of all deleted files on your SSD  with  their
original size and date/time stamp. Select file you want to recover
and press Enter. Then choose destination file for recovered data -
that's all! If you want to recover more then one file you have  to
do it file by file - sorry!
While  you are in the deleted files list you can press Help button
to receive instructions above.

DISCLAIMER
----------
I  take  no responsibility for damage or data loss that occurs  as
result  of  using  this program. NO WARRANTY AT ALL.  Be  careful,
backup often and you will never need to run it.

CREDITS
-------
It  would  be  nightmare  to write this  application  without  OPL
Preprocessor by Andy Clarkson OPP ver. 1.2F because of  Flash  SSD
structure.  With  his STRUCT definition I could think  only  about
algorithms - not how to access SSD system data fields.

Flash SSD data format was derived from Psionics files by Clieve D.
W. Feather - I couldn't find it anywhere else.

Thanks to Daniel Pfund who found a number of bugs in beta release.

SUGGESTIONS AND BUG REPORTS
---------------------------
E-mail  all  suggestions  (Type  II  documentation  is  especially
welcome) and bugs to <kis@ipmce.ru>. Please, include "Undelete" in
your subject to let my mailer put it into appropriate folder.

REGISTRATION AND FEE
--------------------
This  is NOT freeware. I want to offer it as shareware. It has  no
nag  screens or any hidden features. You can test it and even  use
it  without  any limitation. But if you found it useful especially
by  recovering  VERY  important data please, try  not  forget  the
author. All in all if nobody will register it I will not know that
anybody really need my programmer's efforts. All registered  users
will receive all upgrades (if any) of this application.

About registration fee.
I  intend to set a registration fee to $15. If you found a cheaper
tool or place (e.g. Psion customer support) to recover erased file
but  still want to use my application I will agree with a half  of
their  price.  If  you  think it costs more -  I'll  not  bargain.
Registering for me other products as a fee is welcome -  just  ask
me what I want and name a maximum price you can pay.

ALL  ABOVE  ARE FOR PERSONAL USE ONLY!. If you want to use  it  in
commercial purpose you must receive written permission from me.

You  can transfer payment to my account in Russia by the following
way:

Intermediate Bank:     The Bank of New York, NY USA
                       SWIFT Address - IRVT US 3N
                       ABA Code - 021000018
Beneficiary Bank:      DialogBank, Moscow, Russia
                       SWIFT Address - DLGB RU MM
Account of DialogBank at
The Bank of New York:  890-0054-808
Beneficiary:           Konstantin Saliy,
                       debit card #5896 5670 0401 4415

You  can  also  send cheque or cache (10 UK pounds  or  80  french
francs,  but  not $$) to Philippe Lebreton - he will collect  them
for me:
     Philippe LEBRETON
     5 rue Au Metre
     F-78290 Croissy Sur Seine
     France
     e-mail <lebreton.p@ccmail.cgi.fr>
Please, make your cheque payable to Philippe Lebreton.

CHANGES SINCE ver. 100B
----------------------
1. Flash SSD Type II are supported now. Indeed previous version
support them too :)
2. Minor bug fixed
3. Time and data stamp is restored too
4. It is more stable when user changes SSDs or opens doors while
recover in progress.
