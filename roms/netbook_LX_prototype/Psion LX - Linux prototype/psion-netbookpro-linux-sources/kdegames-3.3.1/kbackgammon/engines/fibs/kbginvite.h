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

  $Id: kbginvite.h,v 1.3 2003/03/07 22:05:56 waba Exp $

*/

#ifndef __KBGINVITE_H
#define __KBGINVITE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kdialog.h>

class KBgInvitePrivate;

/**
 *
 * This class implements a dialog for inviting players for games. It
 * is quite simple (but follows the default style guide). The dialog
 * offers specific numbers, unlimited and resume as invitation
 * options.
 *
 * @short Simple dialog that allows to invite somebody on FIBS
 * @author Jens Hoefkens <jens@hoefkens.com>
 *
 */
class KBgInvite : public KDialog
{
    Q_OBJECT

public:

    /**
     * Constructor
     */
    KBgInvite(const char *name = 0);

    /**
     * Destructor
     */
    virtual ~KBgInvite();

public slots:

    /**
     * After hiding, we tell our creator that we are ready to die.
     */
    virtual void hide();

    /**
     * Set the name of the player in the line editor
     */
    void setPlayer(const QString &name);

protected slots:

    /**
     * Emits the FIBS invitation command if the Ok button was clicked.
     */
    void inviteClicked();

    /**
     * Ask FIBS to resume a match
     */
    void resumeClicked();

    /**
     * Ask FIBS for an unlimited match
     */
    void unlimitedClicked();

    /**
     * Clear the entry field
     */
    void cancelClicked();

signals:

    /**
     * Emits the text of an invitation
     */
    void inviteCommand(const QString &cmd);

    /**
     * Delete the dialog after it is closed.
     */
    void dialogDone();

private:

    KBgInvitePrivate *d;
};

#endif // __KBGINVITE_H
