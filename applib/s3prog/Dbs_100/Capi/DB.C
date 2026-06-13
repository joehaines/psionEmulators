/*
Sample C Database Application

Very Simple !!!

Copyright (c) Psion PLC 1995
*/

#include <p_std.h>
#include <p_sys.h>
#include <p_math.h>
#include "dbs.h"

LOCAL_C VOID giveError(TEXT *error)
    {
    p_printf("\n%s",error);
    p_getch();
    p_exit(0);
    }


LOCAL_C VOID giveError2(INT errornum)
    {
    TEXT errorstr[64];
    DbsErrorString(errornum,&errorstr[0]);
    p_printf("\nError %d has occured : %s",errornum,&errorstr[0]);
    p_getch();
    p_exit(0);
    }


LOCAL_C VOID showField(DBS_VIEW hView,INT field,INT type)
    {
    INT logic;
    WORD int16;
    LONG int32;
    DOUBLE dbl;
    P_DTOB format;
    TEXT string[256];
    DBS_DAYSEC date;

    switch (type)
        {
        case DBS_TYPE_LOGICAL:
            DbsViewGetField(hView,field,&logic,0);
            if (logic)
                p_printf("Field %d : TRUE",field);
            else
                p_printf("Field %d : FALSE",field);
            break;
        case DBS_TYPE_INT16:
            DbsViewGetField(hView,field,&int16,0);
            p_printf("Field %d : %d",field,int16);
            break;
        case DBS_TYPE_INT32:
            DbsViewGetField(hView,field,&int32,0);
            p_printf("Field %d : %ld",field,int32);
            break;
        case DBS_TYPE_DOUBLE:
            DbsViewGetField(hView,field,&dbl,0);
            format.type = P_DTOB_GENERAL;
            format.width = 255;
            format.point = '.';
            format.trilen = 0;
            p_dtob(&string[0],&dbl,&format);
            p_printf("Field %d : %s",field,&string[0]);
            break;
        case DBS_TYPE_STRING:
            string[DbsViewGetField(hView,field,&string[0],255)] = '\0';
            p_printf("Field %d : %s",field,&string[0]);
            break;
        case DBS_TYPE_DATE:
            DbsViewGetField(hView,field,&date,0);
            p_printf("Field %d : Days %ld  Secs %ld",field,date.days,date.secs);
            break;
        case DBS_TYPE_LONGBINARY:
            p_printf("Field %d : Long binary field",field);
            break;
        default:
            p_printf("Field %d : Unrecognised field type",field);
            break;
        }
    }

GLDEF_C INT main(VOID)
    {
    DBS_DBASE hDBase;
    DBS_TABLE hTable;
    DBS_VIEW hView;
    DBS_FIELDINFO info;
    INT tableCount,fieldCount,i,res;
    TEXT tableName[DBS_NAMESIZE+1];

    p_printf("Very simple .DBF file info program !");
    p_printf("====================================\n");

/* Connect to server */
    if (DbsConnect(0) < 0)
        giveError("Connection failed !");
    DbsVersion(&i,&tableName[0]);
    p_printf("Server Version %s\n",&tableName[0]);

/* Open database */
    if (DbsDatabaseOpen(&hDBase,"Psion","\\dat\\",DBS_READONLY) < 0)
        giveError("Couldn't open \\DAT\\ directory...");

/* List table files */
    tableCount = DbsDatabaseGetTableCount(hDBase);
    if (tableCount == 0)
        giveError("No .DBF files in the \\DAT\\ directory...");
    for (i=0;i<tableCount;i++)
        {
        DbsDatabaseGetTableName(hDBase,i,&tableName[0]);
        p_printf("Table %d : %s",i,&tableName[0]);
        }

/* Open table */
    p_getl("\nEnter table to open : ",&tableName[0],DBS_NAMESIZE);
    res = DbsTableOpen(hDBase,&hTable,&tableName[0],0);
    if (res < 0)
        giveError2(res);

/* List fields */
    p_printf("Table opened...Fields are : ");
    fieldCount = DbsTableGetFieldCount(hTable);
    for (i=0;i<fieldCount;i++)
        {
        if ((i&0x7)==7)
            {
            p_print("Press a key...");
            p_getch();
            p_print("\r              \r");
            }
        DbsTableGetFieldDef(hTable,i,&info);
        p_printf("Field %d : %s",i,&info.name[0]);
        }
    p_printf("Press a key...");
    p_getch();

/* Show first record */
    DbsTblViewCreate(hTable,&hView,DBS_READONLY);
    if (DbsViewMoveFirst(hView) < 0)
        giveError("No records in table");
    p_printf("\nFirst record is :");
    for (i=0;i<fieldCount;i++)
        {
        if ((i&0x7)==7)
            {
            p_print("Press a key...");
            p_getch();
            p_print("\r              \r");
            }
        DbsTableGetFieldDef(hTable,i,&info);
        showField(hView,i,info.type);
        }
    p_printf("Press a key...");
    p_getch();
    
/* Disconnect from server */
    DbsViewClose(hView);
    DbsTableClose(hTable);
    DbsDatabaseClose(hDBase);
    DbsDisconnect();

    return 0;
    }
