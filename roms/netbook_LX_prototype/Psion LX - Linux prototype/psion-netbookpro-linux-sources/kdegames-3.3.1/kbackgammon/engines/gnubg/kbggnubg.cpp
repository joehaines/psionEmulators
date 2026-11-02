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

  $Id: kbggnubg.cpp,v 1.7 2003/03/07 22:09:20 waba Exp $

*/

#include "kbggnubg.moc"
#include "kbggnubg.h"

#include <kapplication.h>
#include <kmessagebox.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <qlayout.h>
#include <kiconloader.h>
#include <kstdaction.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <kconfig.h>
#include <iostream>
#include <klocale.h>
#include <kmainwindow.h>
#include <klineeditdlg.h>
#include <qregexp.h>
#include <kregexp.h>
#include <knotifyclient.h>

#include <string.h>
#include <stdio.h>
#include <signal.h>

#include "kbgstatus.h"
#include "kbgboard.h"
#include "version.h"


// == cube =====================================================================

/*
 * Double the cube for the player that can double  - asks player
 */
void KBgEngineGNU::cube()
{
	handleCommand("double");
}

/*
 * Double the cube for player w
 */
void KBgEngineGNU::doubleCube(const int w)
{
	dummy = w; // avoid compiler warning
	cube();
}





void KBgEngineGNU::handleLine(const QString &l)
{
	if (l.isEmpty())
		return;

	QString line(l);

	/*
	 * Start of a new game/match
	 */
	if (line.contains(QRegExp("^gnubg rolls [1-6], .* rolls [1-6]\\."))) {
		KRegExp e("^gnubg rolls ([1-6]), .* rolls ([1-6])\\.");
		e.match(line.latin1());
		if (int r = strcmp(e.group(1), e.group(2)))
			turn = (r < 0) ? uRoll : tRoll;
	}

	/*
	 * Bug fixes for older versions of GNUBG - to be removed
	 */
	if (line.contains(QRegExp("^.* cannot move\\..+$"))) {
		KRegExp e("(^.* cannot move.)(.*$)");
		e.match(line.latin1());
		handleLine(e.group(1));
		handleLine(e.group(2));
		return;
	}
	if (line.contains(QRegExp("^Are you sure you want to start a new game, and discard the one in progress\\?"))) {
		KRegExp e("(^Are you sure you want to start a new game, and discard the one in progress\\? )(.+$)");
		e.match(line.latin1());
		handleLine(e.group(1));
		handleLine(e.group(2));
		return;
	}

	/*
	 * Cube handling
	 */
	if (line.contains(QRegExp("^gnubg accepts and immediately redoubles to [0-9]+\\.$"))) {

		// redoubles mess up the game counter "turn"

		//KBgStatus st(board);
		//st.setCube(32, BOTH);
		//emit newState(st);

	}
	if (line.contains(QRegExp("^gnubg doubles\\.$"))) {

		// TODO: we need some generic class for this. the class
		//       can be shared between all engines

#if 0
		KBgStatus st(board);

		int ret = KMessageBox::warningYesNoCancel
			(0, i18n("gnubg doubles the cube to %1.").arg(2*st.cube(THEM)),
			 i18n("gnubg doubles"),
			 i18n("&Accept"), i18n("Re&double"), i18n("&Reject"), true);

		switch (ret) {

		case KMessageBox::Yes:
			handleCommand("accept");
			break;

		case KMessageBox::No:
			handleCommand("redouble");
			break;

		case KMessageBox::Cancel:
			handleCommand("reject");
			break;
		}
#endif
	}

	/*
	 * Ignore the following messages
	 */
	if (line.contains(QRegExp("^TTY boards will be given in raw format"))) {
		line = " ";
	}

	/*
	 * Board messages
	 */
	if (line.contains(QRegExp("^board:"))) {

		KBgStatus st(line);

		/*
		 * Do preliminary analysis of board
		 */
		if (st.doubled()) {
			--turn;
			return;
		}
		if (strcmp(board.latin1(),line.latin1()))
			++turn %= maxTurn;
		board = line;

		/*
		 * Act according to the current state in the move/roll cycle
		 */
		switch (turn) {

		case uRoll:

			if (st.cube() > 0) {
				emit infoText(i18n("Please roll or double."));
				KNotifyClient::event("roll or double");
			} else {
				emit infoText(i18n("Please roll."));
				KNotifyClient::event("roll");
			}

			emit allowCommand(Roll, true);
			emit allowCommand(Cube, true);
			break;

		case uMove:
			st.setDice(THEM, 0, 0);
			st.setDice(THEM, 1, 0);
			emit infoText(i18n("You roll %1 and %2.").arg(st.dice(US, 0)).arg(st.dice(US, 1)));
			switch (st.moves()) {
			case 0:
				// get a message
				break;
			case 1:
				emit infoText(i18n("Please move 1 piece."));
				break;
			default:
				emit infoText(i18n("Please move %1 pieces.").arg(st.moves()));
				break;
			}
			emit allowCommand(Roll, false);
			break;

		case tRoll:
			break;

		case tMove:
			st.setDice(US, 0, 0);
			st.setDice(US, 1, 0);
			emit infoText(i18n("gnubg rolls %1 and %2.").arg(st.dice(THEM, 0)).arg(st.dice(THEM, 1)));
			if (st.moves() == 0)
				emit infoText(i18n("gnubg cannot move."));

			break;

		}

		/*
		 * Bookkeeping
		 */
		undoCounter = 0;
		toMove = st.moves();
		emit allowMoving(st.turn() == US);
		emit newState(st);

		emit statText(i18n("%1 vs. %2").arg(st.player(US)).arg(st.player(THEM)));

		emit allowCommand(Load, true );
		emit allowCommand(Undo, false);
		emit allowCommand(Redo, false);
		emit allowCommand(Done, false);
		return;
	}

	/*
	 * Show the line...
	 */
	line.replace(QRegExp(" "), "&nbsp;");
	if (!line.isEmpty())
		emit infoText(line);
}


