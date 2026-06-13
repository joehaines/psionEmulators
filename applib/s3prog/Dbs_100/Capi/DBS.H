/*		MODULE - DBS.H
Database server releasable header file

Version 1.00

Copyright (c) Psion PLC 1995.
*/

#ifndef DBS_H
#define DBS_H

#ifndef P_STD_H
#include <p_std.h>
#endif

/* Database server flags */

#define DBS_MODE_LEAVE		0x01		/* leave on errors */

#define DBS_ASCEND			0x00
#define DBS_DESCEND			0x01

#define DBS_NOTNULL         0x0001
#define DBS_NOUPDATE		0x0002

#define DBS_UNIQUE			0x0001
#define DBS_MANUAL			0x0002
#define DBS_PRIMARY         0x0008

#define DBS_DATE_CREATE     0x1
#define DBS_DATE_UPDATE     0x2

#define DBS_CREATE			0x0020
#define DBS_TEMPORARY		0x0040
#define DBS_LOCKWRITE		0x0100
#define DBS_LOCKREAD        0x0200
#define DBS_EXCLUSIVE		0x0400
#define DBS_READONLY        0x1000
#define DBS_APPENDONLY      0x2000

/* database server field types */

#define DBS_TYPE_INT16			0
#define DBS_TYPE_INT32			1
#define DBS_TYPE_DOUBLE			2
#define DBS_TYPE_STRING			3 
#define DBS_TYPE_DATE			4
#define DBS_TYPE_LOGICAL		5
#define DBS_TYPE_LONGBINARY		6
#define DBS_TYPE_INT8			7
#define DBS_TYPE_UINT8			8
#define DBS_TYPE_UINT16			9
#define DBS_TYPE_UINT32			10


/* database server seek comparisons */

#define DBS_COMP_EQUAL          0 
#define DBS_COMP_LESS           1
#define DBS_COMP_LESSEQ         2
#define DBS_COMP_GREAT          3
#define DBS_COMP_GREATEQ        4
#define DBS_COMP_NOTEQ          5

/* database server general limits */

#define DBS_NAMESIZE		32
#define DBS_MAXKEYS			8
#define DBS_MAXFIELDS       127

/* database server error numbers */

#define E_DBS_BASE          -1000
#define E_DBS_INVALID       E_DBS_BASE      /* The record is invalid */
#define E_DBS_OPEN          E_DBS_BASE-1    /* The table or index is already open */
#define E_DBS_INUPDATE      E_DBS_BASE-2    /* The record is already being edited */
#define E_DBS_NOUPDATE      E_DBS_BASE-3    /* The record in not in edit mode */
#define E_DBS_NOINDEX       E_DBS_BASE-4    /* The index is not open */
#define E_DBS_TYPE          E_DBS_BASE-5    /* The type is invalid */
#define E_DBS_CONVERT       E_DBS_BASE-6    /* The conversion is not supported */
#define E_DBS_FIELD         E_DBS_BASE-7    /* The field is too large */
#define E_DBS_NAME          E_DBS_BASE-8    /* The field name is not found */
#define E_DBS_LOCKED        E_DBS_BASE-9    /* The field or record is locked */
#define E_DBS_NULL          E_DBS_BASE-10   /* A NOTNULL field is null */
#define E_DBS_KEY           E_DBS_BASE-11   /* The key is too large */
#define E_DBS_DUP           E_DBS_BASE-12   /* A duplictae key has been found */
#define E_DBS_BUFFER        E_DBS_BASE-13   /* field outside record buffer */
#define E_DBS_NOMATCH       E_DBS_BASE-14   /* no matching record in index */
#define E_DBS_LBIN          E_DBS_BASE-15   /* Field not LongBinary */
#define E_DBS_FIELDNAME     E_DBS_BASE-16   /* Invalid field name */
#define E_DBS_SYNTAXERR     E_DBS_BASE-20   /* Syntax error in expression */
#define E_DBS_BRACKET       E_DBS_BASE-21   /* Missing bracket in expression */
#define E_DBS_NOFIELD       E_DBS_BASE-22   /* No field name/number found */
#define E_DBS_MISMATCH      E_DBS_BASE-23   /* Type mismatch in expression */
#define E_DBS_BADHANDLE     E_DBS_BASE-24   /* Bad object handle */
#define E_DBS_NOISAM        E_DBS_BASE-25   /* Isam format not found */
#define E_DBS_CONNECTED     E_DBS_BASE-26   /* Already connected */
#define E_DBS_CANTLOCK      E_DBS_BASE-27   /* Can't lock dbase,table,index */
#define E_DBS_BMARK         E_DBS_BASE-28   /* Invalid bookmark */
#define E_DBS_NUMBER        E_DBS_BASE-29   /* Field number out of range */
#define E_DBS_NOTCLOSED     E_DBS_BASE-30   /* Still stuff open on it */
#define E_DBS_FARPTR        E_DBS_BASE-31   /* Bad far pointer */

