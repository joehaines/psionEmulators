/* S3 Users comment in the next line */
/* #define s3 1 */

/* Tell the compiler to invoke the linker when finished */
#link "-i0 -om:\img\win.img e_gen.o e_win.o e_io.o win.o stdio.o"


/*
 Link usage:
 
#link  "[-eEXT][-sN][-iN] -oOUTFILE INFILE(S)..."

where:
 -eEXT : Set default INFILE extensions to EXT
   -sN : Set Image stack to N paragraphs (default=160)
   -iN : Use startup module N: 
          0 = Nothing with no command line
          1 = Shell with no command line
          2 = Nothing with C type command line
          3 = Shell with C type command line (default)
          4 = Nothing with LBC type command line
          5 = Shell with LBC type command line

Use #linkpath "pathname" to set the default directory
for the linker.

*/

/* Tell the compiler to run the finished image file when 
   everything has compiled and linked */

#run "m:\img\win.img"

/*
Use #runpath "pathname" to set the default initial directory 
for the image file.
*/


#include <stdio.h>



/* These defines should be in their appropriate include
   files. We've kidnapped them to make this demo easier to
   compile.
*/ 

#define WS_FONT_BASE 0x4000

#define W_BORD_CUSHION 0x01
#define W_BORD_SHADOW_D 0x08
#define W_BORD_SHADOW_ON 0x10

#define WM_KEY 1
#define P_FINQ 12

/* There are currently no struct types in the compiler,
   for the time being we'll use defines.
*/

typedef struct
{
 unsigned int window_handle;
 unsigned int font_handle;
 unsigned int line_height;
 unsigned int char_width;
} CONSOLE_INFO;

typedef struct /* The WS_EV struct runs in parallel to the WS_EVENT struct */ 
 { /* but is in a simplified form without the WS_EVENT_X */ 
 /* sub structure */
 short  type;
 unsigned short handle;
 unsigned short time;
 int p[100];
 } WS_EV;

typedef struct
    {
    short x; /* Horizontal coordinate */ 
    short y; /* Vertical coordinate */ 
    } P_POINT;

typedef struct
    {
    P_POINT tl; /* Top left point */ 
    P_POINT br; /* Bottom right point */ 
    } P_RECT;

/* Notify the user that an error has occured */

error(e,s)
int e;
char *s;
{
	GenNotifyError (e,s,0,0,0);
	exit(0);
}


main(argc,argv)
int argc,*argv;
{
	CONSOLE_INFO	cinfo;
	WS_EV	event;

#ifdef s3
/* Open an s3 shell with no cursor, 20x5 chars big */
	openshell(WS_FONT_BASE+4,TRUE,FALSE,20,5);
#else
/* Open an s3a shell with no cursor, 50x17 chars big */
	openshell(WS_FONT_BASE+4,FALSE,FALSE,50,17);
#endif

/* Get the shell's window handle */
	IoWithWait(P_FINQ,stdout,cinfo,0);

/* Create a graphics context and print to it*/
	gCreateGC(cinfo.window_handle,0,0);
	gPrintText(10,20,"Hello world",11);

/* Make a simple border around our window */
	gBorder(W_BORD_SHADOW_ON|W_BORD_SHADOW_D|W_BORD_CUSHION);

/* Wait for a keypress */
	wGetEvent(event);
	do{
			IoWaitForSignal();
	}while (event.type!=WM_KEY);


/* ALWAYS remember to use exit() to end the program */
	exit(0);
}

