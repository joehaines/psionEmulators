OPLDB1.OPX
----------
Copyright (c) 1998 Otfried Cheong and RMR Software

README file
-----------

Contents:

1.	Introduction
2.	File Details
3.	Installation
4.	Using the OPX
5.	Important Notice
6.	Distributing the OPX
7.	Registration
8.	Other Programs from RMR Software


INTRODUCTION
------------

This OPX gives access to all the field types supported by EPOC/32 DBMS, including long types (text of arbitrary length). 

It also allows complete schema enquiries for OPL databases.

The DbFindWhere: function allows you to perform database searching using the full power of SQL.  You can search for any SQL expression normally used in the WHERE part of a SELECT clause.

The OPX also allows you to open and create databases with user-defined field types. In other words, the field type does not have to be hardcoded into the OPL program.

FILE DETAILS
------------

The archive consists of the following files:

README.TXT    This file
OPLDB1.OPX    This is the main OPX file.
OPLDB.OXH     This is the header file
OPLDB.OPL     This is a demonstration program that shows you how the OPX can be used.
 

INSTALLATION
------------

1.	Copy OPLDB1.OPX into the \System\Opx\ folder on either the C: or D: drive.

2.	Copy OPLDB.OXH into the \System\Opl\ folder on either the C: or D: drive

3.	Copy OPLDB.OPL any where you like.


USING THE OPX
-------------

1.	First compile and run the OPLDB.OPL file to make sure everything works.

2.	To use the OPX in your program add the following line to the top of the code, immediately after the APP...ENDA and before the first procedure 

	INCLUDE "OPLDb1.oxh"

3.	You can now use the following additional procedures in your program.


ODbGetTableCount&:(path$)
=========================

Returns the number of tables in the database with filename path$.


ODbGetTableName$:(path$, i&)
============================

Returns the name of the i&th table in database path$.


ODbGetIndexCount&:(path$, table$)
================================

Returns the number of indices for table$ in the database with filename path$.


ODbGetIndexName$:(path$, table$, i&)
===================================

Returns the name of the i&th index for table$ in the database.


ODbGetIndexDescription$:(path$, table$, index$)
==============================================

Returns a string describing the key of index$ for table$ in the database.


ODbGetFieldCount&:(dbase$,table$)
ODbGetFieldName$:(dbase$,table$,fieldNum&)
ODbGetFieldType&:(dbase$,table$,fieldNum&)
==========================================

These functions duplicate the functions of the same name in Dbase.opx. The only difference is that the functions here can also be used when some table of the dbase$ is currently open in your program.

Note that the documentation for the DbGetFieldType&: function in the first OPL manual has the type numbering wrong.  The correct numbers are:

value  type
--------------
0  bit
1  signed byte (8 bits)
2  unsigned byte (8 bits)
3  integer (16 bits)
4  unsigned integer (16 bits)
5  long integer (32 bits)
6  unsigned long integer (32 bits)
7  64-bit integer
8  single precision floating-point number (32 bits)
9  double precision floating-point number (64 bits)
10 date/time object
11 ASCII text
12 Unicode text
13 Binary
14 LongText8
15 LongText16
16 LongBinary

The four OPL types are types 3, 5, 9, and 11.


ODbGetFieldSize&:(path$, table$, i&)
===================================

Returns the size of the i&th field in table$ in the database. This is the length of string fields, otherwise it is undefined.


ODbGetCanBeEmpty%:(path$, table$, i&)
=====================================

Returns -1 if the i&th field in table$ in the database can be empty, zero otherwise.


ODbOpenR:(logicalName&, sql$, fieldTypes$)
ODbOpen:(logicalName&, sql$, fieldTypes$)
========================================

These two functions correspond to the built-in OPL commands OPENR and OPEN.  The difference is that these functions take the field types from the string fieldTypes$, and that all types supported by EPOC/32's DBMS are allowed. 

LogicalName& is a number indicating the logical Name to open:  0 is A, 1 is B, 2 is C, and so on.

FieldTypes$ must have exactly one character for every field, with the following meaning:
 "$" : an OPL string field
 "%" : an OPL integer field,
 "&" : an OPL long integer field,
 "." : an OPL real field,
 "?" : any other field.

