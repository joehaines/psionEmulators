                    Shell3a V1.70
                    =============
                    
              Copyright (c) Nick Murray 1996
                       
                    
Introduction
------------

Shell3a is a command line shell for the Psion Series 3a which
allows basic file operations such as copy, delete, rename, etc,
as well as more sophisticated operations.

Highlights:
<NEW> A 'pipe' like feature allowing the output of one command to
       form the input of another
<NEW> Input redirection
<NEW> Multiple commands per line, separation by ';'
      Command history accessed with the cursor keys
      Support for external, user written, commands
      Access to ROM (rom::) and remote (rem::) filesystems
      Relative pathnames (. ..)
      Wildcards on filenames (e.g. cp *.bat a:\bat)
      Command aliasing 
      Output redirection to a file (e.g. ls > ls.out)
      Shell variables, allowing extensive configuration
      Support for batch files including parameter passing and the
       additional commands IF, GOTO and SHIFT
      Extensive help
      Ability to run .OPA and .APP files (inc. parameters)
      [experimental] Pop-up error log window
      Inline arithmetic evaluation, e.g. echo 1+1 is ${1+1}
      Sophisticated 'path' mechanism

Commands: alias, at, banner, bg, cat, cd, chmod, cls, cp, date, df,
(Builtin) echo, edit, exit, goto, hash, history, if, ls, man, mem
          mkdir, more, mv, od, pause, pwd, rename, rescan, rm,
          rmdir, set, shift, unalias, unset, ver, which

Supplied as external modules: battery, du, kill, play, ps, setpr
(inc. source and help file)   view, wc

More detailed information is contained within the program's help
system. The help system is based on Tom Dolbilin's OPL Help System,
so if you've already installed this you won't need sys$help.opo.


Installation
------------

1) Copy SHELL_3A.OPA to \APP on A: B: or the internal disk.

2) Copy AUTOEXEC.BAT to \APP\SHELL3A on the same drive as above,
  or to the root directory, \, on any disk.

3) Install SHELL_3A.OPA onto the desktop using PSION-I.
[the rest of the installation can be performed using the shell and
 is only necessary to use the help and supplied external commands]

4) Create a directory (eg. \BIN) and copy the .OPO files into it.

5) Create a directory (eg. \HLP) and copy the .HLP files into it.

6) Set the 'path' and 'helppath' variables respectively in the
 AUTOEXEC.BAT files to reflect the directories chosen in (4) & (5)
 [ Changing the autoexec.bat file can best be done via the shell
 'edit' command. ]

HINT: If you want to keep all the Shell3a files together, use a
      single directory (eg. M:\APP\SHELL3A) for both the .OPO and
      .HLP files.

Customisations (usually set in AUTOEXEC.BAT)
--------------
(1) path - This is the list of directories in which the shell will
   look for .BAT, .OPO, .OPA and .APP files. Each time the path is
   searched and a command found, the location is recorded (HASHED).
   This hash table is cleared when the path is changed and can be
   directly manipulated via the 'hash' command. In order for the
   help system to work, SYS$HELP.OPO must be in one of the
   directories of the path.
    eg. set path="loc::m:/opo,loc::m:/bin,loc::m:/app/shell3a"

(2) helppath - The directories in which the help files are stored.
   If help is invoked for a command, eg. mycom, that isn't built
   into the shell, a help file 'MYCOM.HLP' is searched for in
   these directories. This allows help to be provided for user
   written commands.
    eg. set helppath="loc::m:/hlp,loc::a:/hlp"

(3) Setting path and helppath should allow you to view the online
   help. See the *(v) sections at the end of the online help for
   the remaining shell settings.


