/*
** Really mindless text compression program.
**
** The intended use of the compression is to be able to fit
** E-text books (Gutenberg, OBI, Wiretap) into a smaller space
** so that they can be read on a machine with very limited store,
** such as a handheld computer.  One implication of this is that
** the form of compression does not need to be completely lossless
** as long as the result is still readable.
**
** This program assumes a linear address space at least large enough
** to load the whole text.  One implication of this is that it won't
** be usable as a simple DOS program, but the alternative was to spend
** more time on this than I felt I could afford.
*/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define FILTER                 /* don't preprocess text if this is      */
                               /* not defined                           */
                               /* this should give LOSSLESS compression */
                               /* useful mainly for testing purposes    */

#define NEARLY 10              /* less than this is "nearly" unused */

#define GRAM 2                 /* size of strings to scan for */
#define GRAMS 4000             /* max size of dictionary of scanned strings */

#define LLIM 250               /* max size of uncompressed line in file */

#define FALSE 0
#define TRUE 1

#define NL '\n'
#define CR '\r'

static unsigned long character_count[256];

/*
** definition
**
** This is a table of definitions for the 256 possible character values.
** Note that character 0 is never used for anything, in order to avoid
** C string termination problems.
**
** For other characters, the table entry is a pointer to a heap string
** which is what that character would expand to if it appeared in the
** compressed string.
**
** Note that this is NOT a recursive concept; the definition is a flattened
** string of characters.  This, if the character "q" only appears before
** "u" and is chosen as symbol "x", then the definition of "x" will be
** "qu" and "q" itself may then be redefined as something else later on.
*/
static unsigned char *definition[256];

/*
** Text storage
**
** "original_size" is the size, in characters, of the original file.
** "text" is a pointer to a heap area of a little bigger than this size,
** so that it can be terminated by a couple of special characters and
** a zero if required.
** "current_size" is the number of characters in "text" which are
** currently valid.
*/
static size_t original_size;
static size_t current_size;
static unsigned char *text;

/*
** abandon
**
** Single point of exit from the program in the event of problems.
*/
static void abandon ( const char *s )
{
   fprintf(stderr, "Abandon: %s\n", s);
   exit(EXIT_FAILURE);
}

/*
** process_char
**
** Called once for each character in the input file.  This is a state
** machine which weeds out things like redundant white space and
** unwanted hyphenation.
*/
static unsigned char *pp;   /* pointer through which process_char writes */

#if defined(FILTER)
static void process_char ( unsigned char c )
{
   static enum state {
      END_PARA,          /* have just seen end of a paragraph         */
      END_LINE,          /* have just seen end of a line              */
      IN_WORD,           /* we are inside a word                      */
      IN_LINE,           /* inside a line, but not inside a word      */
      PEND_SPACE,        /* in white space                            */
      PEND_HYPHEN,       /* just seen a hyphen, may be at end of word */
      PEND_HYPHEN_NL     /* floop-<NL> seen                           */
   } state = END_PARA;

   switch (state) {

   line_ended:
   case END_LINE:
      if (c == NL) {
	 /*
	 ** If we see a second newline after the end of an
	 ** ordinary line, we have come to the end of the
	 ** paragraph.  Issue the second newline and move
	 ** into the END_PARA state.
	 */
	 *pp++ = c;
	 state = END_PARA;
      } else if (c == ' ') {
	 /*
	 ** If we see a leading space after a newline, we have
	 ** see the _other_ kind of paragraph-starting convention,
	 ** and we must change it to the first kind.
	 */
	 *pp++ = NL;
	 state = END_PARA;
      } else if (isalpha(c)) {
	 *pp++ = c;
	 state = IN_WORD;
      } else {
	 *pp++ = c;
	 state = IN_LINE;
      }
      break;

   case END_PARA:
      if (c == ' ') {
	 /*
	 ** Ignore spaces at this point; we know we are at the end of
	 ** a paragraph already.
	 */
      } else if (c == NL) {
	 /*
	 ** Ignore extra blank lines
	 */
      } else {
	 /*
	 ** Something interesting: consider in line context
	 */
	 goto in_line;
      }
      break;

   in_line:
   case IN_LINE:
      state = IN_LINE;
      if (c == ' ') {
	 state = PEND_SPACE;
      } else if (c == NL) {
	 *pp++ = NL;
	 state = END_LINE;
      } else if (isalpha(c)) {
	 *pp++ = c;
	 state = IN_WORD;
      } else {
	 *pp++ = c;
      }
      break;

   case IN_WORD:
      if (isalpha(c)) {
	 *pp++ = c;
      } else if (c == '-') {
	 state = PEND_HYPHEN;
      } else {
	 goto in_line;
      }
      break;

   case PEND_HYPHEN:
      if (isalpha(c)) { /* hyphen within a word */
	 *pp++ = '-';
	 *pp++ = c;
	 state = IN_WORD;
      } else if (c == ' ') {
	 /* absorb spaces after in-word hyphens */
      } else if (c == NL) {
	 state = PEND_HYPHEN_NL;
      } else {
	 *pp++ = '-'; /* flush hyphen */
         goto in_line;
      }
      break;

   case PEND_HYPHEN_NL:
      if (isalpha(c)) {
	 /*
	 ** We have seen a <alpha><-><wsp><NL><wsp><alpha>
	 ** sequence in a word.  Drop the middle of it and
	 ** join the two lines together, which will also
	 ** join the two parts of the word together.
	 */
	 *pp++ = c;
	 state = IN_WORD;
      } else if (c == ' ') {
	 /*
	 ** Absorb spaces at beginning of lines like these
	 */
      } else {
	 *pp++ = '-';
	 *pp++ = NL;
	 state = END_LINE;
	 goto line_ended;
      }
      break;

   case PEND_SPACE:
      if (c == ' ') {
	 /* ignore multiple white space */
      } else if (c == NL) {
	 /*
	 ** White space before line end: take as line end.
	 */
	 *pp++ = NL;
	 state = END_LINE;
      } else {
	 *pp++ = ' ';
	 goto in_line;
      }
      break;

   default:
      fprintf(stderr, "state: %d character: %d\n", state, c);
      abandon("process state machine failure");
   }
}
#endif /* FILTER */