An example:  The built-in OPL line

 OPEN "c:\test SELECT name, age, income, height FROM Employee", C, f1$, f2%, f3&, f4

would correspond to the OPX function call:

 ODbOPEN:(2, "C:\test SELECT name, age, income, height FROM Employee", "$%&.")

The two expressions are really completely identical. You can use C.F1$, C.F2% etc to access the values of the fields, or, alternatively, you can use the ODbGetXxx and ODbPutXxx functions described below.

On the other hand:

 ODbOpen:(3, "C:\test SELECT name, married, children, birthday, cv FROM Employees", "$????")

could be used if name is a string field, married a boolean field (Yes/No), children a byte field, birthday a DateTime object, and cv a long text field. You can use D.F1$ to access the name, but you will need to use ODbGetWord&:,  ODbGetInt&:, ODbGetDateTime:, and ODbGetLong: to read the values of the other fields in this database.

Use CLOSE to close a database opened using these functions.


ODbStartTable:
ODbTableField:(fieldName$, type&, length&)
ODbCreateTable:(fileName$, tableName$)
======================================

These functions can be used to create a new table with name tableName$ in the database fileName$.  Call ODbStartTable: first to start setting up the creation process. Then call ODbTableField: once for every field in the new table.  FieldName$ is the DBMS name of the field, and type& its DBMS Type (these are the same numbers as returned by the DbGetFieldType&: and ODbGetFieldType&: functions.  (Please be aware that the documentation of DbGetFieldType& in the OPL manual is wrong! See above for the correct numbers.)  The length& argument determines the maximum length of string and binary fields (Types 11 and 13). For all numeric types (Types 0 to 10), the length& argument determines whether the field can be empty. Use &0 if it can be empty (this is the normal case for OPL databases), or &1 if it cannot be empty.  Setting this argument to &1 saves one bit per field per row of your database. The ODbCreateTable: call actually creates the table. Note that this function DOES NOT open a new view on the database.  An eempty table is created in the database. If you want to write to it, you have to open it using ODbOpen:.  If the fileName$ does not exist, it is created as an OPL database file.  This mechanism does not handle the SETDOC command.  If you want to create an OPL document file, you will need to create it using OPL's create command, and then add a table using ODbCreateTable:.


ODbGetLength&:(i&)
==================

Returns the length of the i& field in the current database. If the field is empty, returns zero. Otherwise it returns 1 for any numeric field (types 0 to 10), and the length in bytes for text, binary, long text, and long binary fields. 


ODbGetString$:(i&)
==================

Return the contents of the i& field in the current database as a string. This is normally used for ASCII text and binary fields (types 11 and 13), but can actually be used for any type except for long types.Fields are counted starting from one.

The usual OPL syntax:
  name$ = C.nam$
would translate to:
  USE C
  name$ = ODbGetString$:(1)


ODbGetInt&:(i&)
===============

Return the contents of the i& field in the current database. This has to be a signed integer field (types 1, 3, 5).

The usual OPL syntax:
  age% = C.age%
  salary& = C.sal&
would translate to:
  USE C
  age% = ODbGetInt&:(2)
  salary& = ODbGetInt&:(3)


ODbGetReal:(i&)
===============

Return the contents of the i& field in the current database. This has to be an OPL real field (type 9).


ODbGetReal32:(i&)
================

Return the contents of the i& field in the current database. This has to be a short real field (type 8).


ODbGetWord&:(i&)
================

Return the contents of the i& field in the current database. This has to be an unsigned integer field (types 0, 2, 4, 6).


ODbGetDateTime:(dtime&, i&)
===========================

Get the contents of the i& field in the current database, and place it in the datetime object dtime& (which must have been created using DTNewDateTime&: or DTNow&: from the Date.opx).  The field must be a date/time field (type 10).


ODbGetLong:(buffer&, length&, i&)
=================================

Get length& bytes from the i& field in the current database, and place it in the buffer&.  The field must be a long field type (types 14, 15, 16). Normally you would first read the length of a long field using ODbGetLength&: and use this as the length& argument for this function.


ODbPutEmpty:(i&)
================

Makes field i& in the current database empty.


ODbPutString:(string$, i&)
=========================

Assigns the value of string$ to the i&th field in the current database.  This would normally be used for string or binary fields, or for long text or long binary fields, but it can actually be used for any type of field.


ODbPutInt:(no&, i&)
==================

Assigns the value of no& to the i&th field in the current database. The field must be a signed integer field (types 1, 3, 5).


ODbPutReal:(f, i&)
==================

Assigns the value of f to the i&th field in the current database. The field must be an OPL real field.


ODbPutReal32:(f, i&)
====================

Assigns the value of f to the i&th field in the current database.  The field must be a short real field (type 8).


ODbPutWord:(no&, i&)
====================

Assigns the value of no& to the i&th field in the current database.  The field must be an unsigned integer field (types 0, 2, 4, 6).


ODbPutDateTime:(dtime&, i&)
===========================

Assigns the value of the DateTime object dtime& to the i&th field in the current database.  The field must be a datetime field (type 10).


ODbPutLong:(buffer&, length&, i&)
=================================

Assigns the contents of the buffer& of length& to the i&th field in the current database.  The field must be a long field (type 14, 15, 16).


ODbFindWhere%:(sqlString$, flag%)
=================================

This function is used similar to the built-in function FINDFIELD, in particular the flag% argument and the return value have the same meaning. The difference is that the sqlString$ argument is used as an SQL query. So you can have a query like:

ODbFindWhere%:("name LIKE '*Miller*' AND (height > 1.80 OR salary < 20000)", 1)


ODbUse:(logicalName&)
=====================

Equivalent to the OPL command USE, but takes a number from 0 to 25 instead of a letter from A-Z as an argument.

So:
  USE C
is identical to
  ODbUse:(2)


IMPORTANT NOTICE
----------------

* With the Get and Put functions in this OPX you can use any of the types supported by EPOC/32 DBMS. You loose, however, the security provided by the OPL interpreter.  The OPX does no typechecking. If you use the wrong Get function, your program will crash.

* The schema enquiry functions cannot access Data databases if they are not already open in your OPL program.  This shouldn't be a problem, because (1) you can always use the Dbase.opx and (2) you can simply open the database using OPEN and then call the functions.

* If you use the possibility of specifying that a field cannot be empty, the DBMS may rearrange the order of fields in your table. This means that a view opened with "SELECT *" will not return the fields in the order that you may expect.  If you need to rely on the fields being in the order in which you specified them, you should not use the "Cannot be empty" flag.


DISTRIBUTING THE OPX
--------------------
If you wish to distribute the OPX as part of your program then you need to include OPLDB1.OPX in the ZIP archive for your program and instructions to the user that it needs to be installed in \System\Opx\ folder.   Note that you do NOT need to, and should NOT, distribute OPLDB1.OXH or OPLDB1.OPL, they are simply for use on the developers machine.  Shareware using this OPX must include this information in the "About" screen.  (A line like "Contains OPLDB1.opx © Otfried Cheong" or similar.


REGISTRATION
------------
OPLDB1.OPX is free for personal use and for use in Freeware programs.  If you wish to distribute it in a Shareware Package, then we ask that you register it by E-Mailing us at opx@rmrsoft.com.  We are asking for a nominal fee of twice the registration fee of your program.   That will get you want full backup support, such as a WINS copy, e-mailing of enhancments and influence over future development of the OPX.  Hope you think this is acceptable, we are not trying to make money on this, just cover out costs.


OTHER PROGRAMS FROM RMR
-----------------------
If you like the look of this OPX, why not have a look at our programs.  A full list is as follows:

S5BANKxx.ZIP    	:  A Personal Accounts Suite
S5TASKxx.ZIP     	:  An Extended Task (ToDo) Manager.
S5HOMExx.ZIP		:  A Home Inventory program.
S5NOTExx.ZIP		:  A Note Taker/Jotter program
S5FUELxx.ZIP     	:  A Fuel Consumption Monitor.
S5UTILSx.ZIP		:  A Utility/Conversion program

Some of these are also available in other languages, such as French, German, Spanish etc..  See the Home Page http://www.rmrsoft.com/ for details

