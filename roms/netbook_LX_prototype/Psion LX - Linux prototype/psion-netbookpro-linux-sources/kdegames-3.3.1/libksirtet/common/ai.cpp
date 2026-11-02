#include "ai.h"
#include "ai.moc"

#include <assert.h>

#include <qlabel.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qlayout.h>
#include <qgrid.h>
#include <qwhatsthis.h>

#include <klocale.h>
#include <kconfig.h>
#include <kapplication.h>

#include "commonprefs.h"
#include "board.h"
#include "base/piece.h"
#include "base/factory.h"


//-----------------------------------------------------------------------------
AIPiece::AIPiece()
    : _current(0)
{}

AIPiece::~AIPiece()
{
    delete _current;
}

void AIPiece::init(const Piece *piece, Board *b)
{
    _piece = piece;
    _board = b;
    if ( _current==0 ) _current = new Piece;
    reset();
}

void AIPiece::reset()
{
    curPos = 0;
    curRot = 0;
    if (_piece) _current->copy(_piece);
//    else _current->generateNext(0);
    nbRot = _current->nbConfigurations() - 1;
    nbPos = _board->matrix().width() - _current->size().first + 1;
}

bool AIPiece::increment()
{
    curPos++;
    if ( curPos==nbPos ) {
        if ( curRot==nbRot ) {
//            if ( _piece || _current->type()==Piece::info().nbTypes() ) {
                reset();
                return false;
//            }
//            _current->generateNext(_current->type()+1);
//            nbRot = _current->nbConfigurations() - 1;
//            curRot = 0;
        }
        _current->rotate(true, QPoint(0, 0));
        nbPos = _board->matrix().width() - _current->size().first + 1;
        curRot++;
        curPos = 0;
    }
    return true;
}

bool AIPiece::place()
{
    if ( curRot==3 ) {
        if ( !_board->rotateRight() ) return false;
    } else for (uint i=0; i<curRot; i++)
        if ( !_board->rotateLeft() ) return false;
    curDec = curPos - _board->currentPos().first - _current->min().first;
    if ( curDec!=0 && _board->moveRight(curDec)!=(uint)kAbs(curDec) )
        return false;
    _board->dropDown();
    return !_board->isGameOver();
}

//-----------------------------------------------------------------------------
const AI::Data AI::LastData = { 0, 0, 0, false, 0 };

AI::AI(uint tTime, uint oTime, const Data *DATA)
    : timer(this), thinkTime(tTime), orderTime(oTime), stopped(false),
      board(0)
{
    connect(&timer, SIGNAL(timeout()), SLOT(timeout()));

    for (uint i=0; DATA[i].name; i++) {
        Element element;
        element.data = &DATA[i];
        _elements.append(element);
    }
    settingsChanged();
}

void AI::resizePieces(uint size)
{
    uint oldSize = pieces.size();
    for (uint i=size; i<oldSize; i++) delete pieces[i];
    pieces.resize(size);
    for (uint i=oldSize; i<size; i++) pieces[i] = new AIPiece;
}

AI::~AI()
{
    delete board;
    resizePieces(0);
}

void AI::initThink()
{
    board->copy(*main);
}

void AI::launch(Board *m)
{
    main = m;
    if ( board==0 )
        board = static_cast<Board *>(bfactory->createBoard(false, 0));

    pieces[0]->init(main->currentPiece(), board); // current
    if ( pieces.size()>=2 ) pieces[1]->init(main->nextPiece(), board); // next

    state = Thinking;
    hasBestPoints = false;
    startTimer();
}

void AI::stop()
{
    timer.stop();
    stopped = true;
}

void AI::start()
{
    if (stopped) {
        startTimer();
        stopped = false;
    }
}

void AI::startTimer()
{
    switch (state) {
    case Thinking:     timer.start(thinkTime, true); break;
    case GivingOrders: timer.start(orderTime, true); break;
    }
}

void AI::timeout()
{
    switch (state) {
    case Thinking:
        if ( think() ) state = GivingOrders;
        break;
    case GivingOrders:
        if ( emitOrder() ) return;
        break;
    }

    startTimer();
}

bool AI::emitOrder()
{
    if ( bestRot==3 ) {
        bestRot = 0;
        main->pRotateRight();
    } else if (bestRot) {
        bestRot--;
        main->pRotateLeft();
    } else if ( bestDec>0 ) {
        bestDec--;
        main->pMoveRight();
    } else if ( bestDec<0 ) {
        bestDec++;
        main->pMoveLeft();
    } else {
        main->pDropDownStart();
        return true;
    }
    return false;
}

bool AI::think()
{
    initThink();
    bool moveOk = true;
    for (uint i=0; i<pieces.size(); i++)
        if ( !pieces[i]->place() ) {
            moveOk = false;
            break;
        }
    if (moveOk) {
        double p = points();
        if ( !hasBestPoints || p>bestPoints
             || (p==hasBestPoints && random.getBool()) ) {
            hasBestPoints = true;
            bestPoints = p;
            bestDec = pieces[0]->dec();
            bestRot = pieces[0]->rot();
        }
    }

    for (uint i=pieces.size(); i>0; i--)
        if ( pieces[i-1]->increment() ) return false;
    return true;
}

