======================Beta tester's information================================
Beta Version 0.3 expires 3/1/1997
  * No change to PSION server code still version 0.2.
  * New "Send" parameter to automatically load the PSION with the IRBACKUP.OPA
    server code using the PSION native Infrared Receive Key. This makes it
    much easier for a new user to get started. No more IRCOPY or PSIWIN needed.
  * Code to support the Actisys IR220 (200 too?) external IR adapter has been
    added and needs to be tested by those who have this adapter. Just add
    /ACT to the end of your command line parameters.
  * Better support for selecting the interrupt level on systems that skip
    comports (ie COM1 and COM3 but no COM2).

Beta Version 0.2 expires 3/1/1997
  * PC Version 0.2 will detect, warn, and work with Psion Version 0.1.
    You should load your Psion with the new version to get the better screens
    and the warning messages about other applications still active.
    The Psion Version 0.2 OPA will not work with the older PC Version 0.1.
  * Improved PSION screens. A proper "About" screen when starting, a new
    warning screen, and during the backup a list of files being backed-up.
  * Give warning to user and show list of applications that are still running
    that could prevent all files from being backed-up. User has the option
    to switch and terminate other applications before proceeding with backup.
  * A Log file is now created and appended for each backup cycle. The Log
    lists the files that were backed-up and those that were not due to locks by
    running applications. The final statisics are also written to the log.
  * Handle creating new multi-level PC subdirectories. User can specify a
    directory like c:\psion\siena\jim when c:\psion did not initially exist.
  * There is now a web site for new versions and information.

Beta Version 0.1 expires 3/1/1997.
  Initial Release.

Things to test and report on:
    What hardware/operating systems have you tried it on and did it work?
        Psion type: Siena or 3c
        PC type   : Desktop or Notebook
        Processor : 386/486/Pentium Class  Speed xx Mhz
        IR Port   : built-in or external connected to RS-232 serial port
                    If external direct attach (like AIRport 1000) or cable
        OS        : DOS,Win3.1,WIN95,OS/2
        Comports  : did you use the xxx:i format or just 1,2,3,4

    Does backing up from drive A: and B: work? I only have a Siena.
    What is your resend rate? I get 2% or less except on one system which
                              gets 12%.
    What speeds do you get for a complete backup and what is the total number
    of files and bytes transferred?

    How did you get IRBACKUP.OPA on to your PSION? (PSIWIN,SIEMUL,IRCOPY,other?)

    Any features missing for a basic backup program?
    What needs to be added to or fixed in the readme file?
    Anything else you thing might help improve this program.

The future:
Testing-Proper setup for external IR adapters (set the baud rate)
        (Testing Actisys code in this version still need info on Adaptec's
         AIRport line)
Done   -Use native Psion IR receive key to load OPA file. Psion will think it
        is recieving a file from another Psion much like using the Siena
        Emulator.
      - Visual Basic PC program.
      - Backup time filters? (backup if newer than xx/xx/xx)
      - Unconditional backup (even if PC file is newer)
      - ???

Please E-mail me with your reports. Thanks for helping test IRBACKUP.
  Jim Pollock <jpollock@ix.netcom.com>

===========================Start of Readme file================================
                        IRBACKUP Read Me File
                          Beta Version 0.3
                             2/14/1997

CONTENTS
     What does IRBACKUP do?
     How is the data protected?
     Are All files backed-up.
     Can I backup less than the entire disk?
     How long will it take?
     What if I interrupt a backup?
     How much space do I need?
     Does IRBACKUP write any data to my PSION drives?
     Is a log file keeped?
     What IR hardware do I need?
     Where can I get the latest version?
     IMPORTANT
     DISCLAIMER
     Requirements:
     Files in this package:
     Installation:
     Running IRBACKUP.
     Aborting the program:
     How can I tell if my IR port is working:
     What if my PSION sees the PC but the PC fails to find the PSION?
     Improvements:
     Registration:


What does IRBACKUP do?
  IRBACKUP backs up all your Siena/3c files to a Notebook or Desktop computer
  equipped with an IRDA compliant IR port preserving the date/time and directory
  structure of each file. The PC files are checked and only newer files are
| sent over the IR link and backed-up. Some external IR adapters seem to work
| but are not explicitly supported (currently testing Actisys support in this
| version).

How is the data protected?
  Each data block transferred is protected by a 16 bit CRC check code
  (x^0+x^5+x^12+x^16) and a 16 bit sequence number. Bad and missing blocks are
  automatically resent until received correctly by the PC. The number of
  resent blocks is typically less than 2% of the total blocks sent.

  The resend function was tested by placing the PSION on a slowly turning
  turntable. A backup was then run while the IR beam was continually
  interrupted as the PSION slowly turned in front of the target notebook
  computer.