Files included:

 \APP\SHELL_3A.OPA - the main application
 \APP\SHELL3A\AUTOEXEC.BAT - a sample start up file
 \APP\SHELL3A\SHELL3A.HLP - the help file
 \APP\SHELL3A\DOS.BAT - Aliases more familiar to MS-DOS users.
 \APP\SHELL3A\BATTERY.OPO - command to display battery status
 \APP\SHELL3A\DU.OPO - show disk usage by directory
 \APP\SHELL3A\KILL.OPO - terminate a process
 \APP\SHELL3A\PLAY.OPO - play a .WVE sound file
 \APP\SHELL3A\PS.OPO - display the running processes
 \APP\SHELL3A\SETPR.OPO - change the priority of a process 
 \APP\SHELL3A\VIEW.OPO - show .PIC files
 \APP\SHELL3A\WC.OPO - count the lines, words and chars. in a file
 \APP\SHELL3A\BATTERY.HLP - help for the above commands
 \APP\SHELL3A\DU.HLP
 \APP\SHELL3A\KILL.HLP
 \APP\SHELL3A\PLAY.HLP
 \APP\SHELL3A\PS.HLP
 \APP\SHELL3A\SETPR.HLP
 \APP\SHELL3A\VIEW.HLP
 \APP\SHELL3A\WC.HLP
 \APP\SHELL3A\HCP.OPO - Tom Dolbilin's OPL Help System
 \APP\SHELL3A\SYS$HELP.OPO - "    "     "   "      "
 README.TXT - this file
 PROGRAM.TXT - a description of how to write external commands
 SRC.ZIP - ZIP file containing the source for the 8 external commands


New features in this version
----------------------------

1) Redirection of input into commands

Input can be redirected into a command either by using the notation
'< file', e.g. more < file.txt or by PIPING the output of another
command using the '|' notation,  e.g. ls | more
       
Notes: In the example above 'file.txt' should be a text file as it
        is opened in text, rather than binary mode which restricts
        input lines to less than 255 characters.
       Batch files cannot be subject to input/output redirection
       See notes on pipes below

2) Multiple commands on an input line

Multiple commands can occur on a single command line. The separator
between commands is ';', e.g date ; pwd

Notes: ';' is close to the standard for separation of multiple
        commands within a single command line. As ';' was already
        used to separate pathname components, I have had to change
        the pathname separator to ','.
       If an error occurs in one of the commands the subsequent
        commands are still executed, unless it was an error caught
        in the initial parse of a command (e.g. bad wildcards).

3) Implementation of 'pipes'

This allows the output of one command to form the input of another
command, e.g. ls | more
 Commands must be specially coded for this to work. On the output
side, if the shell flags to the command that the output is
redirected, the command should write output to the handle SHout%
using the IOWRITE command. This is automatically handled if fprint
is used to produce the output.
 On the input side, if the shell flags the input is redirected, the
command should read input from the handle SHin% using the IOREAD
command.
 The shell builtins more, od and cat have been modified to accept
redirected input. They were the only commands that already took
input from a file.

Notes: The pipe character |, char 179 can be produced within the
        shell by pressing <PSION> +. Otherwise use <CONTROL>179.
       Pipes are implemented by using intermediate files to which
        output is written and read. These files are LOC::M:\|.1
        and LOC::M:\|.2. Data between processes should be text as
        the intermediate files are opened in text not binary mode
        (to make reading them easier!).
       Batch files cannot appear in pipes.
       If a command doesn't accept redirected input the input is
        ignored.
       If a command doesn't produce redirected output the input
        to the next command will be empty, but it will still
        execute. If an error occurs in the pipe, the subsequent
        commands are still executed, albeit with empty input
        (except when it' an error with redirection or wildcards).
       If an alias is used in a pipe, multiple command or input/
        output redirection there MUST be a space between the alias
        and the '|', '>','<' or ';' symbol.
        e.g. alias h history
              Work        Fail
             h | more    h| more
             h |more     h|more
        This isn't a new bug, it exists due to the way aliases are
        detected.
       Within batch files, GOTO commands and labels may behave
       stangely if they are part of multiple command line.

4) A new command to count the number of lines, words and
 characters in a file - wc. This is really just an example of code
 that can use input redirection.
 

Changes between V1.70 and V1.60
-------------------------------
- change of separator from ';' to ',' in PATHs
- enhanced the 'man' command to include searching for topics
 and command-line access to any topic
- ability to have AUTOEXEC.BAT file in \APP\SHELL3A directory
- bug fixes for ls (corrupt output on remote drives) and play
 (could crash the machine under certain circumstances)


New features in V1.60
---------------------
- Log window. This displays error and information messages that
 previously were displayed on the text screen. Currently these are
 the messages from "rescan" and bad path/helppath component
 warnings. This window is shown during initialization and then
 hidden. The  key toggles the window.

