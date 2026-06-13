/*
by Jason August 1994
BEAMER by Jason 1993
*/
#include <p_std.h>
#include <p_sys.h>
#include <p_file.h>
#include <p_keyb.h>
#include <p_graf.h>
#include <p_screen.h>
#include <p_serial.h>
#include <epoc.h>

GLREF_C VOID *DatCommandPtr;

#define Version "V1.04F (16/08/94)"
#define XBLEN 0x100

typedef struct
    {
    E_MESSAGE m;
    UBYTE code;
    UBYTE filler;
    } Message;

GLREF_D VOID *winHandle;

LOCAL_D P_CON_KBREC kB;
LOCAL_D VOID *sch;
LOCAL_D WORD tsb;
LOCAL_D INT xbufl;
LOCAL_D INT echo;
LOCAL_D INT sWidth;
LOCAL_D TEXT xbuf[XBLEN];
LOCAL_D INT txbufl;
LOCAL_D TEXT txbuf[XBLEN];
LOCAL_D TEXT fbuf[] =
"0Brick quiz whangs jumpy veldt fox. The quick brown fox jumps over the lazy dog.";


LOCAL_C VOID DeletePDD(VOID)
    {
    p_devdel("TTY.BEM",E_PDD);
    }

LOCAL_C VOID exit(VOID)
    {
    p_close(sch);
    DeletePDD();
    p_exit(0);
    }

LOCAL_C VOID panic(
/*
Output error message and terminate program.
*/
    TEXT *Mess, /* Message to be output */
    INT Err) /* Plib error number */
    {
    TEXT buf[256];

    if (Err)
        {
        p_errs(&buf[0],Err);
        p_printf("*** PANIC *** %s failed - %s",Mess,&buf[0]);
        }
    else
        p_printf("*** PANIC *** %s",Mess);
    p_getch();
    exit();
    }

LOCAL_C VOID trans(
/*
Send anything if there is something to be sent.
*/
    TEXT *buf,
    INT len)
    {
    INT res;

    if (xbufl+len>XBLEN)
        p_print("\177");
    else
        {
        p_bcpy(xbuf+xbufl,buf,len);
        xbufl+=len;
        }
    if (!txbufl)
        {
        if (!len && !xbufl)
            tsb=E_FILE_PENDING;
        else
            {
            p_bcpy(&txbuf[0],&xbuf[0],txbufl=xbufl);
            if ((res=p_ioa5(sch,P_FWRITE,&tsb,&txbuf[0],&txbufl))<0)
                panic("Queue transmit",res);
            xbufl=0;   
            }
        }
    }

LOCAL_C VOID LoadPDD(VOID)
    {
    TEXT buf[P_FNAMESIZE];
    INT ret;

    p_fparse("BEAMV1.PDD",DatCommandPtr,&buf[0],NULL);
    ret=p_loadpdd(&buf[0]);
    if (ret && ret!=E_FILE_EXIST)
        panic("Loading BEAMV1.PDD",ret);
    }

GLDEF_C VOID main(VOID)
/*
Main test program.
*/
    {
    INT res,rqlen,lcount;
    WORD msb,ssb;
    TEXT buf[0x40],chr[2];
    P_RECTP pR;

    DeletePDD();
    p_printf("Beamer Test %s (Jason) (C) Copyright Psion PLC 1994\r\n",Version);
    p_printf("Press any key to continue, or Psion-Esc to exit");
    p_getch();
    LoadPDD();
    p_minit(2,sizeof(Message)-sizeof(E_MESSAGE));
    if ((res=p_iow3(winHandle,P_FSENSE,&pR))<0)
         panic("Sensing the screen",res);
    sWidth=pR.r.br.x;
    if ((res=p_open(&sch,"TTY.BEM:X",-1))<0)
        panic("Opening BEAMER",res);
    p_ioc4(winHandle,P_FREAD,&msb,&kB);
    rqlen=1;
    p_ioc5(sch,P_FREAD,&ssb,&chr[0],&rqlen);
    lcount=0;
    echo=0;
    tsb=E_FILE_PENDING;
    xbufl=txbufl=0;
    p_printf("Setup O.K.");
    FOREVER
        {
        p_iowait(); /* Wait for something */
        if (msb!=E_FILE_PENDING) /* Was it the keyboard */
            {
            if (msb<0)
                panic("Reading the keyboard",msb);
            if (kB.keycode==0x1b)
                exit();
            else if (kB.keycode==6)
                {
                trans(&fbuf[0],p_slen(&fbuf[0]));
                fbuf[0]++;
                if (fbuf[0]>'9')
                    fbuf[0]='0';
                goto requekey;
                }
            trans((TEXT *)&kB.keycode,1);
    requekey:
            p_ioc4(winHandle,P_FREAD,&msb,&kB); /* Que a message key read */
            }
        else if (ssb!=E_FILE_PENDING) /* Serial read ok */
            {
            if (ssb==E_FILE_CANCEL)
                goto reQue;
            if (ssb<0)
                {
                p_errs(&buf[0],ssb);
                p_print("\r\nSerial read failed - %s\r\n",&buf[0]);
                goto reQue;
                }
            chr[1]=0;
            if (chr[0]==9) /* decode tabs */
                {
                lcount+=8;
                p_print("        ");
                }
            else
                p_print("%s",&chr[0]);
            if (p_isprint(chr[0]))      
                lcount++;
            if (chr[0]==13)
                lcount=0;
            if (lcount>sWidth-1)
                {
#ifdef WIDTHCONTROL
                p_print("\n\r");
#endif
                lcount=0;
                }
            if (echo)
                trans(&chr[0],1);
        reQue:
            rqlen=1;
            if ((res=p_ioa5(sch,P_FREAD,&ssb,&chr[0],&rqlen))<0)
                panic("Que serial read",res);
            }
        else if (tsb!=E_FILE_PENDING)
            {
            if (tsb<0)
                {
                if (tsb!=E_FILE_CANCEL)
                    {
                    p_errs(&buf[0],tsb);
                    p_print("\r\nSerial write failed - %s\n",&buf[0]);
                    }
                }
            txbufl=0;
            trans(0,0);
            }
        else
            panic("Unknown signal",0);
        }
    }
