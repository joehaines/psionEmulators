

Existing users please read on, there is lots of new stuff in here ...



Psion Comms Scripts for use with ReadCIS
----------------------------------------

This posting contains some Psion Comms Scripts for accessing Compuserve
features in conjunction with ReadCIS, the Shareware Psion OLR produced by
Dennis Lefebvre and David Keppler.

The scripts have been written for my own use and I am making them available
here should they be of use to others. You are free to copy and modify the
scripts with the following provisos:

1. You must distribute this posting in its entirety, including this text
   file which must be unmodified.
   
2. You may distribute modified versions of the scripts so long as the original
   posting is included, in its entirety, with your modified scripts.
   
3. If you find these scripts useful please drop me a quick note by Compuserve
   mail. I am curious to know if and how they are being used.
   
4. Note that it is an offence to re-distribute TV listings from the PAO
   News Service that you may obtain from these scripts.

   
No warranty is made as to the fitness of these scripts for their intended,
or any other, purpose. No liability is accepted for any damage that they 
may cause to your data or to your machine. 
I have used these scripts for my own use for and have had no problems. 
I have made them available in good faith and I assume that you will use them 
as such. You use them at your own risk.   
  
  
The following files comprise this posting:

    3user.scr       Example 3USER script.
    3user.sco       Translated version of the above.
    address.scr     Script to download the contents of your 
                    Compuserve Mail Address Book
    address.sco     Translated version of the above.
    charges.scr     Script to download a list of recent online charges
    charges.sco     Translated version of the above
    files.scr       Script to download a listing of recent files in the
                    library of each forum you visit from ReadCIS.
    files.sco       Translated version of the above.
    history.txt     List of new features introduced in each release.
    posting.txt     Description to be used when posting on a BBS
    uk_tv.scr       Script to download a TV listing from the PAO news service.
    uk_tv.sco       Translated version of the above.  
    readme.txt      You're reading it!
    setup3u.opa     Setup program for all the above scripts


Installation
------------

1.  Copy the .SCO files that you require to an /SCO directory on a disk
    on your Psion 3a.
    The 3USER script MUST BE INSTALLED ON YOUR DEFAULT DISK. This is the 
    disk you setup from the System Screen, Disk menu, Default Disk entry 
    (shift+Psion+H).
    The other scripts may be installed on any disk (but see warning later).
    
    Note that you only need copy the .SCO files corresponding to services
    you require (e.g. if you don't want UK TV listings then don't copy
    the UK_TV.sco file to your Psion), but YOU MUST COPY 3USER.SCO to your
    Psion 3a if you want to use ANY of the services.
    
2.  If you want to modify any of the scripts then copy the relevant .SCR files 
    to an /SCR directory on the same disk as your /SCO directory on your
    Psion 3a.
    
3.  Copy SETUP3U.OPA to the /APP directory on any disk on your Psion 3a.    
 
4.  Install SETUP3A.OPA on your Psion desktop as an application, in the
    normal way (i.e. place the cursor where you would like to place the
    SETUP3A icon on the System screen, e.g. next to ReadCIS, press Psion-I, 
    select SETUP3U.OPA, press Enter). 
    
5.  Run ReadCIS

6.  Select "Configure Names"

    Steps (7) to (8) are only required if you want to use the UK TV script,
    if you don't then skip to step (7).

7.  Select "Auxillary Names"

8.  Against "Auxillary Name 1" enter the NUMBER of the ITV region you live in 
    from the following list:

        1 ANGLIA                                                                       
        2 BORDER                                                                       
        3 CARLTON/LWT                                                                  
        4 CENTRAL                                                                      
        5 GRAMPIAN                                                                     
        6 GRANADA                                                                      
        7 HTV WALES                                                                    
        8 HTV WEST                                                                     
        9 MERIDIAN                                                                     
        10 SCOTTISH                                                                     
        11 TYNE TEES                                                                    
        12 ULSTER                                                                       
        13 WEST COUNTRY                                                                 
        14 YORKSHIRE                                                                    

    (This list is the PAO UK TV menu and may change from timr to time, 
     you can check it when you are online)


    Step (9) is only required if you want to use the FILES script, if
    you don't then skip to step (10).

10. Select "Forums". Ensure that against each Forum Section there is a list 
    of forum sections or the word "ALL" if you visit all sections. If any
    section is left blank then the Files script will fail.
    
11. Select "Save and Exit"

