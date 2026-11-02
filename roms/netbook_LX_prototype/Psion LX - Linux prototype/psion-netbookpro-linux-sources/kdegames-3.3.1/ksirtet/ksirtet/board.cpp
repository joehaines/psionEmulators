#include "board.h"
#include "board.moc"

#include "common/misc_ui.h"
#include "common/factory.h"
#include "piece.h"


using namespace KGrid2D;

KSBoard::KSBoard(bool graphic, QWidget *parent)
    : Board(graphic, new GiftPool(parent), parent),
      filled(matrix().height()), linesRemoved(4)
{
    linesRemoved.fill(0);
}

void KSBoard::copy(const GenericTetris &g)
{
	Board::copy(g);
	filled = static_cast<const KSBoard &>(g).filled.copy(); // deep copy
}

void KSBoard::start(const GTInitData &data)
{
	addRemoved = 0;
    for (uint i=0; i<4; i++) {
        linesRemoved[i] = 0;
        emit removedLinesUpdated(i, 0);
    }
    Board::start(data);
}

void KSBoard::computeInfos()
{
    Board::computeInfos();

    filled.fill(0);
    for (uint j=0; j<firstClearLine(); j++)
        for (uint i=0; i<matrix().width(); i++) {
            Coord c(i, j);
            if ( matrix()[c] ) filled[j]++;
        }
}

bool KSBoard::needRemoving()
{
	for(uint j=0; j<firstClearLine(); j++)
		if ( filled[j]==matrix().width() ) return true;
	return false;
}

bool KSBoard::toBeRemoved(const Coord &c) const
{
    return ( filled[c.second]==matrix().width() );
}

void KSBoard::remove()
{
    uint nbFullLines = 0;
	for (uint k=0; k<firstClearLine(); k++)
        if ( filled[k]==matrix().width() ) nbFullLines++;

    Board::remove();

	updateRemoved(nbRemoved() + nbFullLines);
    linesRemoved[nbFullLines-1]++;
    emit removedLinesUpdated(nbFullLines-1, linesRemoved[nbFullLines-1]);
	addRemoved += nbFullLines;

    if ( isArcade() ) return;

	// Assign score according to level and nb of full lines (gameboy style)
	switch (nbFullLines) {
	 case 0: break;
	 case 1: updateScore( score() + 40   * level() ); break;
	 case 2: updateScore( score() + 100  * level() ); break;
	 case 3: updateScore( score() + 300  * level() ); break;
	 case 4: updateScore( score() + 1200 * level() ); break;
	}

    updateLevel();
}

bool KSBoard::toFall(const Coord &c) const
{
    return ( filled[c.second-1]==0 );
}

//-----------------------------------------------------------------------------
// Multiplayers methods
uint KSBoard::gift()
{
	uint g = (addRemoved>1 ? addRemoved-1 : 0);
	addRemoved = 0;
	return g;
}

void KSBoard::setGarbageBlock(const Coord &c)
{
   Block *gb = currentPiece()->garbageBlock();
   gb->sprite()->show();
   setBlock(c, gb);
}

bool KSBoard::_putGift(uint nb)
{
	if ( nbClearLines()==0 ) return false;

	// lift all the cases of one line
    // (nbClearLine!=0 --> firstClearLine!=height
	for (uint j=firstClearLine(); j>0; j--)
        for (uint i=0; i<matrix().width(); i++)	{
            Coord src(i, j-1);
            Coord dest(i, j);
            moveBlock(src, dest);
        }

	// fill the emptied low line with garbage :)
	uint _nb = nb;
	uint i;
	do {
		i = randomGarbage.getLong(matrix().width());
        Coord c(i, 0);
		if ( matrix()[c]!=0 ) continue;
        setGarbageBlock(c);
		_nb--;
	} while ( _nb!=0 );

	return true;
}

bool KSBoard::putGift(uint nb)
{
	for (uint k=0; k<nb; k++) {
		if ( !_putGift(matrix().width() - k - 1) ) return false;
        computeInfos();
    }
	return true;
}


//-----------------------------------------------------------------------------
const KSBoard::StageData KSBoard::STAGE_DATA[Nb_Stages] = {
    { 4,  1, false, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0}},
    { 10, 1, false, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0}},
    { 20, 1, false, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0}},

    // 10 blocks on each sides
    { 10, 2, true,  {10,  0,  0,  0,  0,  0,  0,  0,  0, 10}},
    // 10 blocks height triangle
    { 15, 2, true,  { 0,  2,  4,  7, 10, 10,  7,  4,  2,  0}},
    // 10 blocks height opposed triangle
    { 20, 2, true,  {10,  7,  4,  2,  0,  0,  2,  4,  7, 10}}
};

uint KSBoard::normalTime() const
{
    if ( !isArcade() ) return Board::normalTime();
    return cfactory->cbi.baseTime / (1 + stageData().speed);
}

void KSBoard::arcadePrepare()
{
    if ( !stageData().hasPattern ) return;

    for (uint i=0; i<Width; i++) {
        Q_ASSERT( stageData().pattern[i]<=matrix().height() );
        for (uint j=0; j<stageData().pattern[i]; j++)
            setGarbageBlock(Coord(i, j));
    }
    main->update();
}


