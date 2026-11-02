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

  $Id: kbgng.h,v 1.5 2003/09/23 16:20:40 aacid Exp $

*/

#ifndef __KBGNG_H
#define __KBGNG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <qtimer.h>
#include <qspinbox.h>
#include <kaction.h>
#include <qdatastream.h>
#include <kgameproperty.h>

#include <generic/kbgengine.h>

#include "kbgboard.h"
#include "kbgstatus.h"
#include "kbgplayer.h"
#include "kbggame.h"


/**
 *
 * The interface of the next generation backgammon engine.
 *
 */
class KBgEngineNg : public KBgEngine
{
    Q_OBJECT

public:

    /*
     * Constructor and destructor
     */
    KBgEngineNg( QWidget *parent = 0, QString *name = 0, QPopupMenu *pmenu = 0);
    virtual ~KBgEngineNg();

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
     * Roll dice for the player w
     */
    virtual void rollDice(const int w);

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

    /**
     * Process the string text
     */
    virtual void handleCommand(const QString& text);

    /**
     * Start a new game.
     */
    virtual void newGame();
    virtual bool haveNewGame() {return true;}


    void slotPlayerJoinedGame(KPlayer *p);
    void slotNetworkData(int msgid, const QByteArray &msg, Q_UINT32 receiver, Q_UINT32 sender);
    void slotCreatePlayer(KPlayer *&, int, int, bool, KGame *);

    void slotClientDisconnected(Q_UINT32, bool);
    void slotClientConnected(Q_UINT32);

    void slotPropertyChanged(KGamePropertyBase *p, KGame *me);
    void slotPropertyChanged(KGamePropertyBase *p, KPlayer *me);

protected slots:

    void initGame();

    void setGame();

    void changeName();

protected:

    void setAllowed(int cmd, bool f);

private:


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














    enum GameTypes {None = -1, Local, NetServer, NetClient, MaxTypes};
    KSelectAction * _gameSelect;
    KAction* _connectAction;
    KAction* _nameAction;
    int _currGame;
    int _nLocalPlayers;

    int _nplayers;

    QString _host;
    Q_UINT16 _port;

    // ************************************************************
    // ************************************************************

    // DONE

    // ************************************************************
    // ************************************************************


protected:

    /**
     * Return a random integer between 1 and 6. The random numer
     * is based on the @ref KRandomSequence of @ref KGame. Thus,
     * the numbers should be synchronized across the network.
     */
    int getRandom();

private:

    /**
     * Create the i-th player. Legal values for i are 0 and 1. The
     * name of the player is taken from @ref _name and the parent of
     * the player is @ref _player. That means that the players are
     * automatically deleted.
     */
    KBgPlayer * createPlayer(int i, QString name = QString::null);

private:

    KBgGame* _game;

    QString _name[2];

    KBgPlayer* _player[2];

};

#endif // __KBGNG_H
