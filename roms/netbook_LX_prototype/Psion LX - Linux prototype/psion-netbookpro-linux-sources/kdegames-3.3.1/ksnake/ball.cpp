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

#include "ball.h"
#include "board.h"

int bounce[8][8]={
    { NE, NW, SE, SW, N, E, S, W },
    { SE, SW, NE, NW, S, E, N, W },
    { NW, NE, SW, SE, N, W, S, E },
    { SW, SE, NW, NE, S, W, N, E },
    { NE, NW, SE, SW, N, E, S, W },
    { SE, SW, NE, NW, S, E, N, W },
    { NW, NE, SW, SE, N, W, S, E },
    { SW, SE, NW, NE, S, W, N, E }
};

Ball::Ball(Board *b, PixServer *p)
{
    board = b;
    pixServer = p;

    int i = BoardWidth+1;
    while( !board->isEmpty(i) ) i++;
    hold = index = i;
    board->set(index, Balle);
    next = SE;
}

void Ball::zero()
{
    board->set(index, empty);
    pixServer->erase(index);
}

void Ball::nextMove()
{
    hold = index;
    board->set(hold, empty);

    for ( int x = 0; x < 8 ; x++) {
	int d = bounce[next][x];
	int nextSq = board->getNext(d, index);

	if (board->isHead(nextSq) || board->isEmpty(nextSq)) {
	    next = d;
	    index = nextSq;
	    board->set(index, Balle);
	    break;
	}
    }
}

void Ball::repaint()
{
    static int i = 0;
    static bool rotate = TRUE;

    pixServer->erase(hold);
    pixServer->draw(index, BallPix, i);

    if (rotate)
	if (++i > 3) i=0;

	rotate = !rotate;
}

void KillerBall::nextMove()
{
	hold = index;
	board->set(hold, empty);
	
	// Find the snake.
	int sn = board->samyHeadIndex();
	if(board->isHead(sn)) {
		int nextSq = getNextSquare();
		if(nextSq != -1) {
			next = board->direction(index, nextSq);
			index = nextSq;
			board->set(index, Balle);
			return;
		}
	}
	
	for ( int x = 0; x < 8 ; x++) {
		int d = bounce[next][x];
		int nextSq = board->getNext(d, index);
		
		if (board->isHead(nextSq) || board->isEmpty(nextSq)) {
			next = d;
			index = nextSq;
			board->set(index, Balle);
			break;
		}
	}
}

int KillerBall::getNextSquare()
{
	return board->getNextCloseTo(index, board->samyHeadIndex(), true);
}

int DumbKillerBall::getNextSquare()
{
	return board->getNextCloseToDumb(index, board->samyHeadIndex());
}