static load_file(FILE *fin)
{
   int c;

   /*
   ** Work out the size of the file by reading it all in, character
   ** at a time.  Slow but sure.
   */
   original_size = 0;
   for (;;) {
      int c = fgetc(fin);
      if (c == EOF) break;
      original_size++;
   }
   printf("original size: %ld characters\n", (long)original_size);

   /*
   ** Allocate some space for the file...
   */
   text = malloc(original_size+10);
   text+=10;
   if (!text) abandon("could not allocate buffer for text");

   /*
   ** Reset the file pointer to the start and load the file in.
   */
   fseek(fin, 0, SEEK_SET);
   pp = text;
   for (;;) {
      int c = fgetc(fin);
      if (c == EOF) break;
      #if defined(FILTER)
         process_char(c);
      #else
         *pp++ = c;
      #endif
   }

   /*
   ** Purge state machine if we have one.
   */
   #if defined(FILTER)
      process_char(NL);
      process_char(NL);
      process_char(NL);
   #endif
   current_size = pp - text;
}

/*
** pchar
**
** Prints a character in such a way that it is always readable,
** if necessary hexifying it.
*/
static void pchar (int n)
{
   if (isprint(n)) {
      putchar(n);
   } else {
      printf("<%02X>", n);
   }
}

/*
** pstring
**
** Prints a given string "safely" through pchar.
*/
static void pstring ( const unsigned char *s )
{
   while (*s) pchar(*s++);
}

static void show_nearlies(void)
{
   int nearlies;
   int c;
   for (c=1, nearlies=0; c<256; c++) {
      int count = character_count[c];
      if (count && (count<NEARLY)) nearlies++;
   }
   printf("nearly empty character slots: %d\n", nearlies);
   if (nearlies) {
      printf("   ");
      for (c=1; c<256; c++) {
         if (character_count[c] == 0) continue;
         if (character_count[c] < NEARLY) pchar(c);
      }
      putchar(NL);
   }
}

static int count_characters(void)
{
   int c, empties;
   unsigned char *p = text;

   /*
   ** First, zero the count array.
   */
   for (c=1; c<256; c++)
      character_count[c] = 0;

   /*
   ** Make sure we don't by accident use anything as a symbol
   ** that the Psion IOREAD function will regard as significant.
   **
   ** (1000 is large enough to prevent these turning up as "nearlies").
   */
   character_count[26] = 1000;
   character_count[NL] = 1000;
   character_count[CR] = 1000;

   while (p != (text+current_size)) {
      character_count[*p]++;
      p++;
   }

   for (c=1, empties=0; c<256; c++) {
      int count = character_count[c];
      if (count == 0) empties++;
   }

   printf("empty character slots: %d\n", empties);

   return empties;
}

struct gram {
   char ch[GRAM];
   unsigned long nrefs;
};

static struct gram grams[GRAMS];
static int cur_grams;
static unsigned long dropped_grams;

static void pgram ( const struct gram *g )
{
   int i;
   for (i=0; i<GRAM; i++) pchar(g->ch[i]);
}

static int sort_nrefs ( const void *L, const void *R )
{
   const struct gram *LL = L;
   const struct gram *RR = R;
   if (LL->nrefs < RR->nrefs) return -1;
   if (LL->nrefs > RR->nrefs) return 1;
   return 0;
}

