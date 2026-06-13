                            Plotuw8 v.1.3a
                            ==============
                 Shareware proposed by Guillaume Dupont
                       CompuServe ID 100740,1033
                    email: 100740.1033@compuserve.com



Key words: S3A S3C PLOT GRAPHICS STOCKS FINANCE HEALTH WEIGHT


Introduction.
=============
Track the price of your favourite stocks, compare their variations to
an index, plot your weight, a temperature,... Plotuw8 is a simple and
easy to use plotting application: record a variable every day, every
week, or on any day you fancy, and Plotuw8 will instantly update a
graph, automatically adjusting axis and scales to display all records.

Plotuw8 allows to display up to 5 different graphs on the same screen,
zoom in on a particular time zone, delete records between two
dates... One can instantly create a new graph, import data from a text
file, and update a whole set of graphs at once, or bring data from
another application (e.g. spreadsheet),...


Content.
========
The plotuw8.zip file contains the following:
- readme.txt is this documentation
- plotuw8.opa is the application file
- plotuw8.rsc is a help file
- sample.plo is an example
- changes.txt summarizes major changes from previous versions.


Installation.
=============
To install Plotuw8:
- copy plotuw8.opa to any \app\ directory
- copy plotuw8.rsc and sample.plo to the \opd\ directory on the
  internal disk.
- install the application like any other Psion application, by
  pressing <Psion-I> in the system screen...

To install version 1.3a if you had installed a previous version:
- replace the existing plotuw8.opa (in an \app\ directory) and
  plotuw8.rsc (in the \opd\plotuw8\ directory on the internal
  disk) with the newly supplied files

The graphs (.plo files) created with previous versions are
compatible with version 1.3a.
 

Files / disk space management.
==============================
The plotuw8.opa file can be located on any disk, in an \app\ directory.

When the programme is run for the first time, an \opd\plotuw8\
subdirectory is created on the internal disk. A plotuw8.cfg file is
created and stored in the m:\opd\plotuw8\ directory. This
file must remain in that directory on the internal disk.

At the first run all the .plo graph files found in the internal disk's
\opd\ directory are also moved to the m:\opd\plotuw8\ directory.
The user is then free to create other \opd\plotuw8\ on other disks
(not flash disks!!), and create new graph files, or move existing ones
there.

At the first run the help file plotw8.rsc, if found in the \opd\
directory on the internal disk, is moved to the m:\opd\plotuw8\
directory. The user can move it later to any other \opd\plotuw8\
directory on any disk.


Operation.
==========
Plotuw8 can be started like any other standard Psion application:
- to use an existing graph, highlight its name under the Plotuw8
  icon and press <Enter>
- to create a new graph press <PsionðN> while the cursor is under
  the Plotuw8 icon.
- to delete a graph, exit the Plotuw8 programme by pressing <Psion-X>,
  highlight the name of the file under the Plotuw8 icon in the system
  screen, and press <Psion-D>. Then confirm with <Y>.

Press <Help> for help, or any other key to open the menu. The
different commands are:

- <Psion-R> to create one or more new records in the currently
  displayed graph: key in the value and the date in the dialog window.
  Then press <Tab> (to enter new records) or <Enter> (after the last
  record).

- <Psion-U> to spot and update, or delete, one particular record. A
  vertical line appears on the screen at the level of the last record
  (adjust contrast with Psion-> or Psion-< if necessary). Move it to
  the target point with the arrows (plus <Ctrl> or <Psion> for speed),
  and press <Enter>. A window opens. You can then update the
  record (and press <U>) or delete it (press <D>).

- <Psion-D> to delete all records between 2 dates. The selection
  process is the same as for "Zoom" (see Psion-Z under).

- <Psion-P> to display all records as a percentage of the value at
  one reference date. Useful to assess the relative variations of
  a variable, or to compare the relative evolution of two or more
  variables. Do the following:
   * open the graph corresponding to one variable,
   * press <Psion-A> (see after) to display a second variable on
     the same screen,
   * press <Psion-P>,
   * select the date at which both variable will be 100%,
   * press <Enter> to see the modified graph.

- <Psion-Z> to zoom in on a particular zone: a vertical grey line
  appears at the centre of the screen (adjust contrast if necessary).
  The selection process is the same as in standard Psion applications:
  move the line with the left and right arrows (plus <Ctrl> for speed,
  or <Psion> to reach the screen border), then anchor the line by
  pressing <Shift> and open a grey selection window  by pressing one
  arrow (plus <Ctrl> or <Psion> for speed), while <Shift> is kept
  pressed. A special information window in the upper left part of the
  screen shows the limit dates of the selection. Press <Enter> to
  confirm the selection.

  Press <Esc> or <Psion-Z> to exit the zoom mode, or any other key
  to open a menu.
 
- <Psion-V> to open a window, where records are displayed (date and
  value, 8 at a time). Use Up/Down arrows,  PgUp/PgDn or Ctrl-PgUp/
  PgDn to move in the window, or Right/Left arrows  to move the window
  itself. <Esc> to close it.
  
- <Psion-A> to display another graph on the same screen. Up to 5
  graphs can be displayed simultaneously on one screen. Press <Tab>
  to display a legend.
 
- <Psion-O> to open an existing graph

- <Psion-N> to create a new graph. Use any valid Psion file name (8
  characters max,...).

