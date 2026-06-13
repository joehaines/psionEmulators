// TCR Decompressor  v1.02
//
// (c) UK  Andrew Giddings  1999
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
// You're free to use this source as you like.  I _encourage_ you to compile
// a version for your own machine, read it for ideas, let me know of any bugs
// or improvements, etc.  However: you're not allowed to make any money
// from it (except for reasonable distribution costs, etc.), and if you do
// use it you must credit me and let me know!  Reasonable?  (Monetary
// appreciation would be nice, too :)
//
// HISTORY
//
// v1.00
//	First version.
//
// v1.01
//	Handle zero codes.  Although my compressor doesn't use the zero
//	code, some other compressors may.
//
// v1.02
//	Use internal buffers for speed.  Instead of relying on the stdio package
//	to buffer the i/o, we now decompress from one large buffer to another,
//	and handle file reads and writes as necessary.  This is much more
//	complicated, but it runs about four times as fast!
//	Also tighten up checks for dodgy files.

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
const char *COPYRIGHT = "TCR Decompressor  v1.02   (c) UK  Andrew Giddings  1999";

enum return_value {SUCCESS = 0, FAILURE = -1};

// Prototypes

void doFile(char *inName);
void decode(FILE *inFile, FILE *outFile);
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

	exit(SUCCESS);
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
		fatal("Can't open %s - %s", inName, strerror(errno));

	if (!(out = fopen(outName, "wb")))
		fatal("Can't create %s - %s", outName, strerror(errno));

	printf("Decompressing %s into %s...\r\n", inName, outName);

	decode(in, out);

	fclose(in);
	fclose(out);
}

/////////////////////////////////////////////////////////////////////////////////////////

#define BUFSIZE 65536

void decode(FILE *inFile, FILE *outFile)
{	unsigned char *dict[256], *inBuf, *outBuf, *in, *out, *inEnd, *outEnd;
	int c, i, dictLen[256];

	// Check header

	inBuf = (char *) malloc(BUFSIZE);
	if (!inBuf)
		fatal("Not enough memory for input buffer");
	outBuf = (char *) malloc(BUFSIZE);
	if (!outBuf)
		fatal("Not enough memory for input buffer");

	errno = 0;

	fread(inBuf, 1, strlen(FILE_HEADER), inFile);
	if (errno)
		fatal("Can't read header - %s", strerror(errno));

	if (strncmp(inBuf, FILE_HEADER, strlen(FILE_HEADER)))
		fatal("Not a TCR compressed file");

	// Read dictionary

	for (i = 0; i < 256; ++i)
	{	if ((dictLen[i] = getc(inFile)) < 0)
			fatal("Can't read entry %i length - %s", i, strerror(errno));
		if (dictLen[i] > 255)
			fatal("Invalid entry %i length %i - %s", i, dictLen[i], strerror(errno));
		if (!(dict[i] = (char *) malloc(dictLen[i])))
			fatal("Out of memory for entry %i", i);
		if (fread(dict[i], 1, dictLen[i], inFile) < dictLen[i])
			if (errno)
				fatal("Can't read entry %i - %s", i, strerror(errno));
			else
				fatal("Malformed TCR file - too short");
	}

	// Decode file

	in = inEnd = inBuf;
	out = outBuf;
	outEnd = outBuf + BUFSIZE;

	for (;;)
	{	if (in >= inEnd)
		{	int inLen;
			inLen = fread(inBuf, 1, BUFSIZE, inFile);
			if (inLen < 0)
				fatal("Can't read - %s", strerror(errno));
			else if (inLen == 0)
				break;
			in = inBuf;
			inEnd = inBuf + inLen;
		}

		c = *in++;
		if (dictLen[c] == 0)
		{	printf("Warning: unused code %i found...\r\n", c);
			continue;
		}

		if (out + dictLen[c] > outEnd)
		{	if (fwrite(outBuf, 1, out - outBuf, outFile) < out - outBuf)
				fatal("Can't write - %s", strerror(errno));
			out = outBuf;
		}
		memcpy(out, dict[c], dictLen[c]);
		out += dictLen[c];
	}

	if (fwrite(outBuf, 1, out - outBuf, outFile) < out - outBuf)
		fatal("Can't write - %s", strerror(errno));

	// Free dictionary

	for (i = 255; i >= 0; --i)
		free(dict[i]);
	free(outBuf);
	free(inBuf);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Handle a fatal error

void fatal(const char *format,...)
{	va_list list;

	va_start(list, format);
	vfprintf(stderr, format, list);
	fprintf(stderr, "!\r\nPress RETURN...\r\n\a");
	getchar();
	exit(FAILURE);
}