static int sort_grams ( const void *L, const void *R )
{
   const struct gram *LL = L;
   const struct gram *RR = R;
   return strncmp(LL->ch, RR->ch, GRAM);
}

static void count_gram ( const char *g )
{
   struct gram gram;
   struct gram *q;
   memcpy(gram.ch, g, GRAM);
   q = bsearch(&gram, grams, cur_grams, sizeof(struct gram), sort_grams);
   if (q) {
      q->nrefs++;
   } else if (cur_grams < GRAMS) {
      /*
      ** A gram we haven't seen before, but which we do have room
      ** for in the table.
      */
      gram.nrefs = 1;
      #if 0
         /*
	 ** add this gram in at the end of the table
	 */
	 grams[cur_grams] = gram;
      #else
         /*
	 ** Add this gram in at the start of the table.
	 ** This is likely to be statistically close to where it
	 ** will be going, particularly if it is a late arrival.
	 */
         if (cur_grams) memmove(grams+1, grams, cur_grams*sizeof(struct gram));
         grams[0] = gram;
      #endif
      cur_grams++;
      qsort(grams, cur_grams, sizeof(struct gram), sort_grams);
   } else {
      dropped_grams++;
   }
}

/*
** valid_gram
**
** True if the string is an allowed n-gram.  False if it
** contains NL and is therefore prohibited.
*/
static int valid_gram ( const unsigned char *p )
{
   int i;
   for (i=0; i<GRAM; i++)
      if (*p++ == NL) return FALSE;
   return TRUE;
}

/*
** count_grams
**
** Run through the current text counting each n-gram
** as we go.
*/
static void count_grams(void)
{
   char *p = text;
   char *plim = text+current_size-GRAM+1;
   dropped_grams = 0;
   while (p != plim) {
      if (valid_gram(p))
	 count_gram(p);
      p++;
   }
}

static void reset_grams(void)
{
   int cut = cur_grams/100;
   int i;
   if (cut) {
      memmove(grams, grams+cut, (cur_grams-cut)*sizeof(struct gram));
      cur_grams -= cut;
   }
   for (i=0; i<cur_grams; i++)
      grams[i].nrefs = 0;
}

static void replace_gram(int gramno, struct gram *g)
{
   char *from = text;
   char *to = text;
   char *org_limit = text + current_size;
   char *limit = text+current_size-GRAM+1;
   current_size = 0;
   while (from < limit) {
      if (!strncmp(from, g->ch, GRAM)) {
	 *to++ = gramno;
	 from += GRAM;
      } else {
	 *to++ = *from++;
      }
      current_size++;
   }
   while (from != org_limit) {
      *to++ = *from++;
      current_size++;
   }
}

/*
** replace_string
**
** Replace an arbitrary string with a shorter string replacement
** throughout the current version of the text.
*/
static int replace_string ( const unsigned char *s,
			   const unsigned char *r )
{
   int n = 0;
   unsigned char *beg = text;
   int rlen = strlen(r);
   int cut = strlen(s)-strlen(r);  /* number of characters to lose */
   text[current_size] = 0; /* terminate current text */
   while (beg = strstr(beg, s)) {
      memcpy(beg, r, rlen); /* perform replacement */
      n++;
      if (cut) memmove(beg+rlen, beg+rlen+cut, strlen(beg+rlen+cut)+1);
      current_size -= cut;
   }
   return n;
}

static void expand_gram ( unsigned char *buf, struct gram *g )
{
   int i;
   *buf = 0;
   for (i=0; i<GRAM; i++)
      strcat(buf, definition[g->ch[i]]);
}

static void compute_grams(void)
{
   unsigned char ch;
   for (ch=255; ch>=1; ch--) {
      struct gram *g;
      if (!character_count[ch]) {
	 /*
	 ** We can use this character value as a gram definition.
	 */
	 unsigned char x[100];

	 printf("size now %ld (down %2.1f%%) ",
		(long)current_size,
		100.0*(original_size-current_size)/(double)original_size);
	 fflush(stdout);

	 count_grams();

	 qsort(grams, cur_grams, sizeof(struct gram), sort_nrefs);
	 g = &grams[cur_grams-1];

	 printf("top %d-gram is \"", GRAM);
	 pgram(g);
	 printf("\" (ref %ld, drop %lu)\n", g->nrefs, dropped_grams);

	 replace_gram(ch, g);

	 expand_gram(x, g);

	 reset_grams();
	 cur_grams--;   /* we no longer have this one */

	 free(definition[ch]);
	 definition[ch] = strdup(x);
	 printf("   character value %d now expands to \"", ch);
	 pstring(definition[ch]);
	 putchar('"');
	 putchar(NL);
      }
   }
}

