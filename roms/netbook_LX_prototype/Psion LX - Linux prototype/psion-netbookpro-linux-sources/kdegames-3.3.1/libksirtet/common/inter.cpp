#include "inter.h"
#include "inter.moc"

#include <klocale.h>
#include <kaction.h>
#include <kapplication.h>

#include "factory.h"
#include "field.h"
#include "commonprefs.h"
#include "main.h"


const ActionData Interface::ACTION_DATA[Nb_Actions] = {
    { I18N_NOOP("Move Left"),     "move left",     SLOT(moveLeft()),  0 },
    { I18N_NOOP("Move Right"),    "move right",    SLOT(moveRight()), 0 },
    { I18N_NOOP("Drop Down"),     "drop down",     SLOT(dropDownStart()),
                                                   SLOT(dropDownStop()) },
    { I18N_NOOP("One Line Down"), "one line down", SLOT(oneLineDown()), 0 },
    { I18N_NOOP("Rotate Left"),   "rotate left",   SLOT(rotateLeft()),  0 },
    { I18N_NOOP("Rotate Right"),  "rotate right",  SLOT(rotateRight()), 0 },
    { I18N_NOOP("Move to Left Column"), "move left total",
                                                   SLOT(moveLeftTotal()), 0 },
    { I18N_NOOP("Move to Right Column"), "move right total",
                                                   SLOT(moveRightTotal()), 0 }
};

const int Interface::KEYCODE_ONE[Nb_Actions] = {
    Key_Left, Key_Right, Key_Down, Key_Shift, Key_Up, Key_Return,
    CTRL+Key_Left, CTRL+Key_Right
};
const int Interface::KEYCODE_TWO[Nb_Actions] = {
    Key_F, Key_G, Key_D, Key_Space, Key_E, Key_C, SHIFT+Key_F, SHIFT+Key_G
};

Interface::Interface(const MPGameInfo &gi, QWidget *parent)
    : MPSimpleInterface(gi, Nb_Actions, ACTION_DATA, parent)
{
    setDefaultKeycodes(1, 0, KEYCODE_ONE);
    setDefaultKeycodes(2, 0, KEYCODE_TWO);
    setDefaultKeycodes(2, 1, KEYCODE_ONE);
}

MPBoard *Interface::newBoard(uint i)
{
    Field *f = static_cast<Field *>(cfactory->createField(this));
    f->settingsChanged();
    connect(this, SIGNAL(settingsChanged()), f, SLOT(settingsChanged()));
    if ( i==0 ) _firstField = f;
    return f;
}

void Interface::normalGame()
{
    singleHuman();
}

void Interface::arcadeGame()
{
    singleHuman();
    _firstField->setArcade();
}

void Interface::_init()
{
    if ( !server() ) return;
    _data.resize(nbPlayers());
    _scores.setPlayerCount( nbPlayers() );
    for (uint i=0; i<nbPlayers(); i++)
        _scores.setName(i, playerName(i));
}

bool Interface::_readPlayData()
{
    bool end = false;
    for (uint i=0; i<nbPlayers(); i++) {
        readingStream(i) >> _data[i];
        if (_data[i].end) end = true;
    }
    return end;
}

void Interface::_sendPlayData()
{
    ServerPlayData sd;
    for(uint i=0; i<nbPlayers(); i++) {
        sd.prevHeight = _data[prev(i)].height;
        sd.nextHeight = _data[next(i)].height;
        sd.gift       = _data[prev(i)].gift;
        writingStream(i) << sd;
    }
}

void Interface::_showHighscores(QWidget *parent)
{
    if ( !server() || nbPlayers()!=1 ) _scores.show(parent);
    else BaseInterface::_showHighscores(parent);
}

void Interface::_showGameOverData()
{
    if ( !server() || nbPlayers()!=1 ) _scores.show(this);
    else if ( !_firstField->isArcade() ) {
        _score.setType(KExtHighscore::Won);
        BaseField::gameOver(_score, this);
    }
}

uint Interface::prev(uint i) const
{
	if ( i==0 ) return nbPlayers()-1;
	else return i-1;
}

uint Interface::next(uint i) const
{
	if ( i==(nbPlayers()-1) ) return 0;
	else return i+1;
}

// server only
void Interface::_treatInit()
{
    ServerInitData sid;
    sid.seed = kapp->random();
    sid.initLevel = CommonPrefs::initialGameLevel();
    for (uint i=0; i<nbPlayers(); i++) {
        sid.prevName = playerName(prev(i));
        sid.nextName = playerName(next(i));
        sid.name     = playerName(i);
        writingStream(i) << sid;
    }
}

void Interface::_sendGameOverData(QDataStream &s)
{
    bool multiplayers = ( nbPlayers()>1 );
    for (uint i=0; i<nbPlayers(); i++) {
        readingStream(i) >> _score;
        if (multiplayers) _scores.addScore(i, _score);
    }
    if (multiplayers) s << _scores;
    // else no need to send anything
}

// client only
void Interface::_readGameOverData(QDataStream &s)
{
    s >> _scores;
}
