/* Yo Emacs, this -*- C++ -*-

  Copyright (C) 1999-2001 Jens Hoefkens
  jens@hoefkens.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  $Id: kbgoffline.cpp,v 1.14 2004/07/04 12:03:40 binner Exp $

*/

#include "kbgoffline.moc"
#include "kbgoffline.h"

#include <qlayout.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qtimer.h>
#include <qspinbox.h>
#include <qwhatsthis.h>
#include <qlineedit.h>
#include <qvbox.h>

#include <kapplication.h>
#include <kmessagebox.h>
#include <kiconloader.h>
#include <kstdaction.h>
#include <kconfig.h>
#include <klocale.h>
#include <kmainwindow.h>
#include <klineeditdlg.h>
#include <kaction.h>
#include <krandomsequence.h>
#include <ktabctl.h>
#include <stdlib.h>

#include "version.h"

class KBgEngineOfflinePrivate
{
public:

    /*
     * Various flags, representing the current status of the game
     */
    bool mRollFlag, mUndoFlag, mDoneFlag, mCubeFlag, mGameFlag, mRedoFlag;

    /*
     * Store two copies of the game: one backup and a working copy
     */
    KBgStatus mGame[2];

    /*
     * Use the standard method of obtaining random numbers
     */
    KRandomSequence *mRandom;

    /*
     * Game actions
     */
    KAction *mNew, *mSwap;
    KToggleAction *mEdit;

    /*
     * Player's names
     */
    QString mName[2];

    /*
     * Who did the last roll
     */
    int mRoll;

    /*
     * How many checkers to move
     */
    int mMove;

    /*
     * Count the number of available undos
     */
    int mUndo;

    /*
     * Entry fields for the names
     */
    QLineEdit *mLe[2];

};


// == constructor, destructor and other ========================================

/*
 * Constructor
 */
KBgEngineOffline::KBgEngineOffline(QWidget *parent, QString *name, QPopupMenu *pmenu)
    : KBgEngine(parent, name, pmenu)
{
    d = new KBgEngineOfflinePrivate();

    /*
     * get some entropy for the dice
     */
    d->mRandom = new KRandomSequence;
    d->mRandom->setSeed(0);

    /*
     * Create engine specific actions
     */
    d->mNew  = new KAction(i18n("&New Game..."),    0, this, SLOT(newGame()),  this);
    d->mSwap = new KAction(i18n("&Swap Colors"), 0, this, SLOT(swapColors()), this);

    d->mEdit = new KToggleAction(i18n("&Edit Mode"), 0, this,
                                 SLOT(toggleEditMode()), this);
    d->mEdit->setChecked(false);

    /*
     * create & initialize the menu
     */
    d->mNew->plug(menu);
    d->mEdit->plug(menu);
    d->mSwap->plug(menu);

    /*
     * get standard board and set it
     */
    initGame();
    emit newState(d->mGame[0]);

    /*
     * initialize the commit timeout
     */
    ct = new QTimer(this);
    connect(ct, SIGNAL(timeout()), this, SLOT(done()));

    /*
     * internal statue variables
     */
    d->mRollFlag = d->mUndoFlag = d->mGameFlag = d->mDoneFlag = false;
    connect(this, SIGNAL(allowCommand(int, bool)), this, SLOT(setAllowed(int, bool)));

    /*
     * Restore last stored settings
     */
    readConfig();
}

/*
 * Destructor. The only child is the popup menu.
 */
KBgEngineOffline::~KBgEngineOffline()
{
    saveConfig();
    delete d->mRandom;
    delete d;
}


// == configuration handling ===================================================

/*
 * Put the engine specific details in the setup dialog
 */