/*
 * Handle textual commands. All commands are passed to gnubg.
 */
void  KBgEngineGNU::handleCommand(const QString& cmd)
{
	cmdList += cmd;
	nextCommand();
}



// == start and init games =====================================================

/*
 * Start a new game.
 */
void KBgEngineGNU::newGame()
{
	/*
	 * If there is a game running we warn the user first
	 */
	if (gameRunning && (KMessageBox::warningYesNo((QWidget *)parent(),
						      i18n("A game is currently in progress. "
							   "Starting a new one will terminate it."),
						      QString::null, i18n("Start New Game"), i18n("Continue Old Game"))
			    == KMessageBox::No))
		return;

	/*
	 * Start new game
	 */
	handleCommand("new game");
	if (gameRunning)
		handleCommand("yes");

	gameRunning = true;

	emit infoText(i18n("Starting a new game."));
}



// == various slots & functions ================================================

/*
 * Quitting is fine at any time
 */
bool KBgEngineGNU::queryClose()
{
	return true;
}

/*
 * Quitting is fine at any time
 */
bool KBgEngineGNU::queryExit()
{
	return true;
}

/*
 * Load the last known sane state of the board
 */
void KBgEngineGNU::load()
{
	handleCommand("show board");
}

/*
 * Store if cmd is allowed or not
 */
void KBgEngineGNU::setAllowed(int cmd, bool f)
{
	switch (cmd) {
	case Roll:
		rollingAllowed = f;
		return;
	case Undo:
		undoPossible = f;
		return;
	case Cube:
		doublePossible = f;
		return;
	case Done:
		donePossible = f;
		return;
	}
}















// == configuration handling ===================================================

void KBgEngineGNU::setupOk()
{
	// nothing yet
}

void KBgEngineGNU::setupCancel()
{
	// nothing yet
}

void KBgEngineGNU::setupDefault()
{
	// nothing yet
}

