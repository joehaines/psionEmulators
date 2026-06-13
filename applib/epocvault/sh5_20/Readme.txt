                       Shell5 V2.0
                       ===========

              Copyright (c) Nick Murray 1998


Introduction
------------

Shell5 is a command line shell for the Psion Series 5 which
allows basic file operations such as copy, delete, rename, etc,
as well as more sophisticated operations.

Highlights:
      User-defined keys/macros
      Filename completion via the Tab key
      "Time" command for measuring the execution time for commands
      A 'pipe' like feature allowing the output of one command to
       form the input of another
      Command history accessed with the cursor keys
      Multiple commands per line, separation by ';'
      Support for external, user written, commands
      Access to ROM (Z:) filesystems
      Relative pathnames (. ..)
      Wildcards on filenames (e.g. cp *.bat a:\bat)
      Command aliasing
      Redirection of command input and output (e.g. ls > ls.out)
      Shell variables, allowing extensive configuration
      Support for batch files including parameter passing and the
       additional commands IF, GOTO and SHIFT
      Ability to run .OPO and .APP files (inc. parameters)
      Pop-up error log window
      Inline arithmetic evaluation, e.g. echo 1+1 is ${1+1}
      Sophisticated 'path' mechanism
      Extensive help
      Full source code available
      Distributed under the GNU General Public License

Commands: alias, at, banner, bg, bindkey, cat, cd, chmod, cls, cp, date,
(Builtin) df, dirs, echo, edit, exit, goto, hash, help, history, if, log, 
          ls, mkdir, more, mv, od, pause, popd, pushd, pwd, rename,
          rescan, rm, rmdir, set, shift, sysinfo, time, unalias, unset,
          ver, which

Supplied as external modules:
  backlight, charset, compact, contrast, du, getclip, play, putclip, wc

More detailed information is contained within the program's help
system and visit the WWW site at:

     http://www.geocities.com/SiliconValley/Pines/1439/shell.html


Installation
------------
Unzip the file sh5_20.zip
Copy the Shell5 directory to \System\Apps on c: or d:
- this includes the application, .aif file,the default autoexec.bat,
  the help files in the "help" subdirectory and supplied external
  commands (.OPO) in the "bin" subdirectory.
If you haven't already installed Psion's sysram1.opx (many applications
use this) copy the file Sysram1.opx to the directory \System\Opx,
creating the directory if necessary.


Customising the installation
----------------------------
On startup, the program queries its pathname and looks for a file
"autoexec.bat" (which should only contain Shell5 commands) in the same
directory which it then executes. The shell variable $_syspath is also
set to this directory. The path variable (the list of directories the
shell searches to find external commands) defaults to $_syspath\bin,
and the helppath variable (the list of directories the shell searches
to find help files) defaults to $_syspath\help.
 Both of these variables can be reset in autoexec.bat.


Files included
--------------
Shell5\autoexec.bat
Shell5\shell5.aif
Shell5\shell5.app
Shell5\help\external.hlp
Shell5\help\shell5.hlp
Shell5\bin\backlight.opo
Shell5\bin\charset.opo
Shell5\bin\contrast.opo
Shell5\bin\compact.opo
Shell5\bin\du.opo
Shell5\bin\getclip.opo
Shell5\bin\putclip.opo
Shell5\bin\play.opo
Shell5\bin\wc.opo
Readme.txt - this file
Program.txt - a description of how to write external commands
Copying.txt - GNU General Public Licence
Changes.txt - recent changes to the program
Sysram1.opx - Additional System functions .OPX (written by Psion)

Source is always available (in both OPL and text formats) at the WWW
site listed below. If you have difficulties obtaining the files from
that site, email me at the address below and I'll email you the
source.


External Commands
-----------------
If you write any useful external commands that you'd like to make
available to other users, either send me the source or drop me a
note of where it's available, and I'll include this information in
the next release.


Bugs, suggestions, etc.
-----------------------
I can be reached via email at nick-murray@geocities.com and visit
the WWW page: http://www.geocities.com/SiliconValley/Pines/1439/shell.html


Distribution and Warranty
-------------------------
This software comes with ABSOLUTELY NO WARRANTY; for details see the file
Copying.txt, Sections 11 and 12. This is free software, and you are
welcome to distribute it under certain conditions; again, for details see
the file Copying.txt.
