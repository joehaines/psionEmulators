#include "gtetris.h"

#include "piece.h"


using namespace KGrid2D;

GenericTetris::GenericTetris(uint width, uint height, bool withPieces,
                             bool graphic)
    : _nextPiece(0), _currentPiece(0), _score(0), _nbRemoved(0),
      _nbClearLines(height), _main(0),
      _graphic(graphic), _matrix(width, height)
{
    if (withPieces) {
        _nextPiece = new Piece;
        _currentPiece = new Piece;
    }
    _matrix.fill(0);
}

void GenericTetris::copy(const GenericTetris &g)
{
    Q_ASSERT(_currentPiece);
    // copy to non graphic
    _score         = g._score;
    _level         = g._level;
    _nbRemoved     = g._nbRemoved;
    _nbClearLines  = g._nbClearLines;
    _currentPos    = g._currentPos;
    _nextPiece->copy(g._nextPiece);
    _currentPiece->copy(g._currentPiece);
    for (uint i=0; i<_matrix.size(); i++) {
        Coord c = _matrix.coord(i);
        delete _matrix[c];
        if ( g._matrix[c] ) _matrix[c] = new Block(g._matrix[c]->value());
        else _matrix[c] = 0;
    }
}

void GenericTetris::clear()
{
    _currentPos = Coord(0, -1);
    for (uint i=0; i<_matrix.size(); i++) removeBlock(_matrix.coord(i));
    computeInfos();
}

GenericTetris::~GenericTetris()
{
    // everything should already be done by setBlockInfo(0, 0);
}

void GenericTetris::setBlockInfo(BlockInfo *main, BlockInfo *next)
{
    Q_ASSERT( _graphic );
    if (main) {
        _main = main;
        if (_currentPiece) {
            Q_ASSERT(next);
            _nextPiece->setBlockInfo(next);
            _currentPiece->setBlockInfo(main);
        }
    } else { // before destruction
        clear();
        delete _currentPiece;
        delete _nextPiece;
    }
}

void GenericTetris::start(const GTInitData &data)
{
    Q_ASSERT( _graphic );
    _random.setSeed(data.seed);
    _initLevel = data.initLevel;
    updateScore(0);
    updateLevel(_initLevel);
    updateRemoved(0);
    clear();
    if (_nextPiece) {
        _nextPiece->setRandomSequence(&_random);
        _nextPiece->generateNext();
        newPiece();
    }
}

void GenericTetris::dropDown()
{
    uint dropHeight = moveTo(Coord(0, -_currentPos.second));
    pieceDropped(dropHeight);
}

void GenericTetris::oneLineDown()
{
    if ( moveTo(Coord(0, -1))==0 ) pieceDropped(0);
}

bool GenericTetris::newPiece()
{
    Q_ASSERT(_currentPiece);
    Coord min = _nextPiece->min();
    _currentPos.second = _matrix.height() - 1 + min.second;
    _currentPos.first  = (_matrix.width() - _nextPiece->size().first)/2
                         - min.first;
    if ( !canPosition(_currentPos, _nextPiece)) {
        _currentPos.second = -1;
        return false;
    }
    _currentPiece->copy(_nextPiece);
    if (_graphic) {
        _currentPiece->move(toPoint(_currentPos));
        _currentPiece->show(true);
        updatePieceConfig();
    }
    _nextPiece->generateNext();
    if (_graphic) {
        _nextPiece->moveCenter();
        _nextPiece->show(true);
        updateNextPiece();
    }
    return true;
}

bool GenericTetris::canPosition(const Coord &pos, const Piece *piece) const
{
    for(uint k=0; k<piece->nbBlocks(); k++) {
        Coord c(piece->pos(k, pos));
        if ( !_matrix.inside(c) || _matrix[c]!=0 )
            return false; // outside or something in the way
    }
    return true;
}

uint GenericTetris::moveTo(const Coord &dec)
{
    Q_ASSERT(_currentPiece);
    Q_ASSERT(dec.first==0 || dec.second==0);

    Coord newPos = _currentPos;
    Coord d(0, 0);
    uint n, i;

    if (dec.first) {
        d.first = (dec.first<0 ? -1 : 1);
        n = kAbs(dec.first);
    } else {
        d.second = (dec.second<0 ? -1 : 1);
        n = kAbs(dec.second);
    }

    for (i=0; i<n; i++) {
        if ( !canPosition(newPos + d, _currentPiece) ) break;
        newPos = newPos + d;
    }
    if ( i!=0 ) { // piece can be moved
        _currentPos = newPos;
        if (_graphic) {
            _currentPiece->move(toPoint(newPos));
            updatePieceConfig();
        }
    }
    return i;
}

bool GenericTetris::rotate(bool left)
{
    Q_ASSERT(_currentPiece);

    Piece tmp;
    tmp.copy(_currentPiece);
    QPoint p(0, 0);
    tmp.rotate(left, p);
    if ( canPosition(_currentPos, &tmp) ) {
        if (_graphic) p = toPoint(_currentPos);
        _currentPiece->rotate(left, p);
        if (_graphic) updatePieceConfig();
        return true;
    }
    return false;
}

void GenericTetris::computeInfos()
{
    _nbClearLines = 0;
    for (uint j=_matrix.height(); j>0; j--) {
        for (uint i=0; i<_matrix.width(); i++)
            if ( _matrix[Coord(i, j-1)]!=0 ) return;
        _nbClearLines++;
    }
}

void GenericTetris::setBlock(const Coord &c, Block *b)
{
	Q_ASSERT( b && _matrix[c]==0 );
	_matrix[c] = b;
	if (_graphic) {
        QPoint p = toPoint(c);
        b->sprite()->move(p.x(), p.y());
    }
}

void GenericTetris::removeBlock(const Coord &c)
{
	delete _matrix[c];
	_matrix[c] = 0;
}

void GenericTetris::moveBlock(const Coord &src, const Coord &dest)
{
	Q_ASSERT( _matrix[dest]==0 );
	if ( _matrix[src] ) {
		setBlock(dest, _matrix[src]);
		_matrix[src] = 0;
	}
}

QPoint GenericTetris::toPoint(const Coord &c) const
{
	return _main->toPoint(Coord(c.first, _matrix.height() - 1 - c.second));
}

void GenericTetris::gluePiece()
{
    Q_ASSERT(_currentPiece);

	for(uint k=0; k<_currentPiece->nbBlocks(); k++)
		setBlock(_currentPiece->pos(k, _currentPos),
                 _currentPiece->takeBlock(k));
	computeInfos();
}

void GenericTetris::bumpCurrentPiece(int dec)
{
	Q_ASSERT( _graphic && _currentPiece );
	_currentPiece->move(toPoint(_currentPos) + QPoint(0, dec));
}

void GenericTetris::partialMoveBlock(const Coord &c, const QPoint &dec)
{
	Q_ASSERT( _graphic && _matrix[c]!=0 );
    QPoint p = toPoint(c) + dec;
	_matrix[c]->sprite()->move(p.x(), p.y());
}
