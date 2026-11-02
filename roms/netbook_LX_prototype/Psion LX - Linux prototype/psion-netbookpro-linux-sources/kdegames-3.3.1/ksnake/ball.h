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

#ifndef BALL_H
#define BALL_H

#include "pixServer.h"

class Ball {
public:
    Ball(Board *b, PixServer *p);
    virtual ~Ball(){};
    virtual void nextMove();
    void repaint();
    void zero();
protected:
    Board   *board;
    PixServer *pixServer;
    int  index;
    int  hold;
    int  next;
};

class KillerBall : public Ball {
public:
	KillerBall(Board *b, PixServer *p) : Ball(b, p) {};
    	virtual ~KillerBall(){};
	void nextMove();
protected:
	virtual int getNextSquare();
};

class DumbKillerBall : public KillerBall {
public:
	DumbKillerBall(Board *b, PixServer *p) : KillerBall(b, p) {};
protected:
	int getNextSquare();
};

#endif // BALL_H
