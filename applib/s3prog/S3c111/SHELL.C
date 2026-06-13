/* S3 Users comment in the next line */
/* #define s3 1*/


#linkpath "a:\c\"
#link "-i0 -om:\img\shell.img shell.o stdio.o"
#run "m:\img\shell.img"


#include "stdio.h"

#define WS_FONT_BASE 0x4000

main()
{

#ifdef s3
	openshell(WS_FONT_BASE+11,TRUE,FALSE,15,3);
#else
	openshell(WS_FONT_BASE+11,FALSE,FALSE,30,5);
#endif

	printf("hello world\n\r");
 	fgetc(stdin);
	exit(0);
}

