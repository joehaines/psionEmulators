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

  $Id: kbggnubg.h,v 1.3 2003/03/07 22:05:56 waba Exp $

*/

#ifndef __KBGGNU_H
#define __KBGGNU_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <generic/kbgengine.h>

#include <qtimer.h>
#include <qspinbox.h>
#include <kaction.h>
#include <krandomsequence.h>
#include <kprocess.h>
#include <qstringlist.h>

/**
 *
 *
 */
class KBgEngineGNU : public KBgEngine
{
	Q_OBJECT

public:

	/*
	 * Constructor and destructor
	 */
	KBgEngineGNU(QWidget *parent = 0, QString *name = 0, QPopupMenu *pmenu = 0);
	virtual ~KBgEngineGNU();

	/**
	 * Fills the engine-specific page into the notebook
	 */
	virtual void getSetupPages(KDialogBase *nb);

	virtual void setupOk();
	virtual void setupDefault();
	virtual void setupCancel();

	/*
	 * Check with the engine if we can quit. This may require user
	 * interaction.
	 */
	virtual bool queryClose();

	/**
	 * About to be closed. Let the engine exit properly.
	 */
	virtual bool queryExit();

	virtual void start();

public slots:

	/**
	 * Read user settings from the config file
	 */
	virtual void readConfig();

	/**
	 * Save user settings to the config file
	 */
	virtual void saveConfig();

	/**
	 * Double the cube of player w
	 */
	virtual void doubleCube(const int w);

	/**
	 * A move has been made on the board - see the board class
	 * for the format of the string s
	 */
	virtual void handleMove(QString *s);

	/**
	 * Undo the last move
	 */
        virtual void undo();

	/**
	 * Redo the last move
	 */
        virtual void redo();

	/**
	 * Roll dice for whoevers turn it is
	 */
        virtual void roll();

	/**
	 * Double the cube for whoevers can double right now
	 */
        virtual void cube();

	/**
	 * Reload the board to the last known sane state
	 */
	virtual void load();

	/**
	 * Commit a move
	 */
	virtual void done();

        /*
	 * Roll dice for the player w
	 */
        virtual void rollDice(const int w);

	/**
	 * Process the string cmd
	 */
	virtual void handleCommand(const QString& cmd);

	/**
	 * Start a new game.
	 */
	virtual void newGame();
	virtual bool haveNewGame() {return true;}

protected slots:

	/**
	 * Store if cmd is allowed or not
	 */
	void setAllowed(int cmd, bool f);

	void startGNU();

private:

	/**
	 * Use the standard method of obtaining random numbers
	 */
	KRandomSequence random;

	/**
	 * Player's names
	 */
	QString nameUS, nameTHEM;

	/**
	 * Who did the last roll
	 */
	int lastRoll;

	/**
	 * How many checkers to move
	 */
	int toMove;

	/**
	 * Various flags, representing the current status of the game
	 */
	bool rollingAllowed, undoPossible, donePossible;
	bool gameRunning,    redoPossible, doublePossible;

	/**
	 * Count the number of available undos
	 */
	int dummy, undoCounter;

private:

	enum Turn {uRoll, uMove, tRoll, tMove, maxTurn};

	KProcess gnubg;

	QStringList cmdList;

	QTimer *cmdTimer;

	QString partline;

	QString board;

	QString lastmove;

	int turn;

	KAction *resAction;

protected slots:

	void wroteStdin(KProcess *);

	void receiveData(KProcess *, char *buffer, int buflen);

	void handleLine(const QString &l);

	void gnubgExit(KProcess *proc);

	void nextCommand();

};

#endif // __KBGGNU_H