void KBgEngineGNU::getSetupPages(KDialogBase *nb)
{
	/*
	 * Main Widget
	 */
	QVBox *w = nb->addVBoxPage(i18n("GNU Engine"), i18n("Here you can configure the GNU backgammon engine"),
				     kapp->iconLoader()->loadIcon(PROG_NAME "_engine", KIcon::Desktop));
}

/*
 * Restore settings
 */
void KBgEngineGNU::readConfig()
{
	KConfig* config = kapp->config();
	config->setGroup("gnu engine");

	// nothing yet
}

/*
 * Save the engine specific settings
 */
void KBgEngineGNU::saveConfig()
{
	KConfig* config = kapp->config();
	config->setGroup("gnu engine");

	// nothing yet
}



// *****************************************************************************
// *****************************************************************************
// *****************************************************************************
// *****************************************************************************
// *****************************************************************************
// *****************************************************************************




// == constructor, destructor and other ========================================

/*
 * Constructor
 */
KBgEngineGNU::KBgEngineGNU(QWidget *parent, QString *name, QPopupMenu *pmenu)
	: KBgEngine(parent, name, pmenu)
{
	// obsolete
	nameUS   = "US";
	nameTHEM = "THEM";
	random.setSeed(getpid()*time(NULL));

	/*
	 * internal statue variables
	 */
	rollingAllowed = undoPossible = gameRunning = donePossible = false;
	connect(this, SIGNAL(allowCommand(int, bool)), this, SLOT(setAllowed(int, bool)));

	/*
	 * Setup of menu
	 */
	resAction  = new KAction(i18n("&Restart GNU Backgammon"), 0, this, SLOT(startGNU()), this);
	resAction->setEnabled(false); resAction->plug(menu);

	/*
	 * Restore last stored settings
	 */
	readConfig();
}

/*
 * Destructor. Kill the child process and that's it.
 */
KBgEngineGNU::~KBgEngineGNU()
{
	gnubg.kill();
}


// == start, restart, termination of gnubg =====================================

/*
 * Start the GNU Backgammon process in the background and set up
 * some communication links.
 */
void KBgEngineGNU::start()
{
	/*
	 * Will be started later
	 */
	cmdTimer = new QTimer(this);
	connect(cmdTimer, SIGNAL(timeout()), SLOT(nextCommand()) );

	emit infoText(i18n("This is experimental code which currently requires a specially "
			   "patched version of GNU Backgammon.<br/><br/>"));

	/*
	 * Initialize variables
	 */
	partline = board = "";

	/*
	 * Start the process - this requires that gnubg is in the PATH
	 */
	gnubg << "gnubg" << "--tty";

        connect(&gnubg, SIGNAL(processExited(KProcess *)), this, SLOT(gnubgExit(KProcess *)));
        connect(&gnubg, SIGNAL(receivedStderr(KProcess *, char *, int)),
		this, SLOT(receiveData(KProcess *, char *, int)));
	connect(&gnubg, SIGNAL(receivedStdout(KProcess *, char *, int)),
		this, SLOT(receiveData(KProcess *, char *, int)));
	connect(&gnubg, SIGNAL(wroteStdin(KProcess *)), this, SLOT(wroteStdin(KProcess *)));

	startGNU();
}

/*
 * Actually start the background process.
 */
void KBgEngineGNU::startGNU()
{

	resAction->setEnabled(false);

	if (!gnubg.start(KProcess::NotifyOnExit, KProcess::All))
		KMessageBox::information((QWidget *)parent(),
	                i18n("Could not start the GNU Backgammon process.\n"
			     "Make sure the program is in your PATH and is "
			     "called \"gnubg\".\n"
			     "Make sure that your copy is at least version 0.10"));

	/*
	 * Set required gnubg options
	 */
	handleCommand("set output rawboard on");
}

/*
 * The gnubg process has died. Stop all user activity and allow a restart.
 */
void KBgEngineGNU::gnubgExit(KProcess *proc)
{
	ct->stop();

	cmdTimer->stop();

	emit allowCommand(Undo, false);
	emit allowCommand(Roll, false);
	emit allowCommand(Done, false);
	emit allowCommand(Cube, false);
	emit allowCommand(Load, false);

	emit allowMoving(false);

	emit infoText(QString("<br/><font color=\"red\">") + i18n("The GNU Backgammon process (%1) has exited. ")
		      .arg(proc->pid()) + "</font><br/>");

	resAction->setEnabled(true);
}