12. Return to the System screen and run SETUP3U.

    SETUP3U sets the ReadCIS style switches that are used by the scripts.
    Select "Novice Mode" and follow the on-screen instructions. The program
    will ask you which services you want to use and will then install the
    necessary switches. If this is your first time with these scripts then
    you may want to set the "Guide" on, more about this later.
    
    SETUP3U will also check that you have copied all of the scripts that you
    require to your Psion 3a - if any scripts are listed as "NOT INSTALLED" 
    then you must copy them to an \SCO directory.

13. Go online! After your normal online session, the scripts will run and
    collect the information that you specified.
    The filenames used, as seen from ReadCIS, are as follows:
    
        services\radio      UK Radio listing for next 3 days
        services\sky        UK Satellite TV listing for next 3 days
        services\c4         Channel 4 TV listing for next 3 days
        services\itv        ITV TV listing for next 3 days
        services\bbc1       BBC1 TV listing for next 3 days
        services\bbc2       BBC2 TV listing for next 3 days
        forums\files1       Files uploaded to Forum 1 in last 7 days
        forums\files2       Files uploaded to Forum 2 in last 7 days
        forums\files3       Files uploaded to Forum 3 in last 7 days
        mail\address        Contents of Mail address book
        services\charges    Listing of recent online charges
        
    The files may be viewed and deleted from within ReadCIS in the
    usual manner. The scripts will not overwrite a file that already exists.
    This means that should you want to retrieve a new Radio listing then you
    must delete any \cis\services\radio.cis file that exists first. You can
    do this from within ReadCIS.
    
    
    
Online Guide
------------    

The "Online Guide" is a new feature of the scripts. Many users of the last
version of the scripts were confused by the miriad of settings available -
whether or not to install Scripts, which Switches to set, when to delete
files that were previously retrieved. Unfortunately it is not possible to
simplify the installation and setup. Instead, two new features have been
introduced. The SETUP3U program simplifies the task of setting Switches and
remembering which Switch does what. The "Online Guide" walks through an online
session with you, telling you what is happening and why a feature may not be
working the way that you intended. In this respect, it is a bit like
Microsoft's "Wizards" - albeit a lot less sophisticated.

When you set the "Guide.On" switch your online session becomes interactive.
This means that you must not leave a Guided session unattended if you want
to avoid large phone bills!. Whenever the scripts have to make a descision then
the Guide pops-up an alert box to tell you what has been found. 
"Decision"'s are things like: 
    Is a switch set? 
    Has a script been installed on this machine?
    Does a previously retrieved file already exist?

