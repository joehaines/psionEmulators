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

  $Id: kbgoffline.h,v 1.6 2003/03/07 22:05:56 waba Exp $

*/

#ifndef __KBGOFFLINE_H
#define __KBGOFFLINE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <generic/kbgengine.h>

#include "kbgboard.h"
#include "kbgstatus.h"

class KBgEngineOfflinePrivate;

/**
 *
 * The interface of an offline backgammon engine. The engine is inherently
 * stupid and doesn't play - it just manages the games betweeen two humans
 * sitting at the same computer. Network enabled games will be part of the
 * next generation engine (KBgNg).
 *
 * @short The offline backgammon engine
 * @author Jens Hoefkens <jens@hoefkens.com>
 *
 */
class KBgEngineOffline : public KBgEngine
{
    Q_OBJECT

public:

    /**
     * Constructor
     */
    KBgEngineOffline(QWidget *parent = 0, QString *name = 0, QPopupMenu *pmenu = 0);

    /**
     * Destructor
     */
    virtual ~KBgEngineOffline();

    /**
     * Fills the engine-specific page into the notebook
     */
    virtual void getSetupPages(KDialogBase *nb);

    /**
     * Save new steup
     */
    virtual void setupOk();

    /**
     * Load default setup
     */
    virtual void setupDefault();

    /**
     * Cancel the changes to the setup
     */
    virtual void setupCancel();

    /**
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
     * Initialize the state descriptors game[0] and game[1]
     */
    void initGame();

    /**
     * Switch back and forth between edit and play mode
     */
    void toggleEditMode();

    /**
     * Store if cmd is allowed or not
     */
    void setAllowed(int cmd, bool f);

    /**
     * Swaps the used colors on the board
     */
    void swapColors();

protected:

    /**
     * Returns a random integer between 1 and 6
     */
    int getRandom();

    /**
     * Set the dice for player w to a and b. Reload the board and determine the
     * maximum number of moves
     */
    void rollDiceBackend(const int w, const int a, const int b);

    /**
     * Open a dialog to query for the name of player w. Return true unless
     * the dialog was canceled.
     */
    bool queryPlayerName(int w);

private:

    KBgEngineOfflinePrivate *d;

};

#endif // __KBGOFFLINE_H
