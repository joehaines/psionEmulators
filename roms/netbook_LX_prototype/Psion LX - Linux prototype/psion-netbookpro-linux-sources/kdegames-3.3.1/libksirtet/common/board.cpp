#include "board.h"
#include "board.moc"

#include <knotifyclient.h>
#include <klocale.h>

#include "factory.h"
#include "base/piece.h"
#include "misc_ui.h"
#include "ai.h"
#include "commonprefs.h"


Board::Board(bool graphic, GiftPool *gp, QWidget *parent)
    : BaseBoard(graphic, parent),
      _giftPool(gp), aiEngine(0) 
{}

Board::~Board()
{
    delete aiEngine;
}

void Board::setType(bool _ai)
{
    Q_ASSERT( graphic() );
    if (_ai) {
        if ( aiEngine==0 ) aiEngine = cfactory->createAI();
    } else {
        delete aiEngine;
        aiEngine = 0;
    }
}

void Board::start(const GTInitData &data)
{
    randomGarbage.setSeed(data.seed);
    _giftPool->reset();
    BaseBoard::start(data);
}

void Board::stop()
{
    BaseBoard::stop();
    if (aiEngine) aiEngine->stop();
}

void Board::showBoard(bool show)
{
    BaseBoard::showBoard(show);
    showCanvas(_next, show);
}

void Board::unpause()
{
    BaseBoard::unpause();
    if (aiEngine) aiEngine->start(); // eventually restart thinking
}

void Board::updateLevel()
{
    uint nb = cfactory->cbi.nbRemovedToLevel;
    if ( nbRemoved()>=level()*nb ) updateLevel(level()+1);
}

void Board::updateLevel(uint newLevel)
{
    BaseBoard::updateLevel(newLevel);
    emit levelUpdated();
    if ( graphic() ) startTimer();
}

void Board::settingsChanged()
{
    BaseBoard::settingsChanged();
    if (aiEngine) aiEngine->settingsChanged();
}

/*****************************************************************************/
void Board::pMoveLeft()
{
    if ( state!=Normal ) return;
    moveLeft();
    main->update();
}

void Board::pMoveRight()
{
    if ( state!=Normal ) return;
    moveRight();
    main->update();
}

void Board::pMoveLeftTotal()
{
    if ( state!=Normal ) return;
    moveLeft(bfactory->bbi.width);
    main->update();
}

void Board::pMoveRightTotal()
{
    if ( state!=Normal ) return;
    moveRight(bfactory->bbi.width);
    main->update();
}

void Board::pOneLineDown()
{
    if ( state!=Normal ) return;
    oneLineDown();
    main->update();
}

void Board::pDropDownStart()
{
    if ( state!=Normal ) return;
    _dropHeight = 0;
    oneLineDown();
    if ( state==Normal ) {
        state = DropDown;
        startTimer();
    }
    main->update();
}

void Board::pDropDownStop()
{
    if ( state!=DropDown || CommonPrefs::directDropDownEnabled() ) return;
    state = Normal;
    startTimer();
    main->update();
}

void Board::pRotateLeft()
{
    if ( state!=Normal ) return;
    rotateLeft();
    main->update();
}

void Board::pRotateRight()
{
    if ( state!=Normal ) return;
    rotateRight();
    main->update();
}

void Board::pieceDropped(uint dropHeight)
{
    if ( state==DropDown ) state = Normal;
    else _dropHeight = dropHeight;
    _beforeGlue(true);
}

void Board::_beforeGlue(bool first)
{
    if ( graphic() ) {
        state = (beforeGlue(_dropHeight>=1, first) ? BeforeGlue : Normal);
        if ( state==BeforeGlue ) {
            startTimer();
            return;
        }
    }
    gluePiece();
}

void Board::gluePiece()
{
    BaseBoard::gluePiece();
    _afterGlue(true);
    if ( graphic() ) KNotifyClient::event(winId(), "glued", i18n("Piece glued"));
}

void Board::_afterGlue(bool first)
{
    bool b = afterGlue(!graphic(), first);
    if ( graphic() ) {
        state = (b ? AfterGlue : Normal);
        if ( state==AfterGlue ) {
            startTimer();
            return;
        }
    }

    updateScore(score() + _dropHeight);
    if ( needRemoving() ) _beforeRemove(true);
    else _afterAfterRemove();
}

bool Board::afterAfterRemove()
{
    // checkGift
    if ( graphic() && _giftPool->pending() ) {
        if ( putGift(_giftPool->take()) ) {
            computeInfos();
            _afterGift(true);
            return true;
        } else return false;
    }
    return newPiece();
}

void Board::_afterGift(bool first)
{
    Q_ASSERT( graphic() );
    state = (afterGift(first) ? AfterGift : Normal);
    if ( state==AfterGift ) startTimer();
    else afterAfterRemove();
}

bool Board::newPiece()
{
    Q_ASSERT( !graphic() || state==Normal );
    if ( !BaseBoard::newPiece() ) return false;
    if ( graphic() ) {
        main->update();
        _next->update();
        if (aiEngine) aiEngine->launch(this);
        // else : a human player can think by himself ...
    }
    return true;
}

bool Board::timeout()
{
    if ( BaseBoard::timeout() ) return true;

    switch (state) {
    case DropDown:   _dropHeight++;
    case Normal:     oneLineDown();      break;
    case BeforeGlue: _beforeGlue(false); break;
    case AfterGlue:  _afterGlue(false);  break;
    case AfterGift:  _afterGift(false);  break;
    default:         return false;
    }
    main->update();
    return true;
}

uint Board::normalTime() const
{
    return cfactory->cbi.baseTime / (1 + level());
}

bool Board::startTimer()
{
    if ( BaseBoard::startTimer() ) return true;

    switch (state) {
    case Normal:
        timer.start(normalTime());
        break;
    case DropDown:
        timer.start(cfactory->cbi.dropDownTime);
        break;
    case BeforeGlue:
        timer.start(cfactory->cbi.beforeGlueTime, true);
            break;
    case AfterGlue:
        timer.start(cfactory->cbi.afterGlueTime, true);
        break;
    case AfterGift:
        timer.start(cfactory->cbi.afterGiftTime, true);
            break;
    default:
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
bool Board::beforeGlue(bool bump, bool first)
{
    if ( !bump ) return false;
    if (first) {
        loop = 0;
        return true;
    } else loop++;

    float dec = BasePrefs::blockSize() * (loop+1) * -0.1;
    if ( BasePrefs::animationsEnabled() ) bumpCurrentPiece((int)dec);

    return ( loop!=cfactory->cbi.nbBumpStages );
}
