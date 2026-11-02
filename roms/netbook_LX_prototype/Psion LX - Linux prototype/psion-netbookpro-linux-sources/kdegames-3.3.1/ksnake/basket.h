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

#ifndef BASKET_H
#define BASKET_H

#include <krandomsequence.h>

class PixServer;

enum Fruits { Red, Golden };

class Kaffee : public QObject
{
    Q_OBJECT
public:
    Kaffee(int pos, int r1, int r2);
    int position() { return p;}
    Fruits type() { return t;}
    bool dirty;
private slots:
    void golden();
private:
    int p;
    int r;
    Fruits t;
};

class Basket : public QObject
{
    Q_OBJECT
public:
    Basket(Board *b, PixServer *p);
    ~Basket();
    void repaint(bool);
    void newApples();
    void clear();
    Fruits eaten( int i);
signals:
    void openGate();
private:
    Board   *board;
    PixServer *pixServer;
    QPtrList<Kaffee> *list;
    KRandomSequence random;
};

#endif // BASKET_H