void KBgEngineOffline::getSetupPages(KDialogBase *nb)
{
    /*
     * Main Widget
     */
    QVBox *vbp = nb->addVBoxPage(i18n("Offline Engine"), i18n("Use this to configure the Offline engine"),
                                 kapp->iconLoader()->loadIcon(PROG_NAME "_engine", KIcon::Desktop));

    /*
     * Get a multi page work space
     */
    KTabCtl *tc = new KTabCtl(vbp, "offline tabs");

    /*
     * Player names
     */
    QWidget *w = new QWidget(tc);
    QGridLayout *gl = new QGridLayout(w, 2, 1, nb->spacingHint());

    /*
     * Group boxes
     */
    QGroupBox *gbn = new QGroupBox(i18n("Names"), w);

    gl->addWidget(gbn, 0, 0);

    gl = new QGridLayout(gbn, 2, 2, 20);

    d->mLe[0] = new QLineEdit(d->mName[0], gbn);
    d->mLe[1] = new QLineEdit(d->mName[1], gbn);

    QLabel *lb[2];
    lb[0] = new QLabel(i18n("First player:"), gbn);
    lb[1] = new QLabel(i18n("Second player:"), gbn);

    for (int i = 0; i < 2; i++) {
        gl->addWidget(lb[i], i, 0);
        gl->addWidget(d->mLe[i], i, 1);
    }

    QWhatsThis::add(d->mLe[0], i18n("Enter the name of the first player."));
    QWhatsThis::add(d->mLe[1], i18n("Enter the name of the second player."));

    /*
     * Done with the page, put it in
     */
    gl->activate();
    tc->addTab(w, i18n("&Player Names"));
}

/*
 * Called when the setup dialog is positively closed
 */
void KBgEngineOffline::setupOk()
{
    d->mName[0] = d->mLe[0]->text();
    d->mName[1] = d->mLe[1]->text();
}
void KBgEngineOffline::setupDefault()
{
    d->mName[0] = i18n("South");
    d->mName[1] = i18n("North");
}
void KBgEngineOffline::setupCancel()
{
    // do nothing
}

/*
 * Restore settings
 */
void KBgEngineOffline::readConfig()
{
    KConfig* config = kapp->config();
    config->setGroup("offline engine");

    d->mName[0] = config->readEntry("player-one", i18n("South")); // same as above
    d->mName[1] = config->readEntry("player-two", i18n("North")); // same as above
    cl = config->readNumEntry("timer", 2500);
}

/*
 * Save the engine specific settings
 */
void KBgEngineOffline::saveConfig()
{
    KConfig* config = kapp->config();
    config->setGroup("offline engine");

    config->writeEntry("player-one", d->mName[0]  );
    config->writeEntry("player-two", d->mName[1]);
    config->writeEntry("timer",      cl);
}


// == start and init games =====================================================

/*
 * Start a new game.
 */
void KBgEngineOffline::newGame()
{
    int u = 0;
    int t = 0;

    /*
     * If there is a game running we warn the user first
     */
    if (d->mGameFlag && (KMessageBox::warningYesNo((QWidget *)parent(),
                                                   i18n("A game is currently in progress. "
                                                        "Starting a new one will terminate it."),
                                                   QString::null, i18n("Start New Game"),
                                                   i18n("Continue Old Game"))
                         == KMessageBox::No))
        return;

    /*
     * Separate from the previous game
     */
    emit infoText("<br/><br/><br/>");

    /*
     * Get player's names - user can still cancel
     */
    if (!queryPlayerName(US) || !queryPlayerName(THEM))
        return;

    /*
     * let the games begin
     */
    d->mGameFlag = true;

    /*
     * Initialize the board
     */
    initGame();

    /*
     * Figure out who starts by rolling
     */
    while (u == t) {
        u = getRandom();
        t = getRandom();
        emit infoText(i18n("%1 rolls %2, %3 rolls %4.").
                      arg(d->mName[0]).arg(u).arg(d->mName[1]).arg(t));
    }

    if (u > t) {
        emit infoText(i18n("%1 makes the first move.").arg(d->mName[0]));
        d->mRoll = US;
    } else {
        emit infoText(i18n("%1 makes the first move.").arg(d->mName[1]));
        d->mRoll = THEM;
        int n = u; u = t; t = n;
    }

    /*
     * set the dice and tell the board
     */
    rollDiceBackend(d->mRoll, u, t);

    /*
     * tell the user
     */
    emit statText(i18n("%1 vs. %2").arg(d->mName[0]).arg(d->mName[1]));
}

/*
 * Initialize the state descriptors mGame[0|1]
 */
