#include <iostream.h>

#include <kcmdlineargs.h>


int main(int argc, char **argv)
{
    // read argument
	KCmdLineArgs::init(argc, argv, "check_score", 0, 0, true);
	static KCmdLineOptions options[] =
		{ {"+file", "Count of removed lines", 0} };
	KCmdLineArgs::addCmdLineOptions(options);
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if ( args->count()==0 ) KCmdLineArgs::usage();
	QString s = args->arg(0);
	bool ok;
	uint nb = s.toUInt(&ok);
	if ( !ok ) qFatal("The argument is not an unsigned integer.");

    cout << "--------------" << endl;

    // constants
    const uint width = 10;
    const uint height = 22;
    const uint nbLinesPerLevel = 10;
    const uint blocksPerPiece = 4;

	// compute level
	uint level = (nb / nbLinesPerLevel) + 1;
	cout << "Level : " << level << endl << endl;

    // compute max score due to drop down
    uint ddScore =
        ((width*height) / blocksPerPiece) * height; // board full of pieces
    ddScore +=
        ((nb * width) / blocksPerPiece) * height; // piece for removed lines
    cout << "Max Score from drop down pieces : " << ddScore << endl;

    // compute max score due to removed lines
    uint score = 0;
    uint nb_tetris = 0;
    while ( nb>=4 ) {
        nb -= 4;
        level = (nb / nbLinesPerLevel) + 1;
        score += 1200 * level;
        nb_tetris++;
    }
    if ( nb==3 ) score += 300;
    else if ( nb==2 ) score += 100;
    else if ( nb==1 ) score += 40;
    cout << "Max score from removed lines : " << score << endl;
    cout << "Max count of tetris : " << nb_tetris << endl;

    cout << endl;
    cout << "Max score : " << ddScore + score << endl;

    cout << "--------------" << endl;
	return 0;
}
