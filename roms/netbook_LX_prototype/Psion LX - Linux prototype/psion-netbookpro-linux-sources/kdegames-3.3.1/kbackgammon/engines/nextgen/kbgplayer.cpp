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

  $Id: kbgplayer.cpp,v 1.3 2003/03/07 22:09:21 waba Exp $

*/

#include "kbgplayer.moc"
#include "kbgplayer.h"

#include <kgame.h>

#include <iostream>


/*
 * Constructors
 */
KBgPlayer::KBgPlayer()
    : KPlayer()
{
    // do nothing...
}
KBgPlayer::KBgPlayer(KGame *game)
    : KPlayer(game)
{
    // do nothing...
}

int KBgPlayer::rtti() const
{
    return 10500;
}

bool KBgPlayer::load(QDataStream &stream)
{
    KPlayer::load(stream);
    cerr << "-------- KBgPlayer::load" << endl;
}
bool KBgPlayer::save(QDataStream &stream)
{
    KPlayer::save(stream);
    cerr << "-------- KBgPlayer::save" << endl;
}
