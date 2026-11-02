#include "board.h"
#include "board.moc"

#include <knotifyclient.h>
#include <klocale.h>
#include <kzoommainwindow.h>

#include "piece.h"
#include "factory.h"
#include "baseprefs.h"

using namespace KGrid2D;

//-----------------------------------------------------------------------------
FixedCanvasView::FixedCanvasView(QWidget *parent, const char *name)
  : QCanvasView(parent, name, WNoAutoErase)
{}

QSize FixedCanvasView::sizeHint() const
{
  if ( canvas()==0 ) return QSize();
  return canvas()->size() + 2 * QSize(frameWidth(), frameWidth());
}

void FixedCanvasView::adjustSize()
{
  setFixedSize(sizeHint());
}

//-----------------------------------------------------------------------------
const BaseBoard::DirectionData BaseBoard::DIRECTION_DATA[Nb_Direction] = {
    { SquareBase::Left,  Left },
    { SquareBase::Right, Right },
    { SquareBase::Down,  Up },
    { SquareBase::Up,    Down }
};

BaseBoard::BaseBoard(bool graphic, QWidget *parent)
: FixedCanvasView(parent, "board"),
  GenericTetris(bfactory->bbi.width, bfactory->bbi.height,
                bfactory->bbi.withPieces, graphic),
  state(GameOver), timer(this), sequences(0), main(0), _next(0),
 _arcade(false)
{
    if (graphic) {
        setVScrollBarMode(AlwaysOff);
        setHScrollBarMode(AlwaysOff);
        setFrameStyle( QFrame::Panel | QFrame::Sunken );

        sequences = new SequenceArray;
        main = new BlockInfo(*sequences);
        setCanvas(main);
        if (bfactory->bbi.withPieces)
          _next = new BlockInfo(*sequences);
        setBlockInfo(main, _next);
        
        connect(&timer, SIGNAL(timeout()), SLOT(timeout()));
        
        Piece::info().loadColors();
        KZoomMainWindow::addWidget(this);
    }
}

void BaseBoard::copy(const GenericTetris &g)
{
    GenericTetris::copy(g);
    state = static_cast<const BaseBoard &>(g).state;
}

void BaseBoard::settingsChanged()
{
    Q_ASSERT( graphic() );
    Piece::info().loadColors();
}

void BaseBoard::adjustSize()
{
    int size = BasePrefs::blockSize();
    
    sequences->setBlockSize(size);
    main->resize(matrix().width() * size, matrix().height() * size);
    for (uint i=0; i<matrix().width(); i++)
        for (uint j=0; j<firstClearLine(); j++) {
            Coord c(i, j);
            if ( matrix()[c]==0 ) continue;
            partialMoveBlock(c, QPoint(0, 0));
        }
    
    if (_next) {
      Coord c = Piece::info().maxSize() + Coord(2, 2);
      _next->resize(c.first * size, c.second * size);
      _nextPiece->moveCenter();
    }
    
    FixedCanvasView::adjustSize();
}

BaseBoard::~BaseBoard()
{
    if ( graphic() ) {
        setBlockInfo(0, 0); // destruct all sprites before deleting canvas
        delete _next;
        delete main;
        delete sequences;
    }
}

void BaseBoard::init(bool arcade)
{
    _arcade = arcade;
    _arcadeStageDone = false;
}

void BaseBoard::start(const GTInitData &data)
{
	Q_ASSERT( graphic() );
    if ( !_arcadeStageDone || _arcadeStage==bfactory->bbi.nbArcadeStages )
        _arcadeStage = 0;
    _arcadeStageDone = false;
	state = Normal;
	GenericTetris::start(data); // NB: the timer is started by updateLevel !
    if (_arcade) arcadePrepare();
}

void BaseBoard::stop()
{
	timer.stop();
    state = GameOver;
}

void BaseBoard::pause()
{
	Q_ASSERT( graphic() );
    timer.stop();
    _oldState = state;
    state = Paused;
	showBoard(false);
}

void BaseBoard::gameOver()
{
    stop();
    emit gameOverSignal();
}

