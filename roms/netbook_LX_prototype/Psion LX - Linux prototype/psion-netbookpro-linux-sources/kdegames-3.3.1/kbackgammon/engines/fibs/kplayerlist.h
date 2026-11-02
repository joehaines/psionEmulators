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

  $Id: kplayerlist.h,v 1.8 2003/03/07 22:05:56 waba Exp $

*/

#ifndef __KPLAYERLIST_H
#define __KPLAYERLIST_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <klistview.h>

class KTabCtl;
class KFibsPlayerListPrivate;

/**
 *
 * A class that keeps track of players on the server. The server is flooding
 * us with user information. At any given time we are able to have an current
 * list of all loged-in players and their status.
 *
 * @short The FIBS backgammon engine player list
 * @author Jens Hoefkens <jens@hoefkens.com>
 *
 */
class KFibsPlayerList : public KListView
{
    Q_OBJECT

public:

    /**
     * Enumerate player status
     */
    enum PStatus {Ready, Away, Blind, MaxStatus};

    /**
     * Enumerate the different columns of the list
     */
    enum {Player, Opponent, Watches, Status, Rating, Experience,
          Idle, Time, Host, Client, Email, LVEnd};

    /**
     * Constructor
     */
    KFibsPlayerList(QWidget *parent = 0, const char *name = 0);

    /**
     * Destructor
     */
    virtual ~KFibsPlayerList();

    /**
     * Clear the list and reset the client counter
     */
    virtual void clear();

public slots:

    /**
     * Remove the player with the name given by the first word
     */
    void deletePlayer(const QString &player);

    /**
     * Change/Add the entry for the given player
     */
    void changePlayer(const QString &line);

    /**
     * Enables list redraws after an update
     */
    void stopUpdate();

    /**
     * Read the UI settings from disk
     */
    void readConfig();

    /**
     * Read the column info from disk
     */
    void readColumns();

    /**
     * Restore settings from previously stored settings
     */
    void saveConfig();

    /**
     * Change the status of a player
     */
    void changePlayerStatus(const QString &player, int stat, bool flag);

    /**
     * Fills the playerlist page into the notebook
     */
    virtual void getSetupPages(KTabCtl *nb, int space);

    /**
     * Save setting changes
     */
    void setupOk();

    /**
     * Setup changes have been cancelled
     */
    void setupCancel();

    /**
     * Set default settings
     */
    void setupDefault();

    /**
     * Set our own name. This allows us to special case the context
     * menu.
     */
    void setName(const QString &name);

    /**
     * Return the column index
     */
    int cIndex(int col);

protected:

    /**
     * Catch show events, so the engine's menu can be update.
     */
    virtual void showEvent(QShowEvent *e);

    /**
     * Catch hide events, so the engine's menu can be update.
     */
    virtual void hideEvent(QHideEvent *e);

protected slots:

    /**
     * Double click handler, requests information on a player
     */
    void getPlayerInfo(QListViewItem *i, const QPoint &p, int col);

    /**
     * Display a popup menu for the current player
     */
    void showContextMenu(KListView *, QListViewItem *, const QPoint &);

    /**
     * Reload the whole list
     */
    void slotReload();

    /**
     * Upate the caption
     */
    void updateCaption();

    /**
     * Watch user
     */
    void slotWatch();

    /**
     * Update line of user
     */
    void slotUpdate();

    /**
     * Request information on user
     */
    void slotInfo();

    /**
     * Look at user
     */
    void slotLook();

    /**
     * Send an email to user
     */
    void slotMail();

    /**
     * Stop watching
     */
    void slotUnwatch();

    /**
     * Blind user
     */
    void slotBlind();

    /**
     * Talk to user
     */
    void slotTalk();

    /**
     * Invite using the dialog
     */
    void slotInviteD();

    /**
     * Invite to a 1 point match
     */
    void slotInvite1();

    /**
     * Invite to a 2 point match
     */
    void slotInvite2();

    /**
     * Invite to a 3 point match
     */
    void slotInvite3();

    /**
     * Invite to a 4 point match
     */
    void slotInvite4();

    /**
     * Invite to a 5 point match
     */
    void slotInvite5();

    /**
     * Invite to a 6 point match
     */
    void slotInvite6();

    /**
     * Invite to a 7 point match
     */
    void slotInvite7();

    /**
     * Invite to resume a saved match
     */
    void slotInviteR();

    /**
     * Invite to an unlimited match
     */
    void slotInviteU();

signals:

    /**
     * Send a command to the server
     */
    void fibsCommand(const QString &);

    /**
     * Initiate an invitation of a player
     */
    void fibsInvite(const QString &);

    /**
     * Request talking to player user
     */
    void fibsTalk(const QString &);

    /**
     * Allow the engine's menu to be updated
     */
    void windowVisible(bool);

private:

    KFibsPlayerListPrivate *d;

};

#endif // __KPLAYERLIST_H
