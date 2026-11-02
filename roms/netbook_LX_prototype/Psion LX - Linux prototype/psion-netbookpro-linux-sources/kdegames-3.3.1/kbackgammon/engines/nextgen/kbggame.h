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

  $Id: kbggame.h,v 1.3 2003/03/07 22:05:56 waba Exp $

*/

#ifndef __KBGGAME_H
#define __KBGGAME_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kgame.h>

class QObject;
class KPlayer;

/**
 *
 *
 */
class KBgGame : public KGame
{
    Q_OBJECT

public:

    enum MsgID {Text, Cmd, MaxMsg};

    KBgGame(int cookie = 42, QObject *parent = 0);

protected:

    virtual bool playerInput(QDataStream &msg,KPlayer *player);

};

#endif // __KBGGAME_H