Are All files backed-up.
  NO. Hidden files are not backed-up. Files that are being used by a still
  running application program can not be backed-up. When starting the Psion
  server program a check is made and warning given if other applications are
  found to be avtive. A list of these applications is presented on the Psion
  screen. The user may then use the direct application keys or Shift-System
  to switch to each application and terminated it. The user can then rerun
  the test to insure that nothing else is running. The user may also ignore
  the warning and risk not backing-up everything.

Can I backup less than the entire disk?
  Yes. As little as one subdirectory or one file can be backed-up. Instead
  of specifying only a drive letter (m:) just add in a subdirectory or file
  name. You may also use wildcard characters like '?' or '*'. Example path
  names:
          m:\opl               (all files in directory opl)
          m:\o*                (all subdirectories that start with 'o')
          m:\app\irbackup.opa  (a single file
          m:\app\*.app         (a group of files with the same extension)

Note: IRBACKUP uses the OPL command DIR$ to scan for files.

How long will it take?
  Transfers are done at 19,200 baud. Since the PSION server code must scan your
  entire disk there is a fixed time based on the number of files you have on
  your PSION even if no files are transferred. For example if you have 140 files
  it will take the PSION approximately 40 seconds to scan and send the
  directory, name, date, time, and size of each file to your PC. The actual
  transfer of the file depends on its size and the PC/operating system the
  client code is running on. Typical DOS times are 1450 bytes/sec or 88KB per
  minute. Providing a path name instead of just a drive letter reduces the
  number of files that need to be scan and therefore the time.

  The complete backup of a Siena which has 600KB of data in 138 files takes
  less than 7 minutes.

What if I interrupt a backup?
  A backup can be suspended by breaking the IR link or aborting at any time.
  If you pick up the PSION, switch to another app, and at a later time switch
  back placing the PSION within range again the backup will startup where it
  left off. There are NO timeouts on retries in either the PSION or PC. If
  you abort either the PSION (Psion-ESC) or PC (left&right shift) you must
  abort the other as well. When you restart both backup programs they skip over
  all the previously backed-up files until they reach the files that were not
  backed-up and start backing up from there.

How much space do I need?
  The OPA file is about 4.8KB and when running takes about 30KB.

Does IRBACKUP write any data to my PSION drives?
   NO. IRBACKUP only reads the directory information about a file and when
   sending the file to the PC it reads the data in the file. The file is opened
   in read only/share mode.

Is a log file keeped?
   Yes, the results of each backup cycle are written to a log file in the
   starting directory for the backup. The file name is 'BCKUPLOG.TXT'.
   The size of this file grows with each backup taken so it may be necessary to
   prune (remove older lines from the top) the file from time to time.

What IR hardware do I need?
   Any Notebook or Desktop with a built-in IR port should work. An external
   unit such as Adaptec's AIRport 1000 and 2000 may also work. External IR
   adapters that attach to the RS-232 serial port require an extra setup
|  sequence to set the baud rate to the same rate as the serial port's. This
|  version of IRBACKUP now includes code to setup the Actisys IR220 external
|  adapter if the command line parameter /ACT is used.

   IRBACKUP was tested using both the AIRport 1000 and 2000 so it does seem
   that whatever default baud rate they use it is compatible with the one
   IRBACKUP uses. However, efforts are being made to determine the correct
   setup sequence (for the AIRport line) and when available it will be
   included in a future version of IRBACKUP.

Where can I get the latest version?
   A web site has been setup at:

       http://www.geocities.com/SiliconValley/Lakes/3947

   It has the latest version of IRBACKUP plus access to other Psion IR programs
   and information about current and future plans.

IMPORTANT  IMPORTANT  IMPORTANT  IMPORTANT  IMPORTANT  IMPORTANT  IMPORTANT
==============================================================================
It is highly recommended that you verify the accuracy of a few large files
after doing your first backup. If you have text files, open an editor and view
the file, checking the beginning and end for correctness. If you have a binary
compare program try comparing the backed-up version of a program with a
known good copy (one you previously loaded into your PSION from your PC).
==============================================================================
                             DISCLAIMER
Use this software at your own risk. I will not be held responsible for any
damage to you, your machine and/or your information as a result of using
IRBACKUP.
==============================================================================

Requirements:
  PSION: Siena or 3c
  PC   : x86 with IRDA compliant IR port
  Operating System: DOS, Windows 3.1, OS/2 2.1, OS/2 Warp 3,
                    WIN95 (MS-DOS mode only)
  Optional Additional Software: Siena Emulator from WWW.PSION.COM

Files in this package:
  IRBACKUP.COM    Backup Client software. Runs on your PC.
  IRBACKUP.OPA    Backup Server software. Runs on your Siena/3c.
  README.TXT      This file.

Installation:
| This version of IRBACKUP now includes support to use the PSION native
| IR Receive key to transfer the IRBACKUP.OPA server code to your PSION.
|
| Press the IR Receive key on your PSION then type the following on your PC:
|
|      IRBACKUP 1 send      (where 1 is your comport 1,2,3 or 4 or xxx:i )
|      IRBACKUP 1 send /ACT (or this line if you have the Actisys IR unit)
|
| If that fails to work on your system then copy IRBACKUP.OPA to your PSION's
  \APP directory using either PSIWIN 1.1 and a serial cable, running the
  SIEMUL.EXE on your notebook, or using my IRCOPY program from my web page.

  Running SIEMUL:
    After starting SIEMUL on your PC highlight IRBACKUP.OPA on your PC and
    press Cntrl-Alt-Shift B to send the file from your PC to your PSION.

  Once IRBACKUP.OPA is on your PSION use Psion-I to install it.

Running IRBACKUP.
  Be sure to set the IR Link to run at HIGH power. From the system screen press
  Psion-L select Infrared then Power setting to HIGH. Once you have selected
  HIGH you can go back and select NONE since IRBACKUP.OPA will automatically
  turn on the IR link when needed. You can not have the Serial cable selected
  only NONE or Infrared. If you get the error 'File or device in use' when
  starting IRBACKUP make sure you do not have the Serial cable selected.

  Syntax of IRBACKUP.COM:

|            irbackup <IR port> <Psion Drive> <PC path> <Block size> <ext type>

| default value         none        m:         current      2048     builtin

|     range            1 2 3 4    a: b: m:     c:\mine   128 - 4096  /ACTisys
|                       xxx:i    m:\opl         *=current
                                m:\app\*.opa
                  Where xxx is the com port address and i is the IRQ
    Examples:
      irbackup 2             (IR on COM2, Backup M: drive using current path)
      irbackup 1 a: c:\jim   (IR on COM1, Backup A: drive to path c:\jim)
      irbackup 3E8:9 b: e:\jean (Custom com 3 port using interrupt 9)
|     irbackup 3 m: * /ACT   (backup to current directory with Actisys IR unit)

  You can start either the PSION or PC program first. It does not matter which
  and they do not have to be in line of sight of the other IR detector when
  started. Once both IRBACKUP.OPA and IRBACKUP.COM are running align the two IR
  windows so they are facing each other and no farther than three feet apart.

  The PC will display a scrolling list of file sizes, paths, and names for each
  file backed-up. A series of dots is displayed for each file skipped either
  because it has the same date/time as the one on the PC or the PC file is
  newer. You may also see the 'In-Use' warning message if you did not shutdown
  all the applications that access files (Agenda, Database). A running program
  can lock its files preventing IRBACKUP from accessing them.

  During the actual transfer of the file there is a sequence of digits
  displayed to the right of the file name after the open bracket '['. Each
  digit represents the low order three bits of the unique sequence number
  for the block currently being transferred. The digit wraps back to 0 after
  reaching 7. The sequence number is incremented for each block successfully
  transferred. If a block is resent because of an error you will see a single
  letter code followed by the same digit. The code gives the reason for the
  resend as define in the following table:

    k    = Timed out looking for sync sequence at start of data block
    t<n> = Timed out waiting for all bytes of data block
           where n = number of bytes not received
    c    = CRC error on data block
    s    = All data received, CRC good, but wrong sequence number.

  The default data block size for data is 2KB.

Aborting the program:
  To abort a backup on the PC just hold down both the left and right shift
  keys for a few seconds. Then after the statistics have been displayed press
  any key.

  To abort the PSION IRBACKUP program press Psion-Esc or go to the system
  screen and highlight IRBACKUP then press the 'Del' key.

How can I tell if my IR port is working:
  If you have a camcorder get it out and go into record standby mode and
  look through the viewfinder. You should see a flashing light coming from
  the IR window on your PSION or Notebook when they are sending. In IRBACKUP
  the PC is the master and starts sending first so look for a flashing light
  from your PC (you don't even need the PSION running at this point). You
  should see a flash about once a second as the PC looks for the PSION. Next
  startup your PSION and place it so you can watch its IR window for the
  return response.

What if my PSION sees the PC but the PC fails to find the PSION?
  If you are using the wrong interrupt level what you normally see is the
  PC locked looking for the PSION and the PSION screen says PC found. Holding
| down both shift keys may not have any effect and you may need to do a
  hard reset (power off/on PC). Some notebooks change their interrupt
  assignments based on whether they are in a docking station or not. The
  TP-701 has three sets of settings for undocked, with port replicator, and
  docked.

Improvements:
  If you have any suggestions for improvements please email me with them. If
  you can come up with a better icon please send it to me. If I use it you
  will become an instant registered user.

Registration:
  If you find this program useful please register your copy. Registered users
  will have access to newer versions with more features. A Visual Basic version
  of the PC program is planned which should be more user friendly and run
  better on WIN95/NT machines.

  Currently registration is only through the mail. Mail $15 (US currency only
  please) to:

    Jim Pollock
    7194 Silver Lode Lane
    San Jose, CA 95120-3340 USA

  Be sure to include your email address and the version number of your current
  copy of IRBACKUP.

Jim Pollock <jpollock@ix.netcom.com>
===========================End of Readme======================================
