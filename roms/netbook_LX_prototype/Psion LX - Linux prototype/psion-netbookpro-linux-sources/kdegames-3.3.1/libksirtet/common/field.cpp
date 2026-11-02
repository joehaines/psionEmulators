#include "field.h"
#include "field.moc"

#include <qwhatsthis.h>
#include <qlabel.h>
#include <qlayout.h>

#include <klocale.h>
#include <kprogress.h>
#include <kgameprogress.h>
#include <kcanvasrootpixmap.h>
#include <kgamelcd.h>

#include "base/baseprefs.h"
#include "factory.h"
#include "highscores.h"
#include "misc_ui.h"
#include "board.h"
#include "commonprefs.h"


Field::Field(QWidget *parent)
: MPSimpleBoard(parent), BaseField(this)
{
// column 1
    // score LCD
    scoreList = new KGameLCDList(this);
    showScore = new KGameLCD(6, scoreList);
    scoreList->append(showScore);
    showTime = new KGameLCDClock(scoreList);
    scoreList->append(showTime);
    lcds->addWidget(scoreList, 1, 0);
    lcds->setRowStretch(2, 1);

    // removed LCD
    removedList = new KGameLCDList(i18n(bfactory->mainData.removedLabel), this);
    lcds->addWidget(removedList, 3, 0);
    lcds->setRowStretch(4, 1);

    // level progress
    levelLabel = new QLabel(this);
    levelLabel->setAlignment(AlignCenter);
    lcds->addWidget(levelLabel, 5, 0);
    toLevel = new KProgress(this);
    toLevel->setTextEnabled(true);
    toLevel->setFormat("1");
    QWhatsThis::add(toLevel, i18n("Display the progress to complete the current level or stage."));
    lcds->addWidget(toLevel, 6, 0);
    lcds->setRowStretch(7, 1);

// column 2
    // previous player height
    prevHeight = new PlayerProgress(board, this, "prev_progress");
    QWhatsThis::add(prevHeight, i18n("Previous player's height"));
    top->addWidget(prevHeight, 1, 1, AlignHCenter);

// column 3
    // pending gift shower
    Board *b = static_cast<Board *>(board);
    top->addWidget(b->giftPool(), 0, 2, AlignCenter);

    // shadow piece
    shadow = new Shadow(board, this);
    QWhatsThis::add(shadow, i18n("Shadow of the current piece"));
    top->addWidget(shadow, 2, 2);

// column 4
    // next player height
    nextHeight = new PlayerProgress(board, this, "next_progress");
    QWhatsThis::add(nextHeight, i18n("Next player's height"));
    top->addWidget(nextHeight, 1, 3, AlignHCenter);

// column 5
    // next piece shower
    QVBoxLayout *vbl = new QVBoxLayout(10);
    top->addLayout(vbl, 1, 4);
    vbl->addStretch(1);

    labShowNext = new QLabel(i18n("Next Tile"), this);
    labShowNext->setAlignment(AlignCenter);
    vbl->addWidget(labShowNext, 0);
    showNext = new ShowNextPiece(board, this);
    _snRootPixmap = new KCanvasRootPixmap(showNext);
    _snRootPixmap->start();
    vbl->addWidget(showNext, 0);
    vbl->addStretch(4);

    connect(board, SIGNAL(scoreUpdated()), SLOT(scoreUpdatedSlot()));
    connect(board, SIGNAL(levelUpdated()), SLOT(levelUpdated()));
    connect(board, SIGNAL(removedUpdated()), SLOT(removedUpdated()));

    initVariableGUI();
}

void Field::levelUpdated()
{
    toLevel->setFormat(QString::number(board->level()));
    // necessary to update string ...
    int p = toLevel->progress();
    toLevel->setProgress(p+1);
    toLevel->setProgress(p);
}

void Field::removedUpdated()
{
    uint nb = cfactory->cbi.nbRemovedToLevel;
    toLevel->setProgress(isArcade() ? board->arcadeDone()
                         : board->nbRemoved() % nb);
}

void Field::showOpponents(bool show)
{
    Board *b = static_cast<Board *>(board);
    if (show) {
        prevHeight->show();
        nextHeight->show();
        b->giftPool()->show();
    } else {
        prevHeight->hide();
        nextHeight->hide();
        b->giftPool()->hide();
    }
}

