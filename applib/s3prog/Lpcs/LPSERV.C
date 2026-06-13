/*	MODULE - LPSERV.C
Written by Colly, October 1992
*/
#include <p_std.h>
#include <p_gen.h>
#include <p_sys.h>
#include <p_file.h>
#include <p_cons.h>
#include <epoc.h>
#include "lp.h"

#define MSG_Q_SIZE 4

typedef struct
    {
    E_MESSAGE m; /* Defined in EPOC.H */
    UWORD arg1; /* First argument */
    UWORD arg2; /* Second argument */
    } MESS;

GLREF_D VOID *winHandle; /* The open consol channel handle */

LOCAL_C VOID panic(TEXT *mess,INT err)
/*
Output error message and terminate program.
*/
    {
    TEXT buf[256];

    if (err)
	{
	p_errs(&buf[0],err);
	p_printf("%s failed - %s",mess,&buf[0]);
	}
    else
	p_printf("%s",mess);
    p_getch();
    p_exit(TRUE);
    }

GLDEF_C VOID main(VOID)
/*
Act as a link paste server.
*/
    {
    INT ret,iState;
    HANDLE wsrvPid,clientPid;
    WORD kStat,mStat;
    ULONG fmt;
    MESS *pM;
    P_CON_KBREC k;
    TEXT buf[0x40];
/*
As we are going to be a server we will be sent messages, so that we need
to initialise message reception.
*/
    if ((ret=p_minit(MSG_Q_SIZE,sizeof(MESS)-sizeof(E_MESSAGE)))<0)
	panic("Initialise messaging",ret);
/*
Print a banner message and get the console open.
*/
    p_printf("Link Paste Server V1.00F");
    p_printf("Press ESC to exit");
/*
Next we need the pid of the window server as we will send it
a message when we go to background to register ourselves as
the link paste server. No point checking the return value as
if the window server has disappeared there is nobody to print
an error message for us anyway!
*/
    wsrvPid=p_pidfind("SYS$WSRV.*");
/*
Read from the console asynchronously, include events
so that we can detect when we go to background.
*/
    p_ioc4(winHandle,P_EVENT_READ,&kStat,&k);
/*
Read a message asynchronously.
*/
    p_mreceive(&mStat,&pM);
/*
Set clientPid to zero to indicate that we are inactive.
*/
    clientPid=0;
/*
Now enter the main event processing loop.
*/
    FOREVER
	{
	p_iowait(); /* Wait for something to happen */
	if (mStat!=E_FILE_PENDING) /* We have received a message */
	    {
	    if (mStat!=0) /* Just some extra error checking */
		panic("Message receive",ret);
	    switch (pM->m.type) /* Some kind of message has arrived */
		{
	    case TY_LINKSV_STEP: /* Someone wants some data */
		if (clientPid==0) /* First request, so setup the info */
		    {
		    ret=E_GEN_NSUP;
            switch (pM->arg1)
                {
            case DF_LINK_TEXT_VAL:
            case DF_LINK_TABTEXT_VAL:
            case DF_LINK_PARAS_VAL:
			    clientPid=pM->m.pid; /* Register our client */
			    p_logon(clientPid,TY_LINKSV_DEATH); /* Request a death message from EPOC */
			    iState=0; /* Set the server state */
			    ret=0; /* We can support the requested type */
                }
		    break; /* Reply with the result in ret */
		    }
		if (!pM->arg1) /* If the client requests no data, give up */
		    iState=1; /* Jump to the end of data state */
		switch (iState) /* Handle the various states */
		    {
	    case 0:
		    p_getdat(&buf[0]); /* Get the current date and time as text */
/*
Copy the data over to the client, after checking that we don't have more
than the requested length. We all return the length copied as the result.
*/
		    ret=p_slen(&buf[0]); /* The length of the data */
		    if (ret>pM->arg2) /* pM->arg2 has the maximum length */
			ret=pM->arg2;
/*
No need to do any conversion here as the data is the same for all renderings.
*/
		    p_pcpyto(clientPid,(VOID *)pM->arg1,&buf[0],ret); /* Copy over the data */
		    iState++; /* Advance to the next state */
		    break;
	    default:
		    ret=E_FILE_EOF; /* Signifies no more data */
		    p_logoff(clientPid,TY_LINKSV_DEATH); /* No longer interested if the client dies */
		    clientPid=0; /* We are no longer active */
		    }
		break;
	    case TY_LINKSV_DEATH: /* Out client has died */
		clientPid=0; /* Mark ourselves as inactive again */
		break; /* Supervisor does not require a return result */
	    default: /* Some one has sent us a naughty message */
		ret=E_GEN_NSUP; /* Just return a not supported error */
		}
	    p_mfree(pM,ret); /* Return the message to the queue and return a result */
/*
Get another message asynchronously.
*/
	    p_mreceive(&mStat,&pM);
	    }
	else if (kStat!=E_FILE_PENDING) /* We have received a console event */
	    {
	    if (kStat!=0) /* Just some extra error checking */
		panic("Read console",ret);
	    if (k.keycode==CONS_EVENT_BACKGROUND) /* We are going to background */
		{
		fmt=DF_LINK_TEXT|DF_LINK_TABTEXT|DF_LINK_PARAS; /* We are prepared to render in any format */
		ret=p_msendreceivew(wsrvPid,SY_LINK_SERVER,&fmt); /* Inform the window server */
		if (ret<0)
		    panic("Wsrv link server request",ret);
		}
	    else if (k.keycode==0x1b) /* User requested that we exit */
		p_exit(TRUE);
/*
We've used the event so we must queue another read.
*/
	    p_ioc4(winHandle,P_EVENT_READ,&kStat,&k);
	    }
	else
	    panic("Stray signal death",0);
	}
    }

