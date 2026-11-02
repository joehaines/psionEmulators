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

  $Id: kbgengine.cpp,v 1.5 2003/03/07 22:09:21 waba Exp $

*/

#include <qtimer.h>
#include <qpopupmenu.h>

#include <kdialogbase.h>

#include <kbgengine.moc>
#include "kbgengine.h"


/*
 * Constructor initializes the QObject
 */
KBgEngine::KBgEngine(QWidget *parent, QString *name, QPopupMenu *pmenu)
    : QObject(parent, name->local8Bit())
{
    menu = pmenu;
    cl = -1;
    ct = new QTimer(this);
    connect(ct, SIGNAL(timeout()), this, SLOT(done()));
}

/*
 * Destructor is empty
 */
KBgEngine::~KBgEngine()
{
    // empty
}

/*
 * Set the length of the commit timeout. Negative values disable the
 * feature.
 */
void KBgEngine::setCommit(const double com)
{
    cl = int(1000*com);
}

// EOF
