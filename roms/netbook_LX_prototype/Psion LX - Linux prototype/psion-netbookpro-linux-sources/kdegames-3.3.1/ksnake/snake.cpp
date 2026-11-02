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

#include <qwidget.h>

#include "snake.h"

int opposite[4] = { S, N , W, E };

int emptySq[4][4]={
	{ N, E, W, N },
	{ S, W, E, S },
	{ E, N, S, E },
	{ W, S, N, W }
};

Snake::Snake(Board *b, PixServer *p, Gate g, PixMap x)
{
	list.setAutoDelete( true );
	pixServer = p;
	board = b;
	gate  = g;
	pixmap = x;
	random.setSeed(0);
}

void Snake::updateSamy()
{
	int x = tail();
	while ( x > 0)
		*list.at(x) = *list.at(--x);
}

void Snake::zero()
{
	for ( Samy *sam = list.first(); sam != 0; sam = list.next() ) {
		board->set(sam->index, empty);
		pixServer->erase(sam->index);
	}
}

void Snake::appendSamy()
{
	Samy *sam = new Samy;
	list.append(sam);

	int x = tail();
	while ( x > 0)
		*list.at(x) = *list.at(--x);
	grow--;
}

void Snake::reset(int index, int border)
{
	Samy *sam = list.first();

	switch (border) {
	case N:
	sam->pixmap = (tail() == 0 ? HtailUp : HeadUp);
	break;
	case S:
	sam->pixmap = (tail() == 0 ? HtailDown : HeadDown);
	break;
	case E:
	sam->pixmap = (tail() == 0 ? HtailRight : HeadRight);
	break;
	case W:
	sam->pixmap = (tail() == 0 ? HtailLeft : HeadLeft);
	break;
	}

	sam->index = index;
	sam->direction  = border;

	if (tail() > 1) {

	sam = list.next();

	if (sam->direction == border) {
		if (border == N || border == S)
		sam->pixmap = BodyVt;
		else
		sam->pixmap = BodyHz;
	}
	else {
		if (border == W && sam->direction == S
		|| border == N && sam->direction == E)
		sam->pixmap = AngleNw;
		if (border == E && sam->direction == S
		|| border == N && sam->direction == W)
		sam->pixmap = AngleNe;
		if(border == W && sam->direction == N
		   || border == S && sam->direction == E)
		sam->pixmap = AngleSw;
		if(border == E && sam->direction == N
		   || border == S && sam->direction == W)
		sam->pixmap = AngleSe;
	}


	} //end if (tail() > 1)

	if (tail() > 0) {

	sam = list.last();

	switch (list.at(tail()-1)->direction) {
	case N:
		sam->pixmap = TailUp;
		break;
	case S:
		sam->pixmap = TailDown;
		break;
	case E:
		sam->pixmap = TailRight;
		break;
	case W:
		sam->pixmap = TailLeft;
		break;
	}
	}
}

void Snake::repaint( bool dirty)
{
	int x = 0;
	for ( Samy *sam = list.first(); sam != 0; sam = list.next(), x++) {
		if (sam->index != OUT ) {
			if(!dirty && x > 1 && x < tail())
				continue;
			pixServer->draw(sam->index, pixmap, sam->pixmap);
		}
	}
	if (!growing() && hold != OUT && hold != gate) {
		pixServer->erase(hold);
	}
}


CompuSnake::CompuSnake( Board *b, PixServer *p)
	: Snake( b, p, NORTH_GATE, CompuSnakePix )
{
	init();
}

bool CompuSnake::init()
{
	if( !list.isEmpty()) {
		list.clear();
	}

	int index = NORTH_GATE;
	int length = 12;
	grow = 0;
	hold = OUT;

	if ( !board->isBrick(gate) ) return false;

	Samy *sam;
	for ( int x = 0; x < length; x++) {
		board->set(index, snake);
		sam = new Samy;
		sam->direction = S;
		sam->index = index;
		sam->pixmap = (x == 0 ? HeadDown : BodyVt);
		list.append(sam);
		index = -1;
	}
	return true;
}

