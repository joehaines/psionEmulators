#include <plib.h>
#include <dbs.h>

LOCAL_D TEXT ourVersion[]="1.00"; 

LOCAL_C INT checkVersion(TEXT *newStr)
/*
Check whether our version is newer than any installed DBS version
Returns TRUE if the same or a newer version is already installed, FALSE
otherwise
*/
    {
    TEXT versionStr[8];
    TEXT *pStr;
    INT dummy;
    DOUBLE newVer,oldVer;

    if (DbsConnect(0)!=0)
        return FALSE;
    DbsVersion(&dummy,versionStr);
    DbsDisconnect();
    pStr=&versionStr[0];
    p_stod(&pStr,&oldVer,'.');
    p_stod(&newStr,&newVer,'.');
    return (p_fcmp(&oldVer,&newVer)>=0);
    }


GLDEF_C INT main(VOID)
    {
    if (checkVersion(ourVersion))
        p_printf("No need to install the new version...");
    else
        p_printf("Need to install the new version...");
    p_getch();
    return 0;
    }