#define E_DBS_BADMESSAGE    E_DBS_BASE-32   /* Unrecognised server message no */
#define E_DBS_MAJORERROR    E_DBS_BASE-33   /* You're in trouble !!! */
#define E_DBS_TOP           E_DBS_BASE-33

/* database server handles and structures */

typedef VOID* DBS_CLIENT;
typedef VOID* DBS_DBASE;
typedef VOID* DBS_TABLE;
typedef VOID* DBS_VIEW;

typedef struct
    {
    UBYTE data[8];
    } DBS_BOOKMARK;

typedef INT DBS_INDEX;

typedef struct DbsDaySec_tag 
	{
	LONG days;	
	ULONG secs;	
	} DBS_DAYSEC;

typedef struct DbsFieldDef_tag  
	{
	UBYTE type;	
	UBYTE size;	
	UBYTE attributes;	
	TEXT name[DBS_NAMESIZE+1];	
	} DBS_FIELDDEF;

typedef struct DbsTableDef_tag  
	{
	UWORD count;	
	DBS_FIELDDEF *fields[1];	
	} DBS_TABLEDEF;

typedef struct DbsFieldInfo_tag 
	{
	UWORD field_number;	
	UBYTE type;	
	UBYTE size;	
	UBYTE attributes;	
	TEXT name[DBS_NAMESIZE + 1];	
	} DBS_FIELDINFO;

typedef struct DbsKeyElem_tag   
	{
    WORD field_number;
    BYTE dummy;                 
	UBYTE flags;	
	WORD len;	
	} DBS_KEYELEM;

typedef struct DbsKeyDef_tag    
	{
	WORD count;	
	DBS_KEYELEM keys[DBS_MAXKEYS];	
	} DBS_KEYDEF;

typedef struct DbsSeekDef_tag
	{
	WORD keycount;	
	VOID *seekdata[1];	
	} DBS_SEEKDEF;

typedef struct DbsFPtr_tag
    {
    HANDLE segment;
    ULONG offset;
    } DBS_FPTR;

typedef struct DbsDebug_tag
    {
    INT usedcells;
    UINT usedmem;
    INT freecells;
    UINT freemem;
    INT dbases;
    INT tables;
    INT clients;
    INT opendbases;
    INT opentables;
    INT openviews;
    } DBS_DEBUGINFO;

typedef struct DbsDateInfo_tag
    {
    UWORD info;
    DBS_DAYSEC createDate;
    DBS_DAYSEC updateDate;
    } DBS_DATEINFO;



#pragma save, call(reg_param=>(bx,cx,dx,si,di),reg_saved=>(cx,dx,si,di,EPOC_SAVE))
GLREF_C INT DbsConnect(INT);
GLREF_C INT DbsDisconnect(VOID);

GLREF_C INT DbsDatabaseOpen(DBS_DBASE*,TEXT*,TEXT*,INT);
GLREF_C INT DbsDatabaseClose(DBS_DBASE);
GLREF_C INT DbsDatabaseGetTableCount(DBS_DBASE);
GLREF_C INT DbsDatabaseGetTableName(DBS_DBASE,INT,TEXT*);

GLREF_C INT DbsTableCreate(DBS_DBASE,DBS_TABLE*,TEXT*,INT,DBS_TABLEDEF*);
GLREF_C INT DbsTableOpen(DBS_DBASE,DBS_TABLE*,TEXT*,INT);
GLREF_C INT DbsTableClose(DBS_TABLE);
GLREF_C INT DbsTableDelete(DBS_TABLE);
GLREF_C INT DbsTableCompress(DBS_DBASE,TEXT*);
GLREF_C INT DbsTableGetFieldCount(DBS_TABLE);
GLREF_C INT DbsTableGetFieldDef(DBS_TABLE,INT,DBS_FIELDINFO*);
GLREF_C INT DbsTableGetFieldDefByName(DBS_TABLE,TEXT*,DBS_FIELDINFO*);
GLREF_C INT DbsTableGetIndexCount(DBS_TABLE);
GLREF_C INT DbsTableGetIndexName(DBS_TABLE,INT,TEXT*);
GLREF_C INT DbsTableGetDates(DBS_TABLE,DBS_DATEINFO*);

GLREF_C INT DbsIndexCreate(DBS_TABLE,DBS_INDEX*,TEXT*,INT,VOID*);
GLREF_C INT DbsIndexOpen(DBS_TABLE,DBS_INDEX*,TEXT*,INT);
GLREF_C INT DbsIndexClose(DBS_TABLE,DBS_INDEX);
GLREF_C INT DbsIndexDelete(DBS_TABLE,DBS_INDEX);
GLREF_C INT DbsIndexRebuild(DBS_TABLE,DBS_INDEX);
GLREF_C INT DbsIndexGetDef(DBS_TABLE,DBS_INDEX,DBS_KEYDEF*,INT*);
GLREF_C INT DbsIndexGetDates(DBS_TABLE,DBS_INDEX,DBS_DATEINFO*);

