/*	MODULE - LPCLNT.C
Written by Colly, October 1992
*/
#include <p_std.h>
#include <p_gen.h>
#include <p_sys.h>
#include <p_file.h>
#include <epoc.h>
#include "lp.h"

#define MAX_CH 8

GLREF_D VOID *winHandle; /* The open consol channel handle */

LOCAL_D HANDLE srvPid; /* The server we are talking to. */
LOCAL_D WORD sStat; /* The I/O completion word for death notification */
LOCAL_D TEXT buf[0x100]; /* The data receive buffer */
LOCAL_D UINT addr; /* The address to be displayed */
LOCAL_D INT lCnt; /* The current line count */

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

LOCAL_C INT disp(INT len)
/*
Display len bytes of data.
*/
    {
    INT c,d;
    TEXT dis[MAX_CH+2],*p;

    
    p=(&buf[0]);
    while (len)
	{
	lCnt++;
	p_print("%04x: ",addr);
	d=(len>MAX_CH ? MAX_CH : len);
	p_bcpy(&dis[0],p,d);
	for (c=0;c<d;c++)
	    {
	    p_print("%02x ",dis[c]);
	    if (!p_isprint(dis[c]))
		dis[c]='.';
	    }
	for (;c<MAX_CH;c++)
	    {
	    p_print("   ");
	    dis[c]=' ';
	    }
	dis[8]=0;
	p_printf(" %s",&dis[0]);
	if (lCnt==8)
	    {
	    lCnt=0;
	    p_print("Press Esc or ANY other key");
	    c=p_getch();
	    p_printf("");
	    if (c==0x1b)
		return(TRUE);
	    }
	addr+=d;
	p+=d;
	len-=d;
	}
    return(FALSE);
    }

LOCAL_C INT talk(VOID *par)
/*
Send the STEP message to the server and wait for DEATH or the
reply.
*/
    {
    INT ret,sigCount;
    WORD stat;

    p_msendreceivea(srvPid,TY_LINKSV_STEP,par,&stat); /* Send the message and get a reply */
    sigCount=0;
    FOREVER
	{
	p_iowait(); /* Wait for something to happen */
	if (sStat!=E_FILE_PENDING) /* The server has died */
	    {
	    p_mcancel(); /* Cancel the receive request */
	    p_waitstat(&stat); /* Wait for the signal */
	    ret=E_GEN_RECEIVER; /* Return the error */
	    break;
	    }
	else if (stat!=E_FILE_PENDING) /* The result has been returned */
	    {
	    ret=stat;
	    break;
	    }
	else
	    sigCount++; /* Count up all other signals */
	}
    while (sigCount--) /* We have got some spare signals */
	    p_iosignal(); /* Put them back */
    return(ret);
    }

GLDEF_C VOID main(VOID)
/*
Act as a link paste client.
*/
    {
    INT c,ret;
    HANDLE wsrvPid;
    ULONG fmt,*pfmt;
    TEXT *f[4];
	UWORD args[2];

/*
Print a banner message and get the console open.
*/
    p_printf("Link Paste Client V1.00F");
/*
We need the pid of the window server as we will send it
a message when we go to background to register ourselves as
the link paste server. No point checking the return value as
if the window server has disappeared there is nobody to print
an error message for us anyway!
*/
    wsrvPid=p_pidfind("SYS$WSRV.*");
/*
Enter the main loop.
*/
    FOREVER
	{
	p_print("\r\nPress (F)etch, (Q)uit < >\b\b");
	FOREVER
	    {
	    c=p_toupper(p_getch());
	    if (c=='Q')
		p_exit(FALSE);
	    else if (c=='F')
		break;
	    else
		p_print("\7");
	    }
	p_printf("F");
/*
First we have to interrogate the system for the link server pid
and the formats that the server can render the data. Note that
we send the address of a ULONG to receive the format type data.
*/
	pfmt=(&fmt);
	srvPid=p_msendreceivew(wsrvPid,SY_LINK_PASTE,&pfmt); /* Query the window server */
	if (srvPid<0) /* Some kind of error here */
	    {
	    p_errs(&buf[0],srvPid); /* Just display the error */
	    p_printf("Getting server info\r\n%s",&buf[0]);
	    continue;
	    }
    if (!srvPid)
	    {
	    p_printf("No data available for Link Paste");
	    continue;
	    }
/*
Determine what formats are available. We won't ever get native here
as we are never a link paste server.
*/
	f[0]="PLAIN TEXT      - unavailable";
	f[1]="TABBED TEXT     - unavailable";
	f[2]="PARAGRAPH TEXT  - unavailable";
	if (fmt&DF_LINK_TEXT)
	    f[0]="PLAIN TEXT      - P";
	if (fmt&DF_LINK_TABTEXT)
	    f[1]="TABBED TEXT     - T";
	if (fmt&DF_LINK_PARAS)
	    f[2]="PARAGRAPH TEXT  - R";
	f[3]="Quit            - Q";
/*
Display the name of the server and the formats available.
*/
	p_pname(srvPid,&buf[0]);
	p_printf("Server is %s",&buf[0]);
	for (ret=0;ret<4;ret++)
	    p_printf("%s",f[ret]);
	p_print("Press P,Q,R or T < >\b\b");
	FOREVER
	    {
	    c=p_toupper(p_getch());
	    switch (c)
		{
	    case 'P':
		if (fmt&DF_LINK_TEXT)
		    {
		    args[0]=DF_LINK_TEXT_VAL;
		    break;
		    }
		continue;
	    case 'T':
		if (fmt&DF_LINK_TABTEXT)
		    {
		    args[0]=DF_LINK_TABTEXT_VAL;
		    break;
		    }
		continue;
	    case 'R':
		if (fmt&DF_LINK_PARAS)
		    {
		    args[0]=DF_LINK_PARAS_VAL;
		    break;
		    }
		continue;
	    case 'Q':
		break;
	    default:
		p_print("\7");
		continue;
		}
	    p_printf("%c",c);
	    break;
	    }
	if (c=='Q') /* User does'nt want anything, so ask again */
	    continue;
/*
We are about to enter into conversation with the server. We
need to request an asynchronous notification if the server
dies at some point during the conversation.
*/
	p_logona(srvPid,&sStat); /* Request notification */
/*
Finally here we go getting the data. First communicate with the
server and ask it to render the data in the requested format.
Note that we use a special routine which sends the message to the
server asynchronously and waits for either the reply or the server
dying.
*/
	ret=talk(&args[0]); /* Request a rendering */
	if (ret==0) /* Server prepared to render data */
	    {
	    lCnt=0;
	    addr=0;
	    FOREVER
		{
        args[0]=(UWORD)(&buf[0]);
        args[1]=sizeof(buf);
		ret=talk(&args[0]); /* Get the server data */
		if (ret<0)
		    {
		    if (ret==E_FILE_EOF)
			{
			p_printf("---------- EOF ----------");
			ret=0; /* Avoid an error print */
			}
		    break;
		    }
		if (disp(ret)) /* Display the data received */
		    {
            args[0]=0;
		    talk(&args[0]); /* Inform the server that we are finished */
		    break;
		    }
		}
	    }
	if (sStat==E_FILE_PENDING) /* We are still logged onto the server */
	    p_logoffa(srvPid); /* Cancel the notification */
	if (ret<0)
	    {
	    p_errs(&buf[0],ret); /* Just display the error */
	    p_printf("Error talking to the server\r\n%s",&buf[0]);
	    }
	}
    }