// == communication callbacks with GNU bg ======================================

/*
 * Last command has been sent. Try to send pending ones.
 */
void KBgEngineGNU::wroteStdin(KProcess *proc)
{
	if (!proc->isRunning())
		return;
	nextCommand();
}

/*
 * Try to send the next command from the command list to gnubg.
 * If it fails, make sure we call ourselves again.
 */
void KBgEngineGNU::nextCommand()
{
	if (!gnubg.isRunning())
		return;

        for (QStringList::Iterator it = cmdList.begin(); it != cmdList.end(); ++it) {
		QString s = (*it) + "\n";
		if (!gnubg.writeStdin(s.latin1(), strlen(s.latin1()))) {
			cmdTimer->start(250, true);
			cmdList.remove(QString::null);
			return;
		}
		(*it) = QString::null;
        }
	cmdList.remove(QString::null);
	cmdTimer->stop();
}

/*
 * Get data from GNU Backgammon and process them. Note that we may have
 * to buffer the last line and wait for the closing newline...
 */
void KBgEngineGNU::receiveData(KProcess *proc, char *buffer, int buflen)
{
	if (!proc->isRunning())
		return;

	char *buf = new char[buflen+1];

	memcpy(buf, buffer, buflen);
	buf[buflen] = '\0';

	QStringList l(QStringList::split('\n', buf, true));

	/*
	 * Restore partial lines from the previous time
	 */
	l.first() = partline + l.first();
	partline = "";
	if (buf[buflen-1] != '\n') {
		partline = l.last();
		l.remove(partline);
	}

	delete[] buf;

	/*
	 * Handle the information from gnubg
	 */
	for (QStringList::Iterator it = l.begin(); it != l.end(); ++it)
		handleLine(*it);
}


// == moving ===================================================================

/*
 * Finish the last move - called by the timer and directly by the user
 */
void KBgEngineGNU::done()
{
	ct->stop();

	emit allowMoving(false);

	emit allowCommand(Done, false);
	emit allowCommand(Undo, false);
	emit allowCommand(Redo, false);

	// Transform the string to FIBS format
	lastmove.replace(0, 2, "move ");
	lastmove.replace(QRegExp("\\+"), " ");
	lastmove.replace(QRegExp("\\-"), " ");

	// sent it to the server
	handleCommand(lastmove);
}

/*
 * Undo the last move
 */
void KBgEngineGNU::undo()
{
	ct->stop();

	redoPossible = true;
	++undoCounter;

	emit allowMoving(true);

	emit allowCommand(Done, false);
	emit allowCommand(Redo, true);

	emit undoMove();
}

/*
 * Redo the last move
 */
void KBgEngineGNU::redo()
{
	--undoCounter;
	emit redoMove();
}

/*
 * Take the move string and make the changes on the working copy
 * of the state.
 */
void KBgEngineGNU::handleMove(QString *s)
{
	lastmove = *s;

	int index = 0;
	QString t = s->mid(index, s->find(' ', index));
	index += 1 + t.length();
	int moves = t.toInt();

	/*
	 * Allow undo and possibly start the commit timer
	 */
	redoPossible &= ((moves < toMove) && (undoCounter > 0));

	emit allowCommand(Undo, moves > 0);
	emit allowCommand(Redo, redoPossible);
	emit allowCommand(Done, moves == toMove);

	if (moves == toMove && cl >= 0) {
		emit allowMoving(false);
		ct->start(cl, true);
	}
}


// == dice & rolling ===========================================================

/*
 * Roll random dice for the player whose turn it is. We can ignore the
 * value of w, since we have the turn value.
 */
void KBgEngineGNU::roll()
{
	if (turn == uRoll)
		handleCommand("roll");
}
void KBgEngineGNU::rollDice(const int w)
{
	roll();
}



// EOF