static void dump_table ( FILE *fout )
{
   int ch;
   for (ch=0; ch<=255; ch++) {
      const unsigned char *def = definition[ch];
      if (strlen(def)==1 && *def == ch) {
	 /*
	 ** If the character is defined as itself, just put out
	 ** a blank line.
	 */
	 fputc(NL, fout);
      } else {
	 /*
	 ** Otherwise, put out the definition of the character
	 ** on a line.
	 */
	 fprintf(fout, "%s\n", definition[ch]);
      }
   }
}

/*
** initialise_definitions
**
** Set up the definition array with pointers to heap strings
** defining the symbol as equivalent to itself.
*/
static void initialise_definitions(void)
{
   int c;
   for (c=1; c<256; c++) {
      unsigned char x[2];
      x[0] = c; x[1] = 0;
      definition[c] = strdup(x);
      if (!definition[c]) abandon("can't initialise definition");
   }
}

/*
** optimise_lines
**
** This function scans the text and re-formats all the paragraphs
** to give maximal line sizes.  The effect of this is to allow the
** compression algorithms to work slightly more effectively because
** word endings are more often seen as with a space (which can be
** involved in compression) than before (NL can not be part of
** a compression symbol).
**
** Note that any such change leaves the text exactly the same
** size as it was before.
*/
#if defined(FILTER)
static void optimise_lines(void)
{
   unsigned char *p;

   /*
   ** Terminate the current text.
   */
   text[current_size] = 0;

   /*
   ** Phase 1
   **
   ** Make each paragraph (where paragraphs are defined as being
   ** separated by blank lines) be a single line with spaces
   ** separating words.
   */
   p = text;
   while (p = strchr(p, NL)) {
      if (p[1] == NL) {
	 /*
	 ** THIS newline is the end of the paragraph, the NEXT
	 ** one is the required blank line.  We don't want to
	 ** touch this, and THEN we want to start at the start
	 ** of the next paragraph.
	 */
	 p += 2;
      } else if (p[1] == 0) {
	 /*
	 ** Last newline in the file.
	 */
	 break;
      } else {
	 /*
	 ** Simple end of line, but not end of paragraph.
	 */
	 *p++ = ' ';
      }
   }

   /*
   ** Phase 2
   **
   ** For each paragraph/line in the file, reduce it to lines shorter
   ** than or equal to LLIM characters by converting spaces to NLs.
   */
   p = text;
   while (*p) {
      unsigned char *end; /* points to end of this line (NL character) */

      /*
      ** "p" points to the start of the current line.
      ** If this is a blank line, just skip over it.
      */
      if (*p == NL) {
	 p++;
	 continue;
      }

      /*
      ** We are at the start of a non-empty line.
      ** Locate it's NL character and thus figure out
      ** how long the line is.
      */
      end = strchr(p, NL);

      /*
      ** Reduce this line, while it is still too large.
      */
      while ( (end-p) > LLIM ) {
	 unsigned char *brk = p+LLIM;   /* candidate breakpoint */
	 while (*brk != ' ') brk--;     /* run back to previous space */
	 *brk = NL;                     /* overwrite with newline */
	 p = brk+1;                     /* new beginning of line */
      }

      p = end+1;                        /* move to start of next line */
   }
}
#endif

/*
** main
*/
int main(int argc, char *argv[])
{
   FILE *fin, *fout;
   int n;

   if (argc != 3) abandon("two arguments (in, out) required");

   fin = fopen(argv[1], "r");
   if (!fin) abandon("could not open input file");
   
   load_file(fin);
   fclose(fin);

   /*
   ** Perform some text replacements.
   **
   ** For "Terminal Compromise":
   **   ^[ ^H ^\   ->  e-grave
   **   ^[ ^B ^\   ->  e-acute
   **   <130>      ->  e-acute
   **
   ** General textual:
   **   Spaced out ellipsis ". . ." to "..."
   **   Compact lines of stars
   */
   #if defined(FILTER)
      n = replace_string("\x1B\x08\x1C", "\x8A");
      if (n) printf("e-grave replacements: %d\n", n);
      n = replace_string("\x1B\x02\x1C", "\x82");
      n += replace_string("<130>", "\x82");
      if (n) printf("e-acute replacements: %d\n", n);
      n = replace_string(". .", "..");
      if (n) printf("ellipsis compressions: %d\n", n);
      n = replace_string("* *", "**");
      if (n) printf("star compressions: %d\n", n);
   #endif

   /*
   ** Reformat paragraphs
   */
   #if defined(FILTER)
      optimise_lines();
   #endif

   initialise_definitions();

   while (count_characters())
      compute_grams();

   show_nearlies();

   /*
   ** Create the output file
   */
   fout = fopen(argv[2], "w");
   if (!fout) abandon("could not open output file");
   definition[0] = "!!Compressed!!";
   dump_table(fout);
   fwrite(text, 1, current_size, fout);
   fclose(fout);

   return EXIT_SUCCESS;
}