double AI::points() const
{
    double pts = 0;
    for (uint i=0; i<_elements.size(); i++) {
        if ( _elements[i].coefficient==0.0 ) continue;
        double v = _elements[i].data->function(*main, *board);
        if ( _elements[i].data->triggered && qRound(v)<_elements[i].trigger )
            continue;
        pts += _elements[i].coefficient * v;
    }
    return pts;
}

void AI::settingsChanged()
{
    int d = CommonPrefs::thinkingDepth();
    resizePieces(d);
    for (uint i=0; i<_elements.size(); i++) {
        const Data &data = *_elements[i].data;
        _elements[i].coefficient = AIConfig::coefficient(data);
        if (data.triggered) _elements[i].trigger = AIConfig::trigger(data);
    }
    if ( timer.isActive() ) launch(main);
}

double AI::nbOccupiedLines(const Board &, const Board &current)
{
    return current.matrix().height() - current.nbClearLines();
}

double AI::nbHoles(const Board &, const Board &current)
{
    uint nb = 0;
    for (uint i=0; i<current.matrix().width(); i++) {
        for (int j=current.firstColumnBlock(i)-1; j>=0; j--) {
            KGrid2D::Coord c(i, j);
            if ( current.matrix()[c]==0 ) nb++;
        }
    }
    return nb;
}

double AI::peakToPeak(const Board &, const Board &current)
{
    int min = current.matrix().height()-1;
    for (uint i=0; i<current.matrix().width(); i++)
        min = kMin(min, current.firstColumnBlock(i));
    return (int)current.firstClearLine()-1 - min;
}

double AI::mean(const Board &, const Board &current)
{
    double mean = 0;
    for (uint i=0; i<current.matrix().width(); i++)
        mean += current.firstColumnBlock(i);
    return mean / current.matrix().width();
}

double AI::nbSpaces(const Board &main, const Board &current)
{
    double nb = 0;
    double m = mean(main, current);
    for (uint i=0; i<current.matrix().width(); i++) {
        int j = current.firstColumnBlock(i);
        if ( j<m ) nb += m - j;
    }
    return nb;
}

double AI::nbRemoved(const Board &main, const Board &current)
{
    return current.nbRemoved() - main.nbRemoved();
}


//-----------------------------------------------------------------------------
const uint AIConfig::minThinkingDepth = 1;
const uint AIConfig::maxThinkingDepth = 2;

AIConfig::AIConfig(const QValueVector<AI::Element> &elements)
    : QWidget(0, "ai config")
{
    QGridLayout *top = new QGridLayout(this, 3, 2, KDialog::marginHint(),
                                       KDialog::spacingHint());

    QLabel *label = new QLabel(i18n("Thinking depth:"), this);
    top->addWidget(label, 0, 0);
    KIntNumInput *in = new KIntNumInput(this, "kcfg_ThinkingDepth");
    in->setRange(minThinkingDepth, maxThinkingDepth);
    top->addWidget(in, 0, 1);

    top->addRowSpacing(1, KDialog::spacingHint());

    QGrid *grid = new QGrid(2, this);
    top->addMultiCellWidget(grid, 2, 2, 0, 1);
    for (uint i=0; i<elements.size(); i++) {
        const AI::Data &data = *elements.at(i).data;
        QLabel *label = new QLabel(i18n(data.label), grid);
        if (data.whatsthis) QWhatsThis::add(label, i18n(data.whatsthis));
        label->setFrameStyle(QFrame::Panel | QFrame::Plain);

        QVBox *vb = new QVBox(grid);
        if (data.whatsthis) QWhatsThis::add(vb, i18n(data.whatsthis));
        vb->setMargin(KDialog::spacingHint());
        vb->setSpacing(KDialog::spacingHint());
        vb->setFrameStyle(QFrame::Panel | QFrame::Plain);
        if (data.triggered) {
            KIntNumInput *trig = new KIntNumInput(vb, triggerKey(data.name));
            trig->setRange(0, 10, 1, true);
        }
        KDoubleNumInput *coeff = new KDoubleNumInput(vb, coefficientKey(data.name));
        coeff->setRange(0.0, 1.0, 1.0, true);
    }
}

QCString AIConfig::triggerKey(const char *name)
{
    return "kcfg_Trigger_" + QCString(name);
}

QCString AIConfig::coefficientKey(const char *name)
{
    return "kcfg_Coefficient_" + QCString(name);
}

double AIConfig::coefficient(const AI::Data &data)
{
    KConfigSkeletonItem *item = CommonPrefs::self()->findItem( QString("Coefficient_%1").arg(data.name) );
    assert(item);
    return item->property().toDouble();
}

int AIConfig::trigger(const AI::Data &data)
{
    KConfigSkeletonItem *item = CommonPrefs::self()->findItem( QString("Trigger_%1").arg(data.name) );
    assert(item);
    return item->property().toInt();
}
