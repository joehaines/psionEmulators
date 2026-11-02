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

  $Id: kbgengine.h,v 1.4 2003/03/07 22:05:56 waba Exp $

*/

#ifndef __KBGENGINE_H
#define __KBGENGINE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qobject.h>

class QTimer;
class QPopupMenu;

class KDialogBase;

class KBgStatus;

/**
 *
 * Abstract class for a generic backgammon engine. Real engines have
 * to inherit this and implement the interfaces.
 *
 * Engines can and will use the following global events described in
 * the file eventsrc:
 *
 * "game over l"
 * "game over w"
 *
 * "roll"
 * "roll or double"
 *
 * "move"
 * "invitation"
 *
 * @short Abstract base class for backgammon engines
 * @author Jens Hoefkens <jens@hoefkens.com>
 *
 */
class KBgEngine:public QObject
{
    Q_OBJECT public:

    enum Command { Redo, Undo, Roll, Cube, Done, Load };

    /**
     * Constructor
     */
    KBgEngine (QWidget * parent = 0, QString * name = 0, QPopupMenu * pmenu = 0);

    /**
     * Destructor
     */
    virtual ~KBgEngine ();

    /**
     * Fills the engine-specific page into the notebook
     */
    virtual void getSetupPages (KDialogBase * nb) = 0;

    /**
     * Called after the user clicked ok in the setup dialog. Time
     * to save settings.
     */
    virtual void setupOk () = 0;

    /**
     * The user cancelled the setup
     */
    virtual void setupCancel () = 0;

    /**
     * Set engine defaults
     */
    virtual void setupDefault () = 0;

    /**
     * Called when the windows are about to be hidden. Engines
     * should hide all their child windows.
     *
     * The default implementation does nothing.
     */
    virtual void hideEvent ()
    {
    }

    /**
     * Called when the windows are about to be shown. Engines
     * should show all visible child windows.
     *
     * The default implementation does nothing.
     */
    virtual void showEvent ()
    {
    }

    /**
     * Start the engine. This is called pretty much right after
     * the constructor. While the constructor may not have any
     * user interaction, it is possible to display dialogs in
     * start.
     *
     * The default implementation does nothing.
     */
    virtual void start ()
    {
    };

    /**
     * Check with the engine if we can quit. This may require user
     * interaction.
     *
     * The default implementation returns true.
     */
    virtual bool queryClose ()
    {
        return true;
    }

    /**
     * About to be closed. Let the engine exit properly.
     *
     * The default implementation returns true.
     */
    virtual bool queryExit ()
    {
        return true;
    }

    /**
     * Set the length of the commit timeout. Negative values
     * disable the feature.
     */
    void setCommit (const double com = 2.5);

public slots:
    /**
     * Read user settings from the config file
     */
    virtual void readConfig () = 0;

    /**
     * Save user settings to the config file
     */
    virtual void saveConfig () = 0;

    /**
     * Roll dice for the player w
     */
    virtual void rollDice (const int w) = 0;

    /**
     * Double the cube of player w
     */
    virtual void doubleCube (const int w) = 0;

    /**
     * A move has been made on the board - see the board class
     * for the format of the string s
     */
    virtual void handleMove (QString * s) = 0;

    /**
     * Undo the last move
     */
    virtual void undo () = 0;

    /**
     * Redo the last move
     */
    virtual void redo () = 0;

    /**
     * Roll dice for whoevers turn it is
     */
    virtual void roll () = 0;

    /**
     * Double the cube for whoevers can double right now
     */
    virtual void cube () = 0;

    /**
     * Reload the board to the last known sane state
     */
    virtual void load () = 0;

    /**
     * Commit a move
     */
    virtual void done () = 0;

    /**
     * Process the string cmd
     */
    virtual void handleCommand (const QString & cmd) = 0;

    /**
     * Start a new game
     */
    virtual void newGame ()
    {
    }

    /**
     * Can we start a new game?
     */
    virtual bool haveNewGame ()
    {
        return false;
    }

signals:

    /**
     * The text identifies the current game status - could be put
     * into the main window caption
     */
    void statText (const QString & msg);

    /**
     * Text that should be displayed in the ongoing message window
     */
    void infoText (const QString & msg);

    /**
     * Emit the most recent game state
     */
    void newState (const KBgStatus &);

    /**
     * Tell the board that we need the current state of the board.
     */
    void getState (KBgStatus *);

    /**
     * Starts/ends the edit mode of the board
     */
    void setEditMode (const bool f);

    /**
     * Toggle RO/RW flag of the board
     */
    void allowMoving (const bool fl);

    /**
     * Announce that we will accept/reject the command cmd from
     * now on
     */
    void allowCommand (int cmd, bool f);

    /**
     * Tell the board to undo the last move
     */
    void undoMove ();

    /**
     * Tell the board to redo the last undone move
     */
    void redoMove ();

protected:

    /**
     * Context menu for the board
     */
    QPopupMenu * menu;

    /**
     * Commit timer
     */
    QTimer *ct;
    int cl;

};

#endif // __KBGENGINE_H
