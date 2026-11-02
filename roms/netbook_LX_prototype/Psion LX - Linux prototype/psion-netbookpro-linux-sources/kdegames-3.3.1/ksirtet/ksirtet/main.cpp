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
    "006", // multiplayer id (increase when incompatible changes are made)
    4,     // max nb local games
    500,   // interval
    true,  // IA allowed
    0, 0   // no setting slots
};

const MainData MAIN_DATA = {
    "ksirtet",
    I18N_NOOP("KSirtet"),
    I18N_NOOP("KSirtet is an adaptation of the well-known Tetris game"),
    "http://ksirtet.sourceforge.net/",
    I18N_NOOP("Removed Lines"),
    "2.1.10",
    "2.1.10 (4 July 2004)"
};

const uint HISTOGRAM_SIZE = 8;
const uint HISTOGRAM[HISTOGRAM_SIZE] = {
    1, 5000, 10000, 20000, 50000, 100000, 200000, 300000
};

const BaseBoardInfo BASE_BOARD_INFO = {
    KSBoard::Width, 22, // width - height
    true, // with pieces

    150,  // before remove time
    10,   // after remove time
    3,    // nb toggles
    5,    // nb partial fall stages

    KSBoard::Nb_Stages, // nb arcade stages

    HISTOGRAM, HISTOGRAM_SIZE, false, // score is not bound
};

const CommonBoardInfo COMMON_BOARD_INFO = {
    1000, // base time
    20,   // drop down time
    10,   // before glue time
    10,   // after glue time
    10,   // after gift time
    3,    // nb bump stages
    10,   // nb removed to level
    3, 3, 800, 2000 // nb leds, nb max to send, shower timeout, pool timeout
};

KSFactory::KSFactory()
    : CommonFactory(MAIN_DATA, BASE_BOARD_INFO, COMMON_BOARD_INFO)
{}

BaseInterface *KSFactory::createInterface(QWidget *parent)
{
    return new Interface(MP_GAME_INFO, parent);
}

//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
    KHighscore::init(MAIN_DATA.appName);
    KSFactory ksf;
    ksf.init(argc, argv);

    KSPieceInfo pieceInfo;
    CommonHighscores highscores;

    (void) Prefs::self(); // Create preferences

    if ( kapp->isRestored() ) RESTORE(KSMainWindow)
    else {
        KSMainWindow *mw = new KSMainWindow;
        kapp->setMainWidget(mw);
        mw->show();
    }
    return kapp->exec();
}