void KBgEngineOffline::initGame()
{
    /*
     * nobody rolled yet
     */
    d->mRoll = -1;

    /*
     * set up a standard game
     */
    d->mGame[0].setCube(1, true, true);
    d->mGame[0].setDirection(+1);
    d->mGame[0].setColor(+1);
    for (int i = 1; i < 25; i++)
        d->mGame[0].setBoard(i, US, 0);
    d->mGame[0].setBoard( 1, US,   2); d->mGame[0].setBoard( 6, THEM, 5);
    d->mGame[0].setBoard( 8, THEM, 3); d->mGame[0].setBoard(12, US,   5);
    d->mGame[0].setBoard(13, THEM, 5); d->mGame[0].setBoard(17, US,   3);
    d->mGame[0].setBoard(19, US,   5); d->mGame[0].setBoard(24, THEM, 2);
    d->mGame[0].setHome(US, 0); d->mGame[0].setHome(THEM, 0);
    d->mGame[0].setBar(US, 0); d->mGame[0].setBar(THEM, 0);
    d->mGame[0].setDice(US  , 0, 0); d->mGame[0].setDice(US  , 1, 0);
    d->mGame[0].setDice(THEM, 0, 0); d->mGame[0].setDice(THEM, 1, 0);

    /*
     * save backup of the game state
     */
    d->mGame[1] = d->mGame[0];

    emit allowCommand(Load, true);
}

/*
 * Open a dialog to query for the name of player w. Return true unless
 * the dialog was canceled.
 */
bool KBgEngineOffline::queryPlayerName(int w)
{
    bool ret = false;
    QString *name;
    QString text;

    if (w == US) {
        name = &d->mName[0];
        text = i18n("Please enter the nickname of the player whose home\n"
                    "is in the lower half of the board:");
    } else {
        name = &d->mName[1];
        text = i18n("Please enter the nickname of the player whose home\n"
                    "is in the upper half of the board:");
    }

    do {
        *name = KLineEditDlg::getText(text, *name, &ret, (QWidget *)parent());
        if (!ret) break;

    } while (name->isEmpty());

    return ret;
}


// == moving ===================================================================

/*
 * Finish the last move - called by the timer and directly by the used
 */
void KBgEngineOffline::done()
{
    ct->stop();

    emit allowMoving(false);
    emit allowCommand(Done, false);
    emit allowCommand(Undo, false);

    if (abs(d->mGame[0].home(d->mRoll)) == 15) {

        emit infoText(i18n("%1 wins the game. Congratulations!").
                      arg((d->mRoll == US) ? d->mName[0] : d->mName[1]));
        d->mGameFlag = false;
        emit allowCommand(Roll, false);
        emit allowCommand(Cube, false);

    } else {

        emit allowCommand(Roll, true);
        if (d->mGame[0].cube((d->mRoll == US ? THEM : US)) > 0) {

            d->mGame[0].setDice(US  , 0, 0); d->mGame[0].setDice(US  , 1, 0);
            d->mGame[0].setDice(THEM, 0, 0); d->mGame[0].setDice(THEM, 1, 0);

            emit newState(d->mGame[0]);
            emit getState(&d->mGame[0]);

            d->mGame[1] = d->mGame[0];

            emit infoText(i18n("%1, please roll or double.").
                          arg((d->mRoll == THEM) ? d->mName[0] : d->mName[1]));
            emit allowCommand(Cube, true);

        } else {

            roll();
            emit allowCommand(Cube, false);
        }
    }
}

/*
 * Undo the last move
 */
void KBgEngineOffline::undo()
{
    ct->stop();

    d->mRedoFlag = true;
    ++d->mUndo;

    emit allowMoving(true);
    emit allowCommand(Done, false);
    emit allowCommand(Redo, true);
    emit undoMove();
}

/*
 * Redo the last move
 */
void KBgEngineOffline::redo()
{
    --d->mUndo;
    emit redoMove();
}

/*
 * Take the move string and make the changes on the working copy
 * of the state.
 */
void KBgEngineOffline::handleMove(QString *s)
{
    int index = 0;
    QString t = s->mid(index, s->find(' ', index));
    index += 1 + t.length();
    int moves = t.toInt();

    /*
     * Allow undo and possibly start the commit timer
     */
    d->mRedoFlag &= ((moves < d->mMove) && (d->mUndo > 0));
    emit allowCommand(Undo, moves > 0);
    emit allowCommand(Redo, d->mRedoFlag);
    emit allowCommand(Done, moves == d->mMove);
    if (moves == d->mMove && cl) {
        emit allowMoving(false);
        ct->start(cl, true);
    }

    /*
     * Apply moves to d->mGame[1] and store results in d->mGame[0]
     */
    d->mGame[0] = d->mGame[1];

    /*
     * process each individual move
     */
    for (int i = 0; i < moves; i++) {
        bool kick = false;
        t = s->mid(index, s->find(' ', index) - index);
        index += 1 + t.length();
        char c = '-';
        if (t.contains('+')) {
            c = '+';
            kick = true;
        }
        QString r = t.left(t.find(c));
        if (r.contains("bar")) {
            d->mGame[0].setBar(d->mRoll, abs(d->mGame[0].bar(d->mRoll)) - 1);
        } else {
            int from = r.toInt();
            d->mGame[0].setBoard(from, d->mRoll, abs(d->mGame[0].board(from)) - 1);
        }
        t.remove(0, 1 + r.length());
        if (t.contains("off")) {
            d->mGame[0].setHome(d->mRoll, abs(d->mGame[0].home(d->mRoll)) + 1);
        } else {
            int to = t.toInt();
            if (kick) {
                d->mGame[0].setBoard(to, d->mRoll, 0);
                int el = ((d->mRoll == US) ? THEM : US);
                d->mGame[0].setBar(el, abs(d->mGame[0].bar(el)) + 1);
            }
            d->mGame[0].setBoard(to, d->mRoll, abs(d->mGame[0].board(to)) + 1);
        }
    }
}