- <Psion-M> to merge another graph into the displayed graph: the
  points of the second graph become part of the displayed graph,
  permanently. The merged graph file is kept intact.

- <Psion-S> to save the graph under a new name.
 
- <Psion-I> to import data from a text file. Three options are now
  available:
  
  "Several existing files, one date":
  
  a typical example is importing the quotes of a set of shares,
  downloaded at one date into a text file, like quotes.cis obtained
  with ReadCis. Plotuw8 works from a "Labels" table, where one label
  or "ticker" corresponds to each name of graph (see <Psion-L>
  under). The programme will look for one share in each line of text,
  looking first for the label, then for the value, in the columns
  specified by the user. In the case of quotes.cis the labels are
  found in column 1, and the last quotes in column 5. The file looks
  like this (the first 2 lines, which contain no matching labels are
  ignored by the programme):
  
<<

Ticker        Volume    High       Low      Last    Change Cur  Updated
------------  ------  --------   -------  --------  ------ --- ----------
AIRP.PA          715   797.000   786.000   792.000  +2.000 FRF  5:00 CET
ACCP.PA          616   633.000   623.000   626.000  -3.000 FRF  5:00 CET
AXAF.PA         2684   314.800   310.700   313.200  +1.600 FRF  5:00 CET
CCFP.PA         1034   233.600   231.100   232.900  +1.400 FRF  4:59 CET
EUTL.PA        36437     7.900     7.700     7.800  +0.100 FRF  5:00 CET
LAFP.PA         2786   305.900   300.000   302.800  -0.800 FRF  5:00 CET
TOTF.PA         3369   408.000   401.500   405.500  -0.500 FRF  5:00 CET
TFFP.PA           92   573.000   560.000   569.000  +5.000 FRF  5:00 CET
CGEP.PA         2676   451.400   444.200   446.500  -5.000 FRF  5:00 CET
MWDP.PA           55   465.000   459.000   465.000  +5.000 FRF  4:59 CET

Issue:

>>
  
  The user must answer several questions, before the import starts:
    * name of the import text file
    * date of the records
    * position of the column where the labels, or tickers will be
      found (column 1 in this case)
    * position of the column where the quotes will be found (column
      5 in this case)
    * type of separators ("spaces and tabulations" in this case).
 
  When the name of the import file has been selected, its first few
  lines are displayed in a background window, where the user can
  check the structure, count the columns,...
  
 
  "New graph, several dates" or "One existing graph, several dates":

  In this case, once the import text file has been chosen, and a 
  graph name has been selected or created, Plotuw8 will look for one
  record per line of text. The format for entries is:

   xxx  dd*mm*yy  xxx  data
   or
   xxx dd*mm*yyyy xxx data

  where dd*mm*yy or dd*mm*yyyy is the date, xxx and * can be any non
  numerical set of characters, and data is the value (separators are
  interpreted according to the system settings).

  The following is a valid example of a text file for import, which
  will produce a graph with 5 points (assuming that the system
  decimal point is "Dot"):

               le 24.5.94	33.2
               le 12.6.1994	++++   35.6
               15/7/94	anytext  38.6
               anything   20**08**94        37
               25-8-1994   35

- <Psion-B> to bring data from another application and create a new
  graph. Any text, which has been highlighted in another application
  supporting Link Paste (word, data, spreadsheet,...) will be brought
  in and interpreted in the same way as in the "import" option (see
  above). 

- <Psion-S> to save the graph under a new name (one can do it
  before trying the next command "compress"...).
  
- <Psion-K> to "compress" the displayed graph: although Plotuw8
  graph files are extremely compact, one may wish to "compress"
  them even further, in order either to save disk space, or to make
  them more readable. The "compress" option proposes to erase
  some points. One can then accept the proposed compression
  (press <Psion-K> again), or cancel it (press <Psion-C>). After a
  compression, it may be possible to compress even further, until
  the proposed compression factor becomes 0%.
 
- <Psion-L> to edit the "Labels" table for import labels (see above).
  The table is displayed on the right side of the screen, and a series
  of command buttons on the left side. Note that there can be several
  labels for one graph name, but only one graph for one label...

- Press <Help> for the help menu.


Registration.
=============
Plotuw8 is Shareware. You can use it freely during 14 days. After
this period, if you find it useful, please register. Otherwise
remove it from your computer. Registration fee is US$15.00.

You can register on CompuServe via SWREG (GO SWREG.
Registration ID: 10551).

Alternatively you can send US$15, GBP10, DEM20, CHF20, or FRF70 in
cash by mail. Send bank notes, no checks please. Do not forget to
send your e-mail address with payment. Note that sending cash by
mail is at sender's risk. My address is:

Guillaume Dupont
25, rue Pauline Borghese
92200 Neuilly sur Seine
France

Upgrade from previous versions is free.

Non registered version allows to plot 15 points only on each graph
(although an indefinite number can be recorded). Registered users
will receive via e-mail a registration code which allows to plot full
graphs. This code must be entered in the registration window
(<Psion-W>, then <R>) exactly as mailed (spaces, upper cases...).


Disclaimer of responsibility.
=============================
Users of Plotuw8 must accept this disclaimer of warranty: "Plotuw8
is supplied as is. The author disclaims all warranties, expressed or
implied, including, without limitation, warranties of fitness for
any purpose. The author assumes no liability for damages, direct or
consequential, which may result from the use of Plotuw8 or any of its
accompanying files".


Last update 17 May 1997.

