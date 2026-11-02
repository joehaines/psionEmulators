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

#ifndef SAMY_H
#define SAMY_H

#include <krandomsequence.h>
#include "pixServer.h"
#include "board.h"

class Snake : public QObject
{

	Q_OBJECT

signals:
	void score(bool, int);
	void killed();
	void closeGate(int);
	void restartTimer();
	void goingOut();

public:
	Snake(Board *b, PixServer *p, Gate g, PixMap x);
	~Snake() {}
	void repaint( bool );
	void zero();

protected:
	Board   *board;
	PixServer *pixServer;
	Gate  gate;
	PixMap pixmap;

	struct Samy {
		int direction;
		SnakePix pixmap;
		int index;
	};

	QPtrList<Samy> list;

	KRandomSequence random;

	void reset(int index, int border);
	void appendSamy();
	void updateSamy();
	int  tail() const    { return (list.count() -1 ); }
	bool growing() const { return (grow > 0 ? TRUE : FALSE); }

	int   hold;
	int   grow;
};

class CompuSnake : public Snake
{

public:
	CompuSnake(Board *b, PixServer *p);
	virtual ~CompuSnake() {};
	virtual void nextMove();

protected:
	bool init();
	void removeSamy();
	bool findEmpty(int i, int it);
	bool permission();
	void out();

};

/**
 * Don't eat any apples.
 * Try to hit samy's head.
 */
class KillerCompuSnake : public CompuSnake
{

public:
	KillerCompuSnake(Board *b, PixServer *p) : CompuSnake(b, p) {};
	virtual ~KillerCompuSnake() {};
	virtual void nextMove();

private:
	int lastIndex;

};

/**
 * Eat as much apples as it can, from down to up.
 * When all apples are eaten, try to reach the gate.
 */
class EaterCompuSnake : public CompuSnake
{

public:
	EaterCompuSnake(Board *b, PixServer *p) : CompuSnake(b, p) {};
	virtual ~EaterCompuSnake() {};
	virtual void nextMove();

private:
	int lastIndex;

};

enum samyState { ok, ko, out };

class SamySnake : public Snake
{

public:
	SamySnake(Board *, PixServer *);
	samyState nextMove(int direction);
	void init();

};

#endif // SAMY_H