// == dice & rolling ===========================================================

/*
 * Roll random dice for the player whose turn it is
 */
void KBgEngineOffline::roll()
{
    rollDice((d->mRoll == US) ? THEM : US);
}

/*
 * If possible, roll random dice for player w
 */
void KBgEngineOffline::rollDice(const int w)
{
    if ((d->mRoll != w) && d->mRollFlag) {
        rollDiceBackend(w, getRandom(), getRandom());
        return;
    }
    emit infoText(i18n("It's not your turn to roll!"));
}

/*
 * Return a random integer between 1 and 6. According to the man
 * page of rand(), this is the way to go...
 */
int KBgEngineOffline::getRandom()
{
    return 1+d->mRandom->getLong(6);
}

/*
 * Set the dice for player w to a and b. Reload the board and determine the
 * maximum number of moves
 */
void KBgEngineOffline::rollDiceBackend(const int w, const int a, const int b)
{
    /*
     * This is a special case that stems from leaving the edit
     * mode.
     */
    if (a == 0)
        return;

    /*
     * Set the dice and tel the board about the new state
     */
    d->mGame[0].setDice(w, 0, a);
    d->mGame[0].setDice(w, 1, b);
    d->mGame[0].setDice((w == US) ? THEM : US, 0, 0);
    d->mGame[0].setDice((w == US) ? THEM : US, 1, 0);
    d->mGame[0].setTurn(w);

    d->mGame[1] = d->mGame[0];

    d->mRoll = w;
    emit newState(d->mGame[0]);

    /*
     * No more roling until Done and no Undo yet
     */
    emit allowCommand(Undo, false);
    emit allowCommand(Roll, false);
    d->mRedoFlag = false;
    d->mUndo = 0;

    /*
     * Tell the players how many checkers to move
     */
    switch (d->mMove = d->mGame[0].moves()) {
    case -1:
        emit infoText(i18n("Game over!"));
        d->mGameFlag = false;
        emit allowCommand(Roll, false);
        emit allowCommand(Cube, false);
        emit allowMoving(false);
        break;
    case  0:
        emit infoText(i18n("%1, you cannot move.").
                      arg((w == US) ? d->mName[0] : d->mName[1]));
        if (cl)
            ct->start(cl, true);
        emit allowMoving(false);
        break;
    case  1:
        emit infoText(i18n("%1, please move 1 piece.").
                      arg((w == US) ? d->mName[0] : d->mName[1]));
        emit allowMoving(true);
        break;
    default:
        emit infoText(i18n("%1, please move %2 pieces.").
                      arg((w == US) ? d->mName[0] : d->mName[1]).arg(d->mMove));
        emit allowMoving(true);
        break;
    }
}


// == cube =====================================================================

/*
 * Double the cube for the player that can double  - asks player
 */
void KBgEngineOffline::cube()
{
    int w = ((d->mRoll == US) ? THEM : US);

    if (d->mRollFlag && d->mGame[0].cube(w) > 0) {
        emit allowCommand(Cube, false);
        if (KMessageBox::questionYesNo((QWidget *)parent(),
                                       i18n("%1 has doubled. %2, do you accept the double?").
                                       arg((w == THEM) ? d->mName[1] : d->mName[0]).
                                       arg((w == US) ? d->mName[1] : d->mName[0]),
                                       i18n("Doubling"), i18n("Accept"), i18n("Reject")) != KMessageBox::Yes) {
            d->mGameFlag = false;
            emit allowCommand(Roll, false);
            emit allowCommand(Cube, false);
            emit infoText(i18n("%1 wins the game. Congratulations!").
                          arg((w == US) ? d->mName[0] : d->mName[1]));
            return;
        }

        emit infoText(i18n("%1 has accepted the double. The game continues.").
                      arg((w == THEM) ? d->mName[0] : d->mName[1]));

        if (d->mGame[0].cube(US)*d->mGame[0].cube(THEM) > 0)
            d->mGame[0].setCube(2, w == THEM, w == US);
        else
            d->mGame[0].setCube(2*d->mGame[0].cube(w), w == THEM, w == US);

        emit newState(d->mGame[0]);
        emit getState(&d->mGame[0]);

        d->mGame[1] = d->mGame[0];

        roll();
    }
}