bool CompuSnake::permission()
{
	if( list.isEmpty() ){

	if ( hold != OUT) {
		emit killed();
		hold = OUT;
	}

	if(board->isBrick(gate)){
		static int  skip = 12;
		if (skip < 12) {
		skip++;
		return false;
		} else {
		skip = 0;
		return init();
		}
	}
	else return false;
	}
	else return true;
}

void CompuSnake::nextMove()
{
	if (!permission())
		return;

	Samy *sam = list.first();
	int  index = sam->index;
	int  dir = sam->direction;
	static bool varies = false;


	bool found = false;

	for ( int x = 0; x < 4 ; x++) {
		int next = board->getNext(x, sam->index);
		if (board->isApple(next)){
		index = next;
		dir = x;
		found = true;
		grow+=6;
		emit score(false, index);
		break;
		}
	}

	if(!found)
	for ( int x = 0; x < 4 ; x++) {
		int sq = emptySq[sam->direction][x];
		if (varies && (x > 0 && x < 3))
		sq = opposite[sq];
		int next = board->getNext(sq, sam->index);
		if (findEmpty(next, x)) {
		index = next;
		dir = sq;
		found = true;
		break;
		}
	}
	varies = !varies;

	if(!found) {
	hold = list.last()->index;
	if (board->isSnake(hold)) board->set(hold, empty);
	removeSamy();
	}
	else
	if(growing())
		appendSamy();
	else
		if (!growing() && found) {
		hold = list.last()->index;
		if (board->isSnake(hold)) board->set(hold, empty);
		updateSamy();
		}

	if( !list.isEmpty()) {
	board->set(index, snake);
	reset(index, dir);
	}

	if ( hold == gate)
	out();
}

void KillerCompuSnake::nextMove()
{
	if (!permission()) return;
	
	Samy *sam = list.first();
	int  index = sam->index;
	int  dir = sam->direction;
	static bool varies = false;
	
	
	bool found = false;
	
	if(!found) {
		int sn = board->samyHeadIndex();
		if(sn != -1 && board->isHead(sn)) {
			int nextSq = board->getNextCloseTo(index, sn, false, lastIndex);
			if(nextSq != -1) {
				dir = board->direction(index, nextSq);
				index = nextSq;
				found = true;
			}
		}
	}
	
	if(!found)
		for ( int x = 0; x < 4 ; x++) {
			int sq = emptySq[sam->direction][x];
			if (varies && (x > 0 && x < 3))
				sq = opposite[sq];
			int next = board->getNext(sq, sam->index);
			if (findEmpty(next, x)) {
				index = next;
				dir = sq;
				found = true;
				break;
			}
		}
	varies = !varies;
	
	if(!found) {
		hold = list.last()->index;
		if (board->isSnake(hold)) board->set(hold, empty);
		removeSamy();
	}
	else {
		lastIndex = index;
		
		if(growing())
			appendSamy();
		else
			if (!growing() && found) {
				hold = list.last()->index;
				if (board->isSnake(hold)) board->set(hold, empty);
				updateSamy();
			}
	}
			
	if( !list.isEmpty()) {
		board->set(index, snake);
		reset(index, dir);
	}
	
	if ( hold == gate)
		out();
}

