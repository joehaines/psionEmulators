// TCR Decompressor  v1.00
//
// (c) UK  Andrew Giddings  1998
// gidds@cix.co.uk
//
// DESCRIPTION
//
// TCR is a format for compressed text files.  It's especially useful for
// the Psion 5 VReader text viewer, or the similar Psion 3 Reader.
//
// The format is easy to describe: after the header is a dictionary of
// 256 strings, each preceded by a length byte.  The rest of the file is
// a list of indices into this dictionary.
//
// This decompressor is written in the GCC dialect of C (actually, the Atari
// version) but it should be easily portable to most modern dialects.
//
// This is my own work, not derived from Ian Young's or Barry Childress'
// work (although of course it'll be very similar as it's the same file format).
//
// RESTRICTIONS
//
// You're free to use this source as you like - to compile a version for your
// own machine, to get ideas, etc.  However: you're not allowed to make
// any money from it (except for reasonable distribution costs, etc.), and if
// you do use it you must give me credit!  Reasonable?  (An email, postcard
// or some dosh would be nice, too :)

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>

// Constants

const char *FILE_HEADER = "!!8-Bit!!";
const char *COPYRIGHT = "TCR Decompressor  v1.00   (c) UK  Andrew Giddings  1998";

const int SUCCESS = 0, FAILURE = -1;

// Prototypes

void doFile(char *inName);
void decode(FILE *in, FILE *out);
void fatal(const char *format,...);

/////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
	fprintf(stderr, "%s\r\n", COPYRIGHT);

	// If no params, filter stdin to stdout
	if (argc == 1)
		decode(stdin, stdout);
	else
		while (--argc)
			doFile(*++argv);

	exit(0);
}

/////////////////////////////////////////////////////////////////////////////////////////

void doFile(char *inName)
{	char outName[PATH_MAX], *s;
	FILE *in, *out;

	strcpy(outName, inName);
	s = outName + strlen(outName) - 4;
	if (!strcasecmp(s, ".tcr"))
		strcpy(s, ".txt");
	else
		strcat(outName, ".txt");

	if (access(outName, F_OK) == 0)
		fatal("%s already exists", outName);

	errno = 0;

	if (!(in = fopen(inName, "rb")))
		fatal("Couldn't open %s - %s", inName, strerror(errno));

	if (!(out = fopen(outName, "wb")))
		fatal("Couldn't create %s - %s", outName, strerror(errno));

	printf("Uncompressing %s into %s...\r\n", inName, outName);

	decode(in, out);

	fclose(in);
	fclose(out);
}

/////////////////////////////////////////////////////////////////////////////////////////

void decode(FILE *in, FILE *out)
{	char buffer[256], *table[256];
	int c, i, length;

	// Set 64Kb stream buffers, for speed
	setvbuf(in, NULL, _IOFBF, 65536);
	setvbuf(out, NULL, _IOFBF, 65536);

	// Check header

	errno = 0;

	fread(buffer, strlen(FILE_HEADER), 1, in);
	if (errno)
		fatal("Couldn't read header - %s", strerror(errno));

	if (strncmp(buffer, FILE_HEADER, strlen(FILE_HEADER)))
		fatal("Not a TCR compressed file");

	// Read table

	for (i = 0; i < 256; ++i)
	{	if ((length = getc(in)) < 0)
			fatal("Couldn't read entry %i length - %s", i, strerror(errno));
		fread(buffer, length, 1, in);
		if (errno)
			fatal("Couldn't read entry %i - %s", i, strerror(errno));
		buffer[length] = '\0';
		table[i] = strdup(buffer);
		if (!table[i])
			fatal("Out of memory for entry %i", i);
	}

	// Decode file

	while ((c = getc(in)) >= 0)
		if (fputs(table[c], out) < 0)
			fatal("Couldn't write - %s", strerror(errno));

	if (errno)
		fatal("Couldn't read code - %s", strerror(errno));

	// Release table

	for (i = 0; i < 256; ++i)
		free(table[i]);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Handle a fatal error

void fatal(const char *format,...)
{	va_list list;

	va_start(list, format);
	vfprintf(stderr, format, list);
	fprintf(stderr, "!\r\nPress RETURN...\r\n\a");
	getchar();
	exit(-1);
}