The Guide uses "smileys" - these are the strange first few characters of the
alert box that if you tip your head to the left you will see a "face". You
can ignore the smileys if you want, they are only there for fun. The possible
smileys are:

    :-)     Smiling - the scripts are probably doing what you wanted.
    
    :-O     Spotted something - the scripts have made a decision that you
            should check. It may be what you wanted, and there again ...
            
    :-(     Concerned - the scripts have made a decision that you should
            check. It may be what you wanted, but is more likely not what
            you had in mind.

Remember to turn off the Guide when you have completed configuring the scripts
the way you want. Your session will then run quicker, saving your phone bill,
and may be left unattended (but see warning below).


More about Switches
-------------------

After you have used the scripts a little you may want to experiment with the
Switch settings. The switches can be created, toggled or deleted from within
ReadCIS in the usual manner. Alternatively, you may want to use SETUP3U for
this task. SETUP3U's Expert Mode allows you to browse the switches and set them
"On", "Off", or "Unset" (deleted). Select "Expert Mode" in SETUP3U and follow
the on-screen instructions.

Remember to press "Esc" when going back into ReadCIS to see the changes you
have made to the switch settings.


The following switches work in special way:

    Files, has "below" it:
        Files1
        Files2
        Files3
        
    Uk_Tv, has "below" it:
        Bbc1
        Bbc2
        Itv
        HtvWales
        S4C
        Sky
        Radio
    
These switches "blanket" the switches "below them":

    When a "blanket switch" is On then all the services "below" that have
    their own switch Unset, or On, are also On. 
    
    When a "blanket switch" is set Off then all the services "below", 
    regardless of their own switch setting, are also Off.


Examples:
    
    You can turn off all TV services by setting Uk_Tv.Off -
    there is no need to turn Off all the other TV switches.

    You can turn on all TV services by setting Uk_Tv.On - there is no need to
    turn on all other TV switches, but switches that are Off will be skipped.

    You can have all TV services on except Radio by setting Uk_Tv.on 
    and Radio.off.

SETUP3U's Novice mode does not set blanket switches On and it only sets
"below blanket" switches On if required.
 
SETUP3U also never sets "below blanket" switches Off, but will set blanket
switches Off. This means that you can subsequently turn off the blanket 
switch to disable all services or turn on the blanket switch to enable all 
services that were not turned on by SETUP3U.

In Expert mode, SETUP3U allows you to set switches any way you want. What you
set is what you get!

If all that sounded confusing then read it again. Blanket switches are fairly
intuitive to use but difficult to explain!


Running a Second User Script
----------------------------

You can run a second user script by:

1.  Naming the second script "3USER2.SCO" and putting it in an /SCO directory.
2.  Setting the switch User2.On.

I have not used this feature, but have incorporated it for users that
may want to use the Internet scripts available on the forum. Let me
know how you get on if you make use of this feature.



Disks
-----

The scripts are searched for on disks in the following order:
    SSD-slot A:
    SSD-slot B:
    Internal M:
    
This means that if you have an old version of a script on SSD A:, for example,
and you then copy a new version to your Internal disk, say, then the old
version will be run and not the new one.

The exception to this rule, is that the 3USER script MUST BE INSTALLED ON
YOUR DEFAULT DISK. This is the disk you setup from the System Screen, Disk
menu, Default Disk entry (shift+Psion+H).
    

    
Unattended Sessions
-------------------

If the Guide is off, then you can run your ReadCIS session unattended. Quite
a lot of users seem to do this and there is a macro on the forum that allows
you to run ReadCIS at a preset time of day in your absence, e.g. just before
you get-up in the morning. However, a word of warning ...

If your session should run out of diskspace during an unattended session then
ReadCIS or these scripts may pop-up an alert box waiting for you to press a key.
If this happens then your session stays online for a long time! It is likely
that Compuserve will eventually hang-up the phone on you, but I cannot guarantee
that you will not incur a large phone bill if this happens. It is not
very likely, but the possibility is there - you must take the risk.

I accept no liability for any online charges you may incur as a consequence 
of using these scripts.



Modifying the scripts
---------------------

This is not for the faint hearted! However, if you have a basic knowledge
of programming then you should not have too much difficulty in modifying the
scripts to suit your needs (e.g. more than 7 days worth of files or 3 days
worth of TV listings). You will need the 3link manual which explains the
script language used by the Comms application. Note the following:

1.  The scripts call each other in a chain as follows:

    3user -> address -> files -> uk_tv -> charges -> 3user2 or logoff
    
    Take care not to break the chain.
    

2.  Change the DAYS:7 part of the Files script to modify the number of
    days worth of file listings to obtain from the forums.
    
    
3.  Changing the number of days worth of TV listing is not so straightforward.
    The PAO UK TV menus vary daily as to the number of menu items and the
    control constructs offered by Comms are very rudimentary.
    The solution I opted for is to replicate large amounts of the script to
    account for each possible number of menu items at each login (inelegant
    but has the advantage of being reletively readable). 

    Each PAO menu lists the days available, nearest day with highest number,
    furthest day with lowest number, so you will see lots of lines like these
    in the script:
    
        SEND "s 4,3,2"
        SEND "s 5,4,3"
                            ... etc
        
    To make the script list, say, 4 days worth of listing then change all these
    lines accordingly, e.g.
    
        SEND "s 4,3,2,1"
        SEND "s 5,4,3,2"
                            ... etc
                            
    Watch out for Channel 4, though, C4 and S4C share the same menu; C4 uses 
    even numbers and S4C uses odd!
    


Problems
--------

If you have any problems then:

First, please read this text file again. Check your installation. Run SETUP3U
and check your switch settings. Set the Guide on and make sure that you
understand what is hapening during your session and why. Read the "Common
Problems" section below.

Second, please don't contact the authors of ReadCIS for help. These scripts
are not their product and they are very busy as it is.

Third, if you are stuck then please feel free to mail me. Please don't post
messages on the forum for me as I don't check the forum every day and the
scroll-off rate is currently very high. Please turn on the Sleuth switch and
mail me the \sleuth.cis file that is produced by your next online session.
If possible, please mail me your Compuserve Profile settings (go profile).
I do not check my mail every day, so please be patient. I would normally
expect to reply to you within 2 weeks (but often a lot sooner than this!).
Please quote the revision number of the scripts that you are using. This
release is:
    $Revision: 2.0 $



Common Problems
---------------

Most problems in the last version were caused by the confusing number
of things to setup. Hopefully, users will now find this easier using the
SETUP3U program and the GUIDE online switch.

Slow Compuserve nodes can sometimes cause a problem (this can cause problems
with ReadCIS as well). The usual symptom of this is that the script issues
a "go pao", for example, and after a minute or so Compuserve has still
not arrived at "pao". The script then pops-up an alert box saying
"Compuserve has not responded with a prompt". The scripts now incorporate
longer timeouts that should get around this. If Compuserve is very slow
then it is possible that this may still happen, however. If this happens
to you then mail me a Sleuth.cis file (see under "Problems" above). This will
allow me to see where Compuserve is being slow. I can then increase the
script timeouts further for you.

The scripts are heavily reliant on the Compuserve and PAO menu structures.
Compuserve change the menu structure from time-to-time. This stops the scripts
from working. For example, this release of the scripts accesses Uk Tv on
menu item 7 of the PAO menu. The previous release uses menu item 4 - Compuserve
changed the menu structure since the last revision of the scripts. These
problems are fairly infrequent and easily fixed. Once again, mailing me a
Sleuth.cis file will allow me to determine which menus have changed and to
update the scripts accordingly.

The scripts have only been tested with my own profile settings.
If you do not have your Compuserve profile set for menus then you may have
difficulties (see ReadCIS documentation regarding profiles).
The following is a list of my own settings. The settings marked with an 
asterisk (*) may affect the behaviour of the scripts. If you are having 
problems then try to make your Profile settings match mine as much as possible,
especially those settings marked with an asterisk.


------------------------------OOO-------------------------------------

!go profile

Customer Service   TERMINAL

TERMINAL/SERVICE OPTIONS 

Use this area to change your terminal type/parameters and/or service options.

Note: Your permanent and session
settings match.

 1 Instructions
 2 Change permanent settings
 3 Explanation of session vs. permanent
 4 Show session vs. permanent
 5 Change current session settings


Enter choice !4

SESSION VS. PERMANENT

 1 Logon/Service options
 2 Display options
 3 Terminal type/parameters
 4 Transfer protocol/graphic support


Enter choice !1

SERVICE OPTIONS

                          PERMANENT ONLY
First Service at logon            [MAIN]
CompuServe Mail Waiting    [NOTIFY ONLY]
Personal Menu established           [NO]

                   PERMANENT     SESSION
TOP goes to           [MAIN]      [MAIN]
Online editor      [DEFAULT]   [DEFAULT]
Forum mode            [MENU]      [MENU]    *
Basic Alerts           [YES]       [YES]    *


Last page !

SESSION VS. PERMANENT

 1 Logon/Service options
 2 Display options
 3 Terminal type/parameters
 4 Transfer protocol/graphic support


Enter choice !2

DISPLAY OPTIONS

                   PERMANENT     SESSION
PAGED display          [YES]       [YES]    *
BRIEF prompts           [NO]        [NO]    *
CLEAR between pages    [YES]       [YES]
BLANK lines sent       [YES]       [YES]
Line feeds sent        [YES]       [YES]
Language Preference
                   [ENGLISH]   [ENGLISH]    *
Character Set        [ASCII]     [ASCII]    *


Last page !

SESSION VS. PERMANENT

 1 Logon/Service options
 2 Display options
 3 Terminal type/parameters
 4 Transfer protocol/graphic support


Enter choice !3

TERMINAL TYPES/PARAMETERS

                          PERMANENT ONLY
Micro inquiry sequence at logon    [YES]

                   PERMANENT     SESSION
TERMINAL type        [OTHER]     [OTHER]
Screen WIDTH            [80]        [80]
LINES per page          [24]        [24]
Form FEEDS       [SIMULATED] [SIMULATED]
Horizontal TABS  [SIMULATED] [SIMULATED]
Chars. rec'd (CASE)    [U/L]       [U/L]
Chars. sent in CAPS     [NO]        [NO]    *
PARITY                [ZERO]      [ZERO]
Output DELAYS            [1]         [1]
ERASE when backspacing [YES]       [YES]


Last page !

SESSION VS. PERMANENT

 1 Logon/Service options
 2 Display options
 3 Terminal type/parameters
 4 Transfer protocol/graphic support


Enter choice !4

FILE TRANSFER/GRAPHICS

                   PERMANENT     SESSION
PROTOCOL choice  [SHOW MENU] [SHOW MENU]
GIF support             [NO]        [NO]
NAPLPS support          [NO]        [NO]
RLE support             [NO]        [NO]


Last page !

----------------------------OOO---------------------------------



Finally, I hope you enjoy using these scripts!


Mark Hadfield [100407,2723]
 

