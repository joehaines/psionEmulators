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

  $Id: kbgfibschat.h,v 1.14 2003/03/07 22:05:56 waba Exp $

*/

#ifndef __KBGCHAT_H
#define __KBGCHAT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kchat.h>

class QString;
class QPoint;
class QListBox;
class QListBoxItem;
class QPopupMenu;

class KTabCtl;
class KAction;

class KBgChatPrivate;

/**
 * Class of the FIBS Chat Windows
 *
 * This class inherits from KChat and represents a widget for a chat
 * window. It has rich text entries and supports a powerful context
 * menu.
 *
 * @short An extension of the KGame chat window for the FIBS engine
 * @author Jens Hoefkens <jens@hoefkens.com>
 *
 */
class KBgChat : public KChat
{
    Q_OBJECT

public:

    /**
     * Constructor
     */
    KBgChat(QWidget *parent = 0, const char *name = 0);

    /**
     * Destructor
     */
    virtual ~KBgChat();

public slots:

    /**
     * Catch the RMB signal to display a context menu at p. The
     * menu shows entries specific to the selected item i.
     */
    void contextMenu(QListBoxItem *i, const QPoint &p);

    /**
     * Add chat window specific pages to the setup dialog
     */
    void getSetupPages(KTabCtl *nb, int space);

    /**
     * Save and apply the changes made in the setup dialog
     */
    void setupOk();

    /**
     * Do not save any of the changes made in the setup dialog
     */
    void setupCancel();

    /**
     * Load default values from the setup dialog
     */
  void setupDefault();

    /**
     * Player name has logges out. Remove name from the chat
     * window combo box if necessary.
     */
    void deletePlayer(const QString &name);

    /**
     * Process and append msg to the text.
     */
    void handleData(const QString &msg);

    /**
     * Restore previously saved setting or provides defaults
     */
    void readConfig();

    /**
     * Save current settings
     */
    void saveConfig();

    /**
     * Set the opponents name and select whisper
     */
    void startGame(const QString &name);

    /**
     * Game is over. We won (or not) and have been playing (or not)
     */
    void endGame();

    /**
     * Start talking to name
     */
    void fibsTalk(const QString &name);

signals:

    /**
     * Emits a string that can be sent to the server
     */
    void fibsCommand(const QString &cmd);

    /**
     * Request an invitation of player
     */
    void fibsRequestInvitation(const QString &player);

    /**
     * Text of a personal message
     */
    void personalMessage(const QString &msg);

    /**
     * Dialog is visible or not
     */
    void windowVisible(bool v);

protected:

    /**
     * Catch show events, so the engine's menu can be updated.
     */
    virtual void showEvent(QShowEvent *e);

    /**
     * Catch hide events, so the engine's menu can be updated.
     */
    virtual void hideEvent(QHideEvent *e);

    /**
     * Create a custom ListBoxItem that contains a formated string
     * for the chat window.
     */
    virtual QListBoxItem* layoutMessage(const QString& fromName, const QString& text);

protected slots:

    /**
     * Invite the selected player using the dialog
     */
    void slotInviteD();

    /**
     * Invite the selected player to resume a match
     */
    void slotInviteR();

    /**
     * Invite the selected player to an unlimited match
     */
    void slotInviteU();

    /**
     * Invite the selected player to a 1 point match
     */
    void slotInvite1();

    /**
     * Invite the selected player to a 2 point match
     */
    void slotInvite2();

    /**
     * Invite the selected player to a 3 point match
     */
    void slotInvite3();

    /**
     * Invite the selected player to a 4 point match
     */
    void slotInvite4();

    /**
     * Invite the selected player to a 5 point match
     */
    void slotInvite5();

    /**
     * Invite the selected player to a 6 point match
     */
    void slotInvite6();

    /**
     * Invite the selected player to a 7 point match
     */
    void slotInvite7();

    /**
     * Request information on the selected player
     */
    void slotInquire();

    /**
     * Copy the selected line to the clipboard
     */
    void slotCopy();

    /**
     * Talk to the selected player
     */
    void slotTalk();

    /**
     * Add the selected player to the gag list
     */
    void slotGag();

    /**
     * Remove the selected player from the gag list
     */
    void slotUngag();

    /**
     * Clear the gag list
     */
    void slotCleargag();

    /**
     * Toggle everybody silent
     */
    void slotSilent();

    /**
     * Slot for return pressed. Time to send the text to FIBS.
     */
    void handleCommand(int id, const QString& msg);

private:

    KBgChatPrivate *d;

};

#endif // __KBGCHAT_H