/*
 * Double the cube for player w
 */
void KBgEngineOffline::doubleCube(const int)
{
    cube();
}


// == various slots & functions ================================================

/*
 * Check with the user if we should really quit in the middle of a
 * game.
 */
bool KBgEngineOffline::queryClose()
{
    if (!d->mGameFlag)
        return true;

    switch (KMessageBox::warningYesNo((QWidget *)parent(),
                                      i18n("In the middle of a game. "
                                           "Really quit?"))) {
    case KMessageBox::Yes :
        return TRUE;
    case KMessageBox::No :
        return FALSE;
    default: // cancel
        return FALSE;
    }
    return true;
}

/*
 * Quitting is fine at any time
 */
bool KBgEngineOffline::queryExit()
{
    return true;
}

/*
 * Handle textual commands. Right now, all commands are ignored
 */
void  KBgEngineOffline::handleCommand(const QString& cmd)
{
    emit infoText(i18n("Text commands are not yet working. "
                       "The command '%1' has been ignored.").arg(cmd));
}

/*
 * Load the last known sane state of the board
 */
void KBgEngineOffline::load()
{
    if (d->mEdit->isChecked())
        emit newState(d->mGame[1]);
    else {
        // undo up to four moves
        undo();
        undo();
        undo();
        undo();
    }
}

/*
 * Store if cmd is allowed or not
 */
void KBgEngineOffline::setAllowed(int cmd, bool f)
{
    switch (cmd) {
    case Roll:
        d->mRollFlag = f;
        return;
    case Undo:
        d->mUndoFlag = f;
        return;
    case Cube:
        d->mCubeFlag = f;
        return;
    case Done:
        d->mDoneFlag = f;
        return;
    }
}

/*
 * Swaps the used colors on the board
 */
void KBgEngineOffline::swapColors()
{
    d->mGame[1].setDice(US,   0, d->mGame[0].dice(US,   0));
    d->mGame[1].setDice(US,   1, d->mGame[0].dice(US,   1));
    d->mGame[1].setDice(THEM, 0, d->mGame[0].dice(THEM, 0));
    d->mGame[1].setDice(THEM, 1, d->mGame[0].dice(THEM, 1));
    d->mGame[1].setColor(d->mGame[1].color(THEM), US);
    emit newState(d->mGame[1]);
    emit getState(&d->mGame[1]);
    d->mGame[0] = d->mGame[1];
}

/*
 * Switch back and forth between edit and play mode
 */
void KBgEngineOffline::toggleEditMode()
{
    emit setEditMode(d->mEdit->isChecked());
    if (d->mEdit->isChecked()) {
        ct->stop();
        d->mNew->setEnabled(false);
        d->mSwap->setEnabled(false);
        emit allowCommand(Undo, false);
        emit allowCommand(Roll, false);
        emit allowCommand(Done, false);
        emit allowCommand(Cube, false);
        emit statText(i18n("%1 vs. %2 - Edit Mode").arg(d->mName[0]).arg(d->mName[1]));
    } else {
        d->mNew->setEnabled(true);
        d->mSwap->setEnabled(true);
        emit statText(i18n("%1 vs. %2").arg(d->mName[0]).arg(d->mName[1]));
        emit getState(&d->mGame[1]);
        d->mGame[0] = d->mGame[1];
        emit allowCommand(Done, d->mDoneFlag);
        emit allowCommand(Cube, d->mCubeFlag);
        emit allowCommand(Undo, d->mUndoFlag);
        emit allowCommand(Roll, d->mRollFlag);
        int w =((d->mGame[0].dice(US, 0) && d->mGame[0].dice(US, 1)) ? US : THEM);
        rollDiceBackend(w, d->mGame[0].dice(w, 0), d->mGame[0].dice(w, 1));
    }
}

// EOF