void Field::settingsChanged()
{
    BaseField::settingsChanged();
    QColor color = BasePrefs::fadeColor();
    double s = BasePrefs::fadeIntensity();
    _snRootPixmap->setFadeEffect(s, color);
    showNext->canvas()->setBackgroundColor(color);
    bool b = CommonPrefs::showNextPiece();
    if (b) {
       showNext->show();
       labShowNext->show();
   } else {
       showNext->hide();
       labShowNext->hide();
   }
    b = CommonPrefs::showPieceShadow();
    if (b) shadow->show();
    else shadow->hide();
}

void Field::_init(bool AI, bool multiplayer, bool server, bool first,
                  const QString &name)
{
    BaseField::init(AI, multiplayer, server, first, name);
    showOpponents(multiplayer);
    static_cast<Board *>(board)->setType(AI);
}

void Field::_initFlag(QDataStream &s)
{
    ServerInitData sid;
    s >> sid;
    GTInitData data;
    data.seed = sid.seed;
    data.initLevel = sid.initLevel;

    shadow->setDisplay(true);
    toLevel->setValue(0);
    showTime->reset();
    showTime->start();

    BaseField::start(data);
    initVariableGUI();
}

void Field::initVariableGUI()
{
    if ( board->isArcade() ) {
        scoreList->title()->setText(i18n("Elapsed time"));
        showScore->hide();
        showTime->show();
        QWhatsThis::add(scoreList, i18n("Display the elapsed time."));
        levelLabel->setText(i18n("Stage"));
        toLevel->setTotalSteps( board->arcadeTodo() );
    } else {
        scoreList->title()->setText(i18n("Score"));
        showScore->show();
        showTime->hide();
        QWhatsThis::add(scoreList, i18n("<qt>Display the current score.<br/>It turns <font color=\"blue\">blue</font> if it is a highscore and <font color=\"red\">red</font> if it is the best local score.</qt>"));
        levelLabel->setText(i18n("Level"));
        toLevel->setTotalSteps(cfactory->cbi.nbRemovedToLevel);
    }
}

void Field::_playFlag(QDataStream &s)
{
    ServerPlayData spd;
    s >> spd;
    prevHeight->setValue(spd.prevHeight);
    nextHeight->setValue(spd.nextHeight);
    if (spd.gift)
        static_cast<Board *>(board)->giftPool()->put(spd.gift);
}

void Field::_pauseFlag(bool p)
{
    pause(p);
    shadow->setDisplay(!p);
    if (p) showTime->stop();
    else showTime->start();
}

void Field::_stopFlag(bool gameover)
{
    BaseField::stop(gameover);
    showTime->stop();
}

void Field::_dataOut(QDataStream &s)
{
    _cpd.height = board->firstClearLine();
    _cpd.end    = static_cast<Board *>(board)->isGameOver();
    _cpd.gift   = static_cast<Board *>(board)->gift();
    s << _cpd;
}

KExtHighscore::Score Field::currentScore() const
{
    KExtHighscore::Score score(_cpd.end ? KExtHighscore::Lost : KExtHighscore::Won);
    score.setScore(board->score());
    score.setData("level", board->level());
    score.setData("removed", board->nbRemoved());
    return score;
}

void Field::_gameOverDataOut(QDataStream &s)
{
    s << currentScore();
}

void Field::moveLeft() { static_cast<Board *>(board)->pMoveLeft(); }
void Field::moveRight() { static_cast<Board *>(board)->pMoveRight(); }
void Field::dropDownStart() { static_cast<Board *>(board)->pDropDownStart(); }
void Field::dropDownStop() { static_cast<Board *>(board)->pDropDownStop(); }
void Field::oneLineDown() { static_cast<Board *>(board)->pOneLineDown(); }
void Field::rotateLeft() { static_cast<Board *>(board)->pRotateLeft(); }
void Field::rotateRight() { static_cast<Board *>(board)->pRotateRight(); }
void Field::moveLeftTotal() { static_cast<Board *>(board)->pMoveLeftTotal(); }
void Field::moveRightTotal() { static_cast<Board *>(board)->pMoveRightTotal();}
