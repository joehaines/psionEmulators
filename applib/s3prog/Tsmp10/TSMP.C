/*	Written By Jason and DavidW, March 1994
	Demo SYS$SAMP.LDD the sound sampling ldd
*/

#include <p_std.h>
#include <p_file.h>
#include <epoc.h>
#include <wlib.h>

#include "Sampler.h"

GLREF_D VOID *DatCommandPtr;

LOCAL_D WORD Buf[240];
LOCAL_D VOID *pcb;

LOCAL_C VOID Panic(INT err,TEXT *msg)
    {
    p_notifyerr(err,msg,0,0,0);
    p_exit(0);
    }

LOCAL_C VOID DeleteDriver(VOID)
    {   /* ignore error since could be in use by other program */
    p_devdel("SMP",E_LDD);
    }

LOCAL_C VOID CheckContinueOrNot(VOID)
    {
    if (!p_notify("SMP: deleted","Start sampling?","Exit","Continue",0))
        p_exit(0);
    }

LOCAL_C VOID LoadDriver(VOID)
    {
    INT ret;
    TEXT buf[P_FNAMESIZE];

    p_fparse("\\sys$samp.LDD",DatCommandPtr,&buf[0],NULL);
    ret=p_loadldd(&buf[0]);
    if (ret && ret!=E_FILE_EXIST)
        Panic(ret,"Failed to load SYS$SAMP.LDD");
    }

LOCAL_C VOID OpenChannel(VOID)
    {
    INT ret;

    ret=p_open(&pcb,"SMP:",-1);
    if (ret)
        Panic(ret,"Failed to open channel to SMP");
    }

LOCAL_C VOID DisplayInputSound(VOID)
    {
    INT x,yold,y;
    WORD *pb;
    P_RECT box;

    box.tl.x=0;
    box.tl.y=0;
    box.br.x=240;
    box.br.y=80;
    FOREVER
        {
        x=(-1);
        yold=40;
        pb=(&Buf[0]);
        p_read(pcb,pb,240);
        gClrRect(&box,G_TRMODE_CLR);
        while(x++<240)
            {
            y=40+(*pb++)/128;
            gDrawLine(x,yold,x+1,y);
            yold=y;
            }
        wFlush();
        }
    }

GLDEF_C VOID main(VOID)
    {
    DeleteDriver();
    CheckContinueOrNot();
    LoadDriver();
    OpenChannel();
    wStartup();
    DisplayInputSound();
    p_close(pcb);
    }