void BaseBoard::showCanvas(QCanvas *c, bool show)
{
	QCanvasItemList l = c->allItems();
	QCanvasItemList::Iterator it;
	for (it=l.begin(); it!=l.end(); ++it) {
		if (show) (*it)->show();
		else (*it)->hide();
	}
	c->update();
}

void BaseBoard::showBoard(bool show)
{
	showCanvas(main, show);
}

void BaseBoard::unpause()
{
	Q_ASSERT( graphic() );
	showBoard(true);
    state = _oldState;
	startTimer();
}

void BaseBoard::updateRemoved(uint newRemoved)
{
	GenericTetris::updateRemoved(newRemoved);
    emit removedUpdated();
}

void BaseBoard::updateScore(uint newScore)
{
	GenericTetris::updateScore(newScore);
    emit scoreUpdated();
}

int BaseBoard::firstColumnBlock(uint col) const
{
	for (int j=firstClearLine()-1; j>=0; j--) {
        Coord c(col, j);
		if ( matrix()[c]!=0 ) return j;
    }
	return -1;
}

//-----------------------------------------------------------------------------
void BaseBoard::_beforeRemove(bool first)
{
	if ( graphic() ) {
		state = ( beforeRemove(first) ? BeforeRemove : Normal );
		if ( state==BeforeRemove ) {
			startTimer();
			return;
		}
	}
	remove();
	_afterRemove(true);
}

void BaseBoard::remove()
{
    for (uint j=0; j<firstClearLine(); j++)
        for (uint i=0; i<matrix().width(); i++) {
            Coord c(i, j);
            if ( matrix()[c]==0 || !toBeRemoved(c) ) continue;
            removeBlock(c);
        }
    computeInfos();
    if ( graphic() ) {
        main->update();
        KNotifyClient::event(winId(), "removed", i18n("Blocks removed"));
    }
}

bool BaseBoard::doFall(bool doAll, bool first, bool lineByLine)
{
    Q_ASSERT( !lineByLine || !doAll );

    if ( !doAll ) {
		if (first) loop = 0;
		else loop++;
    }
	bool final = (doAll || lineByLine
                  || loop==bfactory->bbi.nbFallStages);

    for (uint i=0; i<matrix().width(); i++) {
        // compute heights
        // we must separate this computation since toFall() can depend
        // directly on the disposition of blocks under the current one
        // (for e.g. in kfouleggs)
        // we do not rely on firstClearLine() here since this method is
        // used in kfouleggs to  make gift blocks fall down ...
        uint h = 0;
        QMemArray<uint> heights(matrix().height());
        for (uint j=1; j<matrix().height(); j++) { // first line cannot fall
            Coord src(i, j);
            if ( toFall(src) ) h++;
            heights[j] = h;
        }

        // do move
        for (uint j=1; j<matrix().height(); j++) {
            Coord src(i, j);
            if( heights[j]==0 || matrix()[src]==0 ) continue;
            if (lineByLine) final = false;
            uint k = j - (lineByLine ? 1 : heights[j]);
            Coord dest(i, k);
            if ( final || lineByLine ) moveBlock(src, dest);
            else partialBlockFall(src, dest);
        }
    }

    if (final) computeInfos();
    return final;
}

void BaseBoard::_afterRemove(bool first)
{
	AfterRemoveResult r = afterRemove(!graphic(), first);
	switch (r) {
	  case Done:
          state = Normal;
          _afterAfterRemove();
          return;
	  case NeedAfterRemove:
          state = AfterRemove;
          startTimer();
          return;
	  case NeedRemoving:
          _beforeRemove(true);
          return;
	}
}

BaseBoard::AfterRemoveResult BaseBoard::afterRemove(bool doAll, bool first)
{
    return (doFall(doAll, first, false) ? Done : NeedAfterRemove);
}

void BaseBoard::_afterAfterRemove()
{
    if ( isArcade() && arcadeDone()>=arcadeTodo() ) {
        _arcadeStage++;
        _arcadeStageDone = true;
        gameOver();
        return;
    }
    if ( !afterAfterRemove() ) gameOver();
    else if ( graphic() ) startTimer();
}

