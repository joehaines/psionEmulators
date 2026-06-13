#include <stdio.h>
#include <stdlib.h>

int main(void)
{
   unsigned char line[1000];
   unsigned char *def[256];
   int i;
   gets(line);
   if (strcmp(line, "!!Compressed!!")) {
      printf("not a compressed file\n");
      exit(EXIT_FAILURE);
   }
   for (i=1; i<256; i++) {
      gets(line);
      if (*line == 0) {
	 line[0] = i;
	 line[1] = 0;
      }
      def[i] = strdup(line);
   }
   while (gets(line)) {
      unsigned char *s = line;
      while (*s) printf("%s", def[*s++]);
      putchar('\n');
   }
}
