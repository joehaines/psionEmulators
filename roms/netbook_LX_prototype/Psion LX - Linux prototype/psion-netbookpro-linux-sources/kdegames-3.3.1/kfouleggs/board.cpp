#include "board.h"
#include "board.moc"

#include <kglobal.h>

#include "common/misc_ui.h"
#include "piece.h"


using namespace KGrid2D;

FEBoard::FEBoard(bool graphic, QWidget *parent)
: Board(graphic, new GiftPool(parent), parent),
  _field(matrix().width(), matrix().height()), _chainedPuyos(4), _nbPuyos(0)
{
  for (uint i=0; i<_chainedPuyos.count(); i++)
    _chainedPuyos[i] = 0;
}

void FEBoard::copy(const GenericTetris &g)
{
    Board::copy(g);
    _nbPuyos = static_cast<const FEBoard &>(g)._nbPuyos;
}

void FEBoard::start(const GTInitData &data)
{
    _nbPuyos  = 0;
    _chained  = 0;
    _giftRest = 0;
    for (uint i=0; i<4; i++) {
        _chainedPuyos[i] = 0;
        emit removedUpdated(i, 0);
    }
    Board::start(data);
}

void FEBoard::computeInfos()
{
    Board::computeInfos();
    if ( graphic() ) computeNeighbours();
}

bool FEBoard::afterGlue(bool doAll, bool first)
{
    return !doFall(doAll, first, false);
}

void FEBoard::removeBlock(const Coord &c)
{
    Board::removeBlock(c);

    // remove surrounding garbage
    CoordList list = matrix().neighbours(c, true, true);
    for (CoordList::const_iterator i = list.begin(); i!=list.end(); ++i)
		if ( matrix()[*i]!=0 && matrix()[*i]->isGarbage() )
            Board::removeBlock(*i);
}

bool FEBoard::toBeRemoved(const Coord &c) const
{
    return ( _field[c]>=4 );
}

bool FEBoard::toFall(const Coord &c) const
{
    Coord under = c - Coord(0, 1);
    return ( matrix()[under]==0 );
}

void FEBoard::remove()
{
    Board::remove();

    // score calculation from another game
	// not sure it is the "official" way
	uint nbPuyos = _groups.size(); // number of group detroyed
	uint nbEggs = 0;               // number of eggs destroyed
	for (uint k=0; k<_groups.size(); k++) nbEggs += _groups[k];

	uint bonus = nbEggs - 3; // more than 4 since we are here !
	if ( nbEggs==11   ) bonus += 2;
	if ( nbPuyos>=2   ) bonus += 3 * (1 << (nbPuyos-2)); // 3 * 2^(nb-2)
	if ( _chained>=1  ) bonus += 1 << (_chained+2);      // 2^(chained+2)
	uint dscore = 10 * nbPuyos * bonus;

    uint c = kMin(_chained, (uint)3);
    _chainedPuyos[c] += nbPuyos;
	_chained++;
	_giftRest += dscore;
	_nbPuyos += nbPuyos;
	updateRemoved(nbRemoved() + nbEggs);
	updateScore(score() + dscore);
    emit removedUpdated(c, _chainedPuyos[c]);

    updateLevel();
}

Board::AfterRemoveResult FEBoard::afterRemove(bool doAll, bool first)
{
    Board::AfterRemoveResult res = Board::afterRemove(doAll, first);
	if ( res==Done && needRemoving() ) return NeedRemoving;
	return res;
}

bool FEBoard::needRemoving()
{
    _groups = findGroups(_field, 4);
    if ( _groups.size()==0 ) _chained = 0;
	return _groups.size();
}

/*****************************************************************************/
// Multiplayers methods
uint FEBoard::gift()
{
	uint n   = _giftRest / 60;
	_giftRest = _giftRest % 60;
	return n;
}

bool FEBoard::putGift(uint n)
{
	QMemArray<bool> free(matrix().width());

	// garbage blocks are put randomly on conlumns with more than 5 free lines.
	uint nbFree = 0;
	for (uint i=0; i<free.size(); i++) {
		int f = firstColumnBlock(i);
		if ( f==-1 || f>=(int)matrix().height()-5 ) free[i] = false;
		else {
			free[i] = true;
			nbFree++;
		}
	}
	uint nb = kMin(nbFree, n);
	while (nbFree && nb) {
		uint k = (uint)randomGarbage.getLong(nbFree);
		uint l = 0;
		for (uint i=0; i<free.size(); i++) {
			if ( free[i]==false ) continue;
			if ( k==l ) {
				Block *gb = currentPiece()->garbageBlock();
				gb->sprite()->show();
                Coord c(i, matrix().height()-1);
				setBlock(c, gb);
				free[i] = false;
				nbFree--;
				nb--;
				break;
			}
			l++;
		}
	}
	return true;
}
