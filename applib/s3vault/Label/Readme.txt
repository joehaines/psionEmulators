Label for the Psion S3a (version 2.0)
_______________________________________________

The complete Freeware package consists of this Readme file plus Label.opl and Label.dbf
Copy Label.opl to your \opl directory. When you have read the following and made any necessary changes, translate and run the file. 
Feel free to adapt this programme in any way but you are not authorised to incorporate it into any product for sale. The copyright for this software belongs to the author.
Label.dbf - An example database.

SUMMARY
____________

I wanted a simple programme to print Xmas card labels from my database.  The programme prints labels from an address database after converting the first line i.e. John & Sally Smith becomes Mr. & Mrs. J. Smith. I then made the conversion optional so that it would cope with a database of company addresses.

MORE DETAIL
________________

The programme searches for records in an address database based on search criteria which is entered. The default criteria is an X as the first character in the 10th line of the database (Notes).  It then converts the first line of the database according to set rules to produce a title line containing Mr, Mrs etc It prints to an A4 sheet containing 3 labels horizontally and 8 vertically. 

As the print routine first uses up labels towards the end of the sheet to ensure a half used sheet feeds better through the printer, the position of the last remaining label can be entered; otherwise it assumes a complete sheet. This means there is no wastage for small numbers of labels.

An option allows the programme to print to the screen as a check.


Changes you may have to make:

REM statements in the programme point out possible areas to change.
The programme will have to be amended to cater for non-Epson printers and character sets. It will also need amending if you want to change from the following database setup:
Line  1-Name
Lines 2 -4 Telephone numbers or other non-printed data
Lines 5-9 Address lines
Line 10 A non-printed data field ("Notes") the first character ONLY of which is matched to inputted criteria to determine which records are printed. Alternatively all fields (lines) can be searched for up to 12 characters but this 10th field still has to exist.

Title Conversion Rules

The programme converts:
John & Sally Smith to Mr. & Mrs. J. Smith
John Smith to Mr. J. Smith
Mrs Sally Smith to Mrs. S. Smith
Miss Sally Smith to Miss S. Smith
Dr John & Sally Smith to Dr. & Mrs. S. Smith
Dr John Smith to Dr. J. Smith

(Note that the ampersand {&} is required rather than "and" but punctuation after Mrs. etc. in the original database is optional)

Known Bugs

It converts Dr John & Dr Sally Smith to Dr. J. & Mrs. Smith
It does not correctly translate the return if an extra line has been created in the address lines.
No traps for files in use.
No traps for name and address lines longer than 35 characters or notes longer than 128 characters
In fact, no error routines of any kind!


Disclaimer

Label is a freeware application. The author cannot be held responsible for any damage which might result from its use.


Praise and comment to the author below:

Richard Churchill

Rick_Churchill@CompuServe.com