GLREF_C INT DbsTblViewCreate(DBS_TABLE,DBS_VIEW*,INT);
GLREF_C INT DbsTblViewSetIndex(DBS_VIEW,DBS_INDEX);
GLREF_C INT DbsTblViewSeek(DBS_VIEW,INT,DBS_SEEKDEF*);
GLREF_C INT DbsTblViewAddIndex(DBS_VIEW);
GLREF_C INT DbsTblViewRemoveIndex(DBS_VIEW);

GLREF_C INT DbsDynaViewCreateOnTable(DBS_TABLE,DBS_VIEW*,TEXT*,VOID*,INT);
GLREF_C INT DbsDynaViewCreateOnDynaView(DBS_VIEW,DBS_VIEW*,TEXT*,VOID*,INT);

GLREF_C INT DbsViewClose(DBS_VIEW);
GLREF_C INT DbsViewClone(DBS_VIEW,DBS_VIEW*);
GLREF_C INT DbsViewGetRecordCount(DBS_VIEW,LONG*);
GLREF_C INT DbsViewFindFirst(DBS_VIEW,TEXT*);
GLREF_C INT DbsViewFindLast(DBS_VIEW,TEXT*);
GLREF_C INT DbsViewFindNext(DBS_VIEW,TEXT*);
GLREF_C INT DbsViewFindPrevious(DBS_VIEW,TEXT*);
GLREF_C INT DbsViewMoveFirst(DBS_VIEW);
GLREF_C INT DbsViewMoveLast(DBS_VIEW);
GLREF_C INT DbsViewMoveNext(DBS_VIEW);
GLREF_C INT DbsViewMovePrevious(DBS_VIEW);
GLREF_C INT DbsViewGetBookmark(DBS_VIEW,DBS_BOOKMARK*);
GLREF_C INT DbsViewGetLastModified(DBS_VIEW,DBS_BOOKMARK*);
GLREF_C INT DbsViewMoveBookmark(DBS_VIEW,DBS_BOOKMARK*);
GLREF_C INT DbsViewGetFieldSize(DBS_VIEW,INT,LONG*);
GLREF_C INT DbsViewGetFieldSizeByName(DBS_VIEW,TEXT*,LONG*);
GLREF_C INT DbsViewGetField(DBS_VIEW,INT,VOID*,INT);
GLREF_C INT DbsViewGetFieldByName(DBS_VIEW,TEXT*,VOID*,INT);
GLREF_C INT DbsViewSetField(DBS_VIEW,INT,VOID*,UINT);
GLREF_C INT DbsViewSetFieldByName(DBS_VIEW,TEXT*,VOID*,UINT);
GLREF_C INT DbsViewLBinGetChunk(DBS_VIEW,INT,UBYTE*,INT,LONG*);
GLREF_C INT DbsViewLBinGetChunkByName(DBS_VIEW,TEXT*,UBYTE*,INT,LONG*);
GLREF_C INT DbsViewLBinGetFChunk(DBS_VIEW,INT,DBS_FPTR*,UINT*,LONG*);
GLREF_C INT DbsViewLBinGetFChunkByName(DBS_VIEW,TEXT*,DBS_FPTR*,UINT*,LONG*);
GLREF_C INT DbsViewLBinAppendChunk(DBS_VIEW,INT,UBYTE*,UINT);
GLREF_C INT DbsViewLBinAppendChunkByName(DBS_VIEW,TEXT*,UBYTE*,UINT);
GLREF_C INT DbsViewLBinAppendFChunk(DBS_VIEW,INT,DBS_FPTR*,UINT);
GLREF_C INT DbsViewLBinAppendFChunkByName(DBS_VIEW,TEXT*,DBS_FPTR*,UINT);
GLREF_C INT DbsViewLBinClear(DBS_VIEW,INT);
GLREF_C INT DbsViewLBinClearByName(DBS_VIEW,TEXT*);
GLREF_C INT DbsViewAppendRecord(DBS_VIEW);
GLREF_C INT DbsViewEditRecord(DBS_VIEW);
GLREF_C INT DbsViewUpdateRecord(DBS_VIEW);
GLREF_C INT DbsViewCancelUpdate(DBS_VIEW);
GLREF_C INT DbsViewDeleteRecord(DBS_VIEW);

GLREF_C INT DbsDebugInfo(DBS_DEBUGINFO*);
GLREF_C INT DbsErrorString(INT,TEXT*);
GLREF_C INT DbsVersion(INT*,TEXT*);
GLREF_C INT DbsSetTempDir(TEXT*);

#pragma restore

#endif
