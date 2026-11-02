/**
 * This file is part of the ksnake package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <qtimer.h>
#include <qptrlist.h>

#include "board.h"
#include "basket.h"
#include "pixServer.h"

Kaffee::Kaffee(int pos, int r1, int r2)
{
    p = pos;
    t = Red;
    r = r2;
    QTimer::singleShot( r1, this, SLOT(golden()) );
    dirty = TRUE;
}

void Kaffee::golden()
{
    dirty = TRUE;
    t = (t == Red ? Golden : Red);
    QTimer::singleShot( r, this, SLOT(golden()) );
}

Basket::Basket(Board *b, PixServer *p)
{
    board = b;
    pixServer = p;
    list = new QPtrList<Kaffee>;
    list->setAutoDelete( TRUE );
}

Basket::~Basket()
{
	delete list;
}

void Basket::clear()
{
    if( !list->isEmpty())
	list->clear();
}

void Basket::newApples()
{
    int x;
    int i = 0;

    while(i < 10) {
	x =  random.getLong(board->size());
	if ((unsigned)x < board->size() && board->isEmpty(x) && x > BoardWidth+4) {
	    Kaffee *g = new Kaffee(x, random.getLong(40000), random.getLong(40000));
	    board->set(x, Apple);
	    list->append(g);
	    i++;
	}
    }
}

void Basket::repaint(bool dirty )
{
    Kaffee *g;
    for ( g  = list->first(); g != 0; g = list->next()) {
	if (dirty) {
	    pixServer->draw(g->position(), ApplePix, (int)g->type());
	    g->dirty = FALSE;
	}
	else if (g->dirty) {
	    pixServer->draw(g->position(), ApplePix, (int)g->type());
	    g->dirty = FALSE;
	}
    }
}

Fruits Basket::eaten(int i)
{
    Kaffee *g;
    Fruits f = Red;

    for (g = list->first(); g != 0; g = list->next() )
	{
	    if (g->position() == i) {
		f = g->type();
		list->remove(g);
		break;
	    }
	}
    if (list->isEmpty())
	emit openGate();

    return f;
}

#include "basket.moc"
