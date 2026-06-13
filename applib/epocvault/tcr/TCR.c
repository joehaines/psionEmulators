// TCR Compressor  v1.00
//
// (c) UK  Andrew Giddings  1998
// gidds@cix.co.uk
//
// DESCRIPTION
//
// TCR is a format for compressed text files.  It's especially useful for
// the Psion 5 VReader text viewer, or the similar Psion 3 Reader.
//
// This compressor is written in the GCC dialect of C (actually, the Atari
// version) but it should be easily portable to most modern dialects.
//
// The TCR compression format is easy to describe: after the header is
// a dictionary of 256 strings, each preceded by a length byte.  The rest
// of the file is a list of indices into this dictionary.
//
// The compressor works by starting with each code defined as itself.
// While there's an unused code, it finds the most common two-code
// combination, and creates a new code for it, replacing all occurrences
// in the text with the new code.
// It also searches for codes that are always followed by another,
// which it can merge, possibly freeing up some.
//
// This program's designed for max. compression, and speed, at the
// expense of memory (e.g. 64Kb frequency table, and 64Kb reserved
// for the dictionary insted of allocating dynamically), and has several
// fence values.  As a result, zero bytes aren't allowed in the file to be
// compressed (big loss!).  (I guess it also assumes a linear address
// space, but hey, who's worried about brain-dead ones that aren't?)
//
// This program isn't derived from either Ian Young's slow ZVRZ (coz I
// couldn't understand it), or Barry Childress' TCReader (coz he wouldn't
// let me see his source).  Instead, it uses ideas from Ian's version, but
// is much better coz I wrote it. :)  I've speeded it up massively, and
// added some optimisations of my own.  Thanks also to the guys in the
// CiX algorithms conference for ideas, especially Peter Maud.
//
// RESTRICTIONS
//
// You're free to use this source as you like - to compile a version for your
// own machine, to get ideas, etc.  However: you're not allowed to make
// any money from it (except for reasonable distribution costs, etc.), and if
// you do use it you must give me credit!  Reasonable?  (An email, postcard
// or some dosh would be nice, too :)

// Headers

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <stat.h>
#include <time.h>

// Constants

const char *COPYRIGHT = "TCR Compressor  v1.00   (c) UK  Andrew Giddings  1998";
const char *FILE_HEADER = "!!8-Bit!!";
const int SCAN_INTERVAL = 8;	// How many creates to do before another scan is needed

// Why aren't these in a standard header?
const int FALSE = 0, TRUE = 1;

// Globals

unsigned char *text;		// The text file we're working on (zero-terminated)
int textLength;			// And its length
unsigned char dict[256][256];	// The 256 dictionary strings, zero-terminated
int dictLength[257];		// Their lengths (for speed; always = strlen(dict[])) - and a fence
int dictTotalLength;		// The sum + 256, i.e. the size of the dictionary within the TCR file

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

/////////////////////////////////////////////////////////////////////////////////////////
// Search the text for frequencies of 2-code pairs.
// To keep the table small, it's only made up of unsigned chars.
// So when a freq reaches 255, we assumes it's always worth replacing,
// and we can stop searching.  Otherwise, we must search for the best
// bet, taking into account the space the extra dictionary entry will need.
// Also, we can't create a code if it'd be longer than 255 chars.
// We can use a scan to determine several replacements - as long as we
// clear any related possibilities each time.  This saves quite a bit of time
// for medium-length files (for larger ones, scans overflow easier), and hardly
// affects the compression.