bool BaseBoard::timeout()
{
    Q_ASSERT( graphic() );
    if ( state==GameOver ) return true;
	switch (state) {
        case BeforeRemove: _beforeRemove(FALSE); break;
        case AfterRemove:  _afterRemove(FALSE);  break;
        default:           return false;
	}
	main->update();
    return true;
}

bool BaseBoard::startTimer()
{
    Q_ASSERT( graphic() );
    if ( state==GameOver ) return true;
	switch (state) {
        case BeforeRemove:
            timer.start(bfactory->bbi.beforeRemoveTime, true);
            break;
        case AfterRemove:
            timer.start(bfactory->bbi.afterRemoveTime, true);
            break;
        default:
            return false;
	}
    return true;
}

bool BaseBoard::beforeRemove(bool first)
{
    if (first) loop = 0;
	else loop++;

	for (uint j=0; j<firstClearLine(); j++)
		for (uint i=0; i<matrix().width(); i++) {
            Coord c(i, j);
			if ( toBeRemoved(c) ) matrix()[c]->toggleLight();
        }

	return ( loop!=bfactory->bbi.nbToggles );
}


//-----------------------------------------------------------------------------
void BaseBoard::partialBlockFall(const Coord &src, const Coord &dest)
{
    Q_ASSERT( loop<bfactory->bbi.nbFallStages );

    float c = float(loop+1) / bfactory->bbi.nbFallStages * BasePrefs::blockSize();
    int xdec = dest.first - src.first;
    int ydec = src.second - dest.second;
    QPoint p(int(xdec * c), int(ydec * c));
    partialMoveBlock(src, p);
}

uint BaseBoard::findGroup(Square<int> &field, const Coord &c) const
{
    uint nb = 0;
    _findGroup(field, c, nb, false);
    return nb;
}

void BaseBoard::setGroup(Square<int> &field, const Coord &c, uint nb) const
{
    _findGroup(field, c, nb, true);
}

void BaseBoard::_findGroup(Square<int> &field, const Coord &c,
                           uint &nb, bool set) const
{
    if (!set) nb++;
	field[c] = (set ? (int)nb : -1);
    uint value = matrix()[c]->value();
    CoordList n = matrix().neighbours(c, true, true);
    for (CoordList::const_iterator i = n.begin(); i!=n.end(); ++i)
        blockInGroup(field, *i, value, nb, set);
}

void BaseBoard::blockInGroup(Square<int> &field, const Coord &c, uint value,
                             uint &nb, bool set) const
{
	if ( matrix()[c]==0 ) return;
	if ( matrix()[c]->value()!=value ) return;
	if ( field[c]!=(set ? -1 : 0) ) return;
	_findGroup(field, c, nb, set);
}

QMemArray<uint> BaseBoard::findGroups(Square<int> &field, uint minSize,
									  bool exitAtFirstFound) const
{
    field.fill(0);
    QMemArray<uint> groups;
 	for (uint j=0; j<firstClearLine(); j++)
		for (uint i=0; i<matrix().width(); i++) {
            Coord c(i, j);
			if ( matrix()[c]==0 || matrix()[c]->isGarbage() ) continue;
			if ( field[c]!=0 ) continue;
            uint nb = findGroup(field, c);
            setGroup(field, c, nb);
            if ( nb>=minSize ) {
				uint s = groups.size();
				groups.resize(s+1);
				groups[s] = nb;
                if (exitAtFirstFound) return groups;
			}
		}
    return groups;
}

uint BaseBoard::drawCode(const Coord &c) const
{
	uint v = matrix()[c]->value();
	uint code = 0;
    for (uint i=0; i<Nb_Direction; i++) {
        Coord nc = SquareBase::neighbour(c, DIRECTION_DATA[i].neighbour);
        if ( !matrix().inside(nc) || matrix()[nc]==0
            || matrix()[nc]->value()!=v ) continue;
        code |= DIRECTION_DATA[i].direction;
    }
	return code;
}

void BaseBoard::computeNeighbours()
{
	for (uint j=0; j<firstClearLine(); j++)
		for (uint i=0; i<matrix().width(); i++) {
            Coord c(i, j);
			if ( matrix()[c]==0 || matrix()[c]->isGarbage() ) continue;
			matrix()[c]->sprite()->setFrame( drawCode(c) );
		}
}
