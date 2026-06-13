/* Take a look at win.c for full details about
   using #link and #run etc..
 */ 

#link "-om:\img\hello.img hello.o stdio.o"
#run "m:\img\hello.img"


#include <stdio.h>

main()
{
	printf("Hello world\n\r");
 	fgetc(stdin);
	exit(0);
}