int findBestCreate(void)
{
	unsigned char *s = text, c1 = *s++, c2;
	static unsigned char freq[256][256];	
		// (freq[a][b] is # occurrences of a followed by b)
	register int bestSaving, saving, bestc1, bestc2, i1, i2;
	static int skip = 0;	// The number of times to skip a full scan - zero to do a scan this time

	if (!skip--)
	{	skip = SCAN_INTERVAL;
		memset(freq, 0, sizeof(freq));
// fputc('?', stderr);
		while ((c2 = *s++))
		{	if (++freq[c1][c2] == 255
			&& dictLength[c1] + dictLength[c2] < 256)
			{	skip = 0;
				return (c1 << 8) | c2;
			}
			c1 = c2;
		}
	}

	// None have overflowed, so we gotta search the freqs ourselves

	bestSaving = bestc1 = bestc2 = 0;
	for (i1 = 1; i1 < 256; ++i1)
		if (dictLength[i1])
			for (i2 = 1; i2 < 256; ++i2)
			{	saving = freq[i1][i2] - dictLength[i1] - dictLength[i2];
				// Note - a sequence of repeated characters will cause overestimation!
				// No easy way around this, but it's not a major problem...
				if (saving > bestSaving
				&& dictLength[i1] + dictLength[i2] < 256)
				{	bestc1 = i1;
					bestc2 = i2;
					bestSaving = saving;
				}
			}

	if (bestc1 == 0)
	{	skip = 0;
		return 0;
	}

	// Now zero all frequencies that would interfere with the one we're about to
	// change, so we don't do a full scan next time.
	freq[bestc1][bestc2] = 0;
	for (i1 = 1; i1 < 256; ++i1)
		freq[bestc2][i1] = freq[i1][bestc1] = 0;

	return (bestc1 << 8) | bestc2;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Create a new code t, combining two existing ones (f1 and f2),
// and replace all occurrences of f1 f2 in the text with t.

void create(unsigned char t, unsigned char f1, unsigned char f2)
{
	register unsigned char *s = text, *d = text, c = *s;

// fputc('.', stderr);

	// Substitute all occurrences in the text

	while (c)
	{	if (c != f1)
		{	*d++ = c;
			c = *++s;
		}
		else if ((c = *++s) != f2)
			*d++ = f1;
		else
		{	*d++ = t;
			c = *++s;
		}
	}

	*d = '\0';
	textLength = d - text;

	// Now adjust the dictionary

	strcpy(dict[t], dict[f1]);
	strcat(dict[t], dict[f2]);
	dictLength[t] = dictLength[f1] + dictLength[f2];
	dictTotalLength += dictLength[t];

	if (!strchr(text, f1))
	{	dict[f1][0] = '\0';
		dictTotalLength -= dictLength[f1];
		dictLength[f1] = 0;
	}

	if (!strchr(text, f2))
	{	dict[f2][0] = '\0';
		dictTotalLength -= dictLength[f2];
		dictLength[f2] = 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Search for any codes that are always followed by another,
// and merge the second code into the first, replacing
// all occurrences in the text.  This also extends to chains of
// three or more codes that always appear in sequence.
// This may not be quite as quick as a single search/create, but
// it can do all such sequences in one go.
// Returns TRUE if any replacements were made.

int merge(void)
{
	register unsigned char *s = text, *d, c1 = *s++, c2;
	static int follows[256], freq[256], skip[256], seen[256];
	int i, j, found = FALSE;

	memset(follows, 0, sizeof(follows));
	memset(freq, 0, sizeof(freq));
	memset(seen, 0, sizeof(seen));

	// Scan for following characters
	// Set follows[c] to 0 if no characters ever follow c;
	// c2 if only c2 ever follows it, and -1 if more than one
	// character does.

	while ((c2 = *s++))
	{	if (follows[c1] < 0)
			;
		else if (follows[c1] == c2)
			++freq[c1];
		else if (follows[c1] == 0)
		{	follows[c1] = c2;
			freq[c1] = 1;
		}
		else
			follows[c1] = -1;
		c1 = c2;
	}
	follows[c1] = -1;	// Can't merge past EOF

	// Find sequences to merge.  Adjust the dictionary.
	// Set up a 'skip' array for fast merging of the text.

	for (i = 1; i < 256; ++i)
	{	skip[i] = 1;
		j = follows[i];
		while (j > 0 && dictLength[i] + dictLength[j] < 256)
		{	if (freq[i] < dictLength[j])	// Would increase size!
				break;
			found = TRUE;
			++skip[i];
// fputc('+', stderr);
			strcat(dict[i], dict[j]);
			dictLength[i] += dictLength[j];
			dictTotalLength += dictLength[j];
			j = follows[j];
		}
	}

	if (!found)
		return FALSE;

	// Now perform all replacements - and note which codes are still used

	s = d = text;
	while ((*d++ = *s))
	{	seen[*s] = TRUE;
		s += skip[*s];
	}

	textLength = d - text - 1;

	// Clear any unused codes

	for (i = 1; i < 256; ++i)
		if (!seen[i])
		{	dict[i][0] = '\0';
			dictTotalLength -= dictLength[i];
			dictLength[i] = 0;
		}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Return an unused code, if there is one, or -1 if they're all used.

int findFreeCode(void)
{
	register int i = 1;		// Ignore code 0

	// Assume dictLength[] has extra fence [256] = 0.

	while (dictLength[i])
		++i;

	return (i >= 256) ? -1 : i;
}

/////////////////////////////////////////////////////////////////////////////////////////

void readFile(const char *fsp)
{
	FILE *in;
	struct stat statbuf;
		
	if (stat(fsp, &statbuf))
		fatal("Can't check %s: %s", fsp, strerror(errno));

	textLength = statbuf.st_size;

	text = (char *) malloc(textLength + 1);
	if (!text)
		fatal("Not enough memory for %s", fsp);

	in = fopen(fsp, "rb");
	if (!in)
		fatal("Can't open %s: %s", fsp, strerror(errno));

	fread(text, textLength, 1, in);
	
	if (ferror(in))
		fatal("Can't read %s: %s", fsp, strerror(errno));

	fclose(in);

	text[textLength] = '\0';

	if (memchr(text, '\0', textLength))
		fatal("Can't compress %s: contains a zero byte", fsp);
}

/////////////////////////////////////////////////////////////////////////////////////////

void writeFile(const char *fsp)
{
	FILE *out;
	int i;

	out = fopen(fsp, "wb");
	if (!out)
		fatal("Can't create %s: %s", fsp, strerror(errno));

	fputs(FILE_HEADER, out);

	for (i = 0; i < 256; ++i)
		fprintf(out, "%c%s", dictLength[i], dict[i]);

	fwrite(text, textLength, 1, out);
	
	if (ferror(out))
		fatal("Can't write %s: %s", fsp, strerror(errno));

	fclose(out);

	free(text);
	text = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Compress the named file

void compressFile(const char *fsp)
{
	unsigned char *s;
	int seen[256];
	char outFsp[PATH_MAX], *p;
	int origLength, c, newLength, i;
	time_t t1, t2;

	fprintf(stderr, "TCR compressing %s\r\n", fsp);

	// Read file

	readFile(fsp);

	origLength = textLength;

	t1 = time(NULL);

	// Init dictionary

	memset(seen, 0, sizeof(seen));
	s = text;
	while (*s)
		seen[*s++] = TRUE;

	dictTotalLength = 256;
	for (c = 0; c < 256; ++c)
		if (seen[c])
		{	dict[c][0] = c;
			dict[c][1] = '\0';
			dictLength[c] = 1;
			dictTotalLength++;
		}
		else
		{	dict[c][0] = '\0';
			dictLength[c] = 0;
		}

	dictLength[256] = 0;	// Fence value

	// Main compression loop

	merge();

	while (TRUE)
	{	fprintf(stderr, "\r%.2f%%  ", (strlen(FILE_HEADER) + dictTotalLength + textLength) * 100.0 / origLength);
		if ((c = findFreeCode()) >= 0)
		{	if ((i = findBestCreate()))
			{	create(c, i >> 8, i & 0xFF);
				continue;
			}
		}
		if (!merge())
			break;
	}

	t2 = time(NULL);

	// Save file
	// (This assumes Unix-style filenames, allowing multiple dots etc.)

	strcpy(outFsp, fsp);
	p = outFsp + strlen(outFsp) - 4;
	if (!strcasecmp(p, ".txt"))
		strcpy(p, ".tcr");
	else
		strcat(outFsp, ".tcr");

	writeFile(outFsp);

	// Show stats

	newLength = strlen(FILE_HEADER) + dictTotalLength + textLength;
	fprintf(stderr, "\rReduced from %i to %i (down to %.2f%%) in %i seconds\n", origLength, newLength, newLength * 100.0 / origLength, (int) difftime(t2, t1));
}

/////////////////////////////////////////////////////////////////////////////////////////
// Process all args as filenames

int main(int argc, char *argv[])
{
	fprintf(stderr, "%s\r\n", COPYRIGHT);

	if (!argc)
	{	fprintf(stderr, "Usage: %s <file>...\r\n", argv[0]);
		fprintf(stderr, "Compresses each <file> into <file>.tcr\r\n");
		exit(0);
	}

	while (--argc)
		compressFile(*++argv);
	exit(0);
}