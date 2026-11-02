#include "main.h"
#include "main.moc"

#include <klocale.h>
#include <kapplication.h>
#include <khighscore.h>

#include "common/inter.h"
#include "common/highscores.h"
#include "prefs.h"
#include "piece.h"

//-----------------------------------------------------------------------------
const MPGameInfo MP_GAME_INFO = {
    "004", // multiplayer id (increase when incompatible changes are made)
    4,     // max nb local games
    500,   // interval
    true,  // IA allowed
    0, 0   // no setting slots
};

const MainData MAIN_DATA = {
    "kfouleggs",
    I18N_NOOP("KFoulEggs"),
    I18N_NOOP("KFoulEggs is an adaptation of the well-known\n(at least in "
              "Japan) PuyoPuyo game"),
    "http://kfouleggs.sourceforge.net/",
    I18N_NOOP("Puyos"),
    "2.1.9",
    "2.1.9 (5 August 2004)"
};

const uint HISTOGRAM_SIZE = 8;
const uint HISTOGRAM[HISTOGRAM_SIZE] = {
    1, 5000, 10000, 20000, 50000, 100000, 200000, 300000
};

const BaseBoardInfo BASE_BOARD_INFO = {
    6, 15, // width - height
    true, // with pieces

    150,  // before remove time
    10,   // after removed time
    3,    // nb toggles
    5,    // nb partial fall stages

    0,    // nb arcade stages

    HISTOGRAM, HISTOGRAM_SIZE, false, // score is not bound
};

const CommonBoardInfo COMMON_BOARD_INFO = {
    1000, // base time
    10,   // drop down time
    10,   // before glue time
    10,   // after glue time
    10,   // after gift time
    3,    // nb bump stages

    100,  // nb removed to level
    5, 6, 800, 2000 // nb leds, max to send, shower timeout, pool timeout
};

FEFactory::FEFactory()
    : CommonFactory(MAIN_DATA, BASE_BOARD_INFO, COMMON_BOARD_INFO)
{}

BaseInterface *FEFactory::createInterface(QWidget *parent)
{
    return new Interface(MP_GAME_INFO, parent);
}


//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
    KHighscore::init(MAIN_DATA.appName);
    FEFactory fef;
    fef.init(argc, argv);

    FEPieceInfo pieceInfo;
    CommonHighscores highscores;
    (void) Prefs::self(); // Create preferences

    if ( kapp->isRestored() ) RESTORE(FEMainWindow)
    else {
        FEMainWindow *mw = new FEMainWindow;
        kapp->setMainWidget(mw);
        mw->show();
    }
	return kapp->exec();
}