- ${expression}. This allows the evaluation (using the OPL EVAL
 construct) of arithmetic expressions. This allows the numeric
 manipulation of shell variables. eg. set i=${$i-1}. This removes
 the need for a separate "EVAL" command.

- The path searching has been reworked. In previous versions the
 list of commands found in the directories of the path were stored
 when the path was set and never changed. Although this was fast,
 it meant relative paths couldn't be used (eg. '.') and new files
 weren't automatically found (hence the rehash command).
  The new algorithm searches the path when an unknown command is
 encountered and stores the path if it is found. The "hash" command
 is used to manipulate the hash table.

- External command execution. In previous versions, a separate
 command "exec" was needed to run .OPA and non-shell .OPO files.
 This has been reworked and any .BAT, .OPO, .OPA or .APP files can
 be run by typing it's full pathname or just the name if it is in
 a directory in the path. The extension is optional now, and if
 one isn't supplied the search order is BAT, OPO, OPA then APP. If
 "." isn't in your path programs in the current directory can be
 accessed as "./program".
  A single parameter can be supplied to an OPA or APP. Some
 applications EXPECT an argument and will fail without one. Others
 ignore the argument, or perform unusually if one is given!
 A word of warning - some APPs don't like being executed like this.
 For example running ROM::RUNOPL.APP or ROM::RUNIMG.APP causes a
 soft reset! - I've never found out exactly why.
  
Changes between V1.60 and V1.40
-------------------------------
- Back to 1 executable
- Variables are prepended either by $ or % depending on "unixvar"
- Loss of eval, exec and rehash commands; their functionality has
 been moved elsewhere (see new features above).
- Extra commands: mem, banner, battery, view, setpr, du
- Use of the OPL cache

New features in V1.40
---------------------
- The number of commands stored in the history can be set via the
 'set' command
- There is no longer any limit on the number or size of aliases.
- batch files can now take variables which are are accessed using
 %0 .. %9
- Shell variables have been introduced. These are specified as
 %string
- Settings such as path, helppath, etc are implemented as shell
 variables
- new IF command (DOS syntax)
- SHIFT, GOTO and labels in batch files.

Changes between V1.40 and V1.23
-------------------------------
- The program now consists of 2 parts, shell3a.opa and commands.opo
 which contains all the user commands.
- As shell variables use the '%' DOS convention, prompt 'variables'
 are now referred as $H and $p rather than %H and %p.
 
New features in V1.23
---------------------
- Items in the command history are accessed using the up/down
 cursor keys
- The <Help> key brings up the main help menu.
- Support for external modules and hence user-expansion of the
 capabilities of the shell.

Changes between V1.23 and V1.02
-------------------------------
- The comment string in .BAT files is now '#'
- The Mac 3-link can now be used. Pathnames and device names with
 spaces have to be quoted, eg. cd "rem::Hard Disk:".
- The path is no longer searched each time a command that doesn't
 match a built-in is found. Instead a list of .OPO of .BAT files
 found in the path is stored and searched. This means external
 .OPO modules are executed with little noticable delay. Members of
 the list are accessed WITHOUT the suffix .BAT or .OPO.
- Separate current directory for each device. Because of this
 all possible devices are scanned when the program starts. If
 devices are added the command 'rescan' must be used to recognize
 the new device. Note the rescan command has the side-effect of
 resetting the current working directory.
- 'df' without arguments show a summary of all devices


Acknowledgements
----------------
Psionics files by Clive Feather
Tom Dolbilin for his excellent help system
Suggestions: Chris Dadd
             mike@btwelve.demon.co.uk

External Commands
-----------------
If you write any useful external commands that you'd like to make
available to other users, either send me the source or drop me a
note of where it's available, and I'll include this information in
the next release.


Bugs, suggestions, etc.
-----------------------
I can be reached via email at nmurray@csd.abdn.ac.uk


Disclaimer
----------
The author of this software is not responsible for any damage due
to use of this program. This software is provided without warranty
of any kind.


Copyright
---------
Last and by no means least, this program is NOT public domain and I
retain the copyright. However provided you don't make money from it
this program is freely distributable.
