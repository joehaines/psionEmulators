#include <stdio.h>
#include <stdlib.h>
void message(void);
void message2(void);

void main(int argc, char **argv)
{
	FILE *Fileptr, *Writeptr;
	char Text, Pre1, Pre2;

	puts("By Roger Muggleton hzk@cix.co.uk (c)1997");

	if(argc<3)message();

	Fileptr = fopen(argv[1], "rb");
	if( Fileptr == 0 )
	{
		printf("File not found\n");
		exit(0);
	}

	Text = 6;

	fseek(Fileptr, 8L, SEEK_SET);  // go to 3rd UID

	Text = fgetc(Fileptr);  // read 1st byte of UID
	if(Text != -123)  // hex 85
  	{
		fclose(Fileptr);
		message2();
	}

	if(fgetc(Fileptr) != 0)  // read 2nd byte of UID
	{
		fclose(Fileptr);
		message2();
	}

	if(fgetc(Fileptr) != 0)  // read 3rd byte of UID
	{
		fclose(Fileptr);
		message2();
	}

	if(fgetc(Fileptr) != 16)  // read 4th byte of UID
	{
		fclose(Fileptr);
		message2();
	}

	// move file pointer to start of OPL code.

	fseek(Fileptr, 26L, SEEK_CUR);

	Writeptr = fopen(argv[2], "wb");

	// this stuff checks whether the OPL code starts 2 bytes later,
	// if so, Pre2 will be 0x00, and so these two bytes will not be written.
	Pre1 = fgetc(Fileptr);
	Pre2 = fgetc(Fileptr);
	if(Pre2 != 0)  // i.e. OPL code starts here.
	{
		fprintf(Writeptr, "%c",Pre1);
		fprintf(Writeptr, "%c",Pre2);
	}

	while (Text != EOF)
	{
		Text = fgetc(Fileptr);
		// Look for ASCII value 1 which marks end of OPL code.
		if(Text == 1)
			break;

		// Replace ASCII 6 with CR+LF
		if(Text == 6)
			fprintf(Writeptr, "\r\n");
		else
			fprintf(Writeptr, "%c",Text);
     }

	fclose(Fileptr);
	fclose(Writeptr);
	printf("Done.\r\n");
}

void message(void)
{
	puts("Converts OPL32 source to ascii text. Version 2.01b");
	puts("Use: OPL32TXT Infile Outfile.");
	exit(0);
}

void message2(void)
{
	puts("Not a valid OPL32 source file.");
	exit(0);
}