void EaterCompuSnake::nextMove()
{
	if (!permission()) return;
	
	Samy *sam = list.first();
	int  index = sam->index;
	int  dir = sam->direction;
	static bool varies = false;
	
	
	bool found = false;
	
	for ( int x = 0; x < 4 ; x++) {
		int next = board->getNext(x, sam->index);
		if (board->isApple(next)){
			index = next;
			dir = x;
			found = true;
			grow+=6;
			emit score(false, index);
			break;
		}
	}
	
	if(!found) {
		int sn;
		bool apple = false;
		for(sn = board->count() - 1; sn > 0; sn --) {
			if(board->isApple(sn)) {
				apple = true;
				int nextSq = board->getNextCloseTo(index, sn, false);
				if(nextSq != -1) {
					dir = board->direction(index, nextSq);
					index = nextSq;
					found = true;
					break;
				}
			}
		}
		if(!found && !apple) {
			// No more apples, move snake to gate.
			int nextSq = board->getNextCloseTo(index, gate, false, lastIndex);
			if(nextSq != -1) {
				dir = board->direction(index, nextSq);
				index = nextSq;
				found = true;
			}
		}
	}
	
	if(!found)
		for ( int x = 0; x < 4 ; x++) {
			int sq = emptySq[sam->direction][x];
			if (varies && (x > 0 && x < 3))
				sq = opposite[sq];
			int next = board->getNext(sq, sam->index);
			if (findEmpty(next, x)) {
				index = next;
				dir = sq;
				found = true;
				break;
			}
		}
	varies = !varies;
	
	if(!found) {
		hold = list.last()->index;
		if (board->isSnake(hold)) board->set(hold, empty);
		removeSamy();
	}
	else {
		lastIndex = index;
		
		if(growing())
			appendSamy();
		else
			if (!growing() && found) {
				hold = list.last()->index;
				if (board->isSnake(hold)) board->set(hold, empty);
				updateSamy();
			}
	}
			
	if( !list.isEmpty()) {
		board->set(index, snake);
		reset(index, dir);
	}
	
	if ( hold == gate)
		out();
}

bool CompuSnake::findEmpty(int i, int it)
{
	bool found = false;
	bool change = false;
	static int s_random =  random.getLong(BoardWidth/2);
	static int moves  = 0;

	if (moves > s_random) {
		s_random =  random.getLong(BoardWidth/2);
		moves  = 0;
		change = true;
	}

	found =  (   (  board->isEmpty(i) && it > 0)
		 || (  board->isEmpty(i) && !change && it == 0) );

	moves++;
	change = false;
	return found;
}

void CompuSnake::removeSamy()
{
	list.remove();
	grow = 0;
}

void CompuSnake::out()
{
	emit closeGate( gate );

	if( list.isEmpty() )
		return;

	if(list.first()->index == OUT) {
		emit restartTimer();
		list.clear();
	}
}

SamySnake::SamySnake( Board *b, PixServer *p)
	: Snake( b, p, SOUTH_GATE, SamyPix )
{

}

void SamySnake::init()
{
	if( !list.isEmpty()) {
		list.clear();
	}
	Samy *sam;

	int index = SOUTH_GATE;
	int length = 12;
	grow = 0;
	hold = 0;

	for ( int x = 0; x < length; x++) {
		board->set(index, head);
		sam = new Samy;
		sam->direction = N;
		sam->index = index;
		sam->pixmap = (x == 0 ? HeadUp : BodyVt);
		list.append(sam);
		index = -1;
	}
}

samyState SamySnake::nextMove(int direction)
{
	Samy *sam = list.first();

	if(!board->isHead(sam->index) && sam->index != OUT)
		return ko;

	if ( direction == opposite[sam->direction])
		direction = sam->direction;

	if(sam->index == gate || sam->index == OUT )
		direction = N;

	if (sam->index == NORTH_GATE) {
		emit goingOut();
		direction = N;
	}

	int index = board->getNext(direction, sam->index);

	if (board->isApple(index)) {
		grow+=6;
		emit score(true, index);
	}
	else if (!board->isEmpty(index))
		return ko;

	if(growing())
		appendSamy();
	else {
		hold = list.last()->index;
		board->set(hold, empty);
		updateSamy();
	}

	board->set(sam->index, snake);
	reset(index, direction);
	board->set(index, head);

	if ( hold == gate)
		emit closeGate( gate );
	else if ( hold == NORTH_GATE)
		return out;

	return ok;
}

#include "snake.moc"

