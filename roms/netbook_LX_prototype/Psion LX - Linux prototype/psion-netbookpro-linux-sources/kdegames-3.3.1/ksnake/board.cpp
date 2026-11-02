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

#include <qrect.h>
#include "board.h"

Pos::Pos(Pos *p, int i, int r) {
	_parent = p;
	_index = i;
	_price = r;
	left = right = next = fnext = 0;
	inList = false;
}
Pos::~Pos() {
	delete fnext;
}
int Pos::index() const {
	return(_index);
}
void Pos::setPrice(int p) {
	_price = p;
}
int Pos::price() const {
	return(_price);
}
void Pos::setParent(Pos *p) {
	_parent = p;
}
Pos *Pos::parent() const {
	return(_parent);
}
Pos *Pos::listNext() {
	inList = false;
	return(next);
}
void Pos::addBTree(Pos *np) {
	// Check direction np is going to.
	Pos **p = 0;
	if(np->index() < index())
		p = &left;
	else if(np->index() > index())
		p = &right;
	else {
		qFatal("Repeated nodes on btree should never happens");
	}
	
	if(! *p) {
		*p = np;
	}
	else {
		(*p)->addBTree(np);
	}
}
Pos *Pos::searchBTree(int i) {
	if(i == index()) {
		return(this);
	}
	else if(i < index() && left) {
		return(left->searchBTree(i));
	}
	else if(right) {
		return(right->searchBTree(i));
	}
	// Node not found.
	return(0);
}
void Pos::addFList(Pos *np) {
	np->fnext = fnext;
	fnext = np;
}
void Pos::addList(Pos *np) {
	if(np->inList)
		return; // We're already in list.
	
	np->inList = true;
	Pos *p, *n;
	for(p = this; p; p = n) {
		// Check if the node next to p has a higher price.
		n = p->next;
		if(! n) {
			// The new node go to tail.
			np->next = 0;
			p->next = np;
			return;
		}
		
		if(np->price() <= n->price()) {
			// Add new node after p.
			np->next = p->next;
			p->next = np;
			return;
		}
	}
	qFatal("Shouldn't reach this point");
}

Board::Board(int s)
  :QMemArray<int> (s)
{
  sz = s;
	samyIndex = -1;
}

void Board::index(int i)
{
  row = i/BoardWidth;
  col = i-(row*BoardWidth);
}

bool Board::inBounds(int i)
{
  return ( i < 0 || i > sz-1 ? FALSE : TRUE);
}

void Board::set(int i, Square sq)
{
  if (inBounds(i))
    at(i) = sq;
	if(sq == head)
		samyIndex = i;
}

QRect Board::rect(int i)
{
  index(i);
  return (QRect(col*BRICKSIZE, row*BRICKSIZE, BRICKSIZE, BRICKSIZE));
}

bool Board::isEmpty(int i)
{
  if (inBounds(i))
    return (at(i) == empty ? TRUE : FALSE);
  return TRUE;
}

bool Board::isBrick(int i)
{
  if (inBounds(i))
    return (at(i) == brick ? TRUE : FALSE);
  return FALSE;
}

bool Board::isApple(int i)
{
  if (inBounds(i))
    return (at(i) == Apple ? TRUE : FALSE);
  return FALSE;
}

bool Board::isHead(int i)
{
  if (inBounds(i))
    return (at(i) == head ? TRUE : FALSE);
  return FALSE;
}

bool Board::isSnake(int i)
{
  if (inBounds(i))
    return (at(i) == snake ? TRUE : FALSE);
  return FALSE;
}

int Board::getNext(int n, int i)
{
  index(i);

  switch(n)
    {
    case NW:
      return( i >= BoardWidth && col > 0 ? (i-BoardWidth)-1 : OUT);
    case N:
      return( i >= BoardWidth ? i-BoardWidth : OUT );
    case NE:
      return( i >= BoardWidth && col < BoardWidth-1 ? (i-BoardWidth)+1 : OUT);
    case W:
      return(col > 0 ? i-1 : OUT );
    case E:
      return(col < BoardWidth-1 ? i+1 : OUT );
    case SW:
      return( row < sz-BoardWidth && col > 0 ? (i+BoardWidth)-1 : OUT);
    case S:
      return( row < sz-BoardWidth ? i+BoardWidth : OUT );
    case SE:
      return( row < sz-BoardWidth && col < BoardWidth-1 ? (i+BoardWidth)+1 : OUT);
    default:
      return OUT;
    }
}

int Board::getNextCloseToDumb(int s, int d)
{
	if(s == d)
		return(-1); // What can I say, we're here! ;o)
	
	int nextSq = getNext(direction(s, d), s);
	
	if(! isEmpty(nextSq))
		return(-1);
	
	return(nextSq);
}

int Board::getNextCloseTo(int s, int d, bool diag, int lastIndex)
{
	if(s == d)
		return(-1); // What can I say, we're here! ;o)
	
	const int firstN = diag ? 4 : 0;
	const int lastN = diag ? 8 : 4;
	
	Pos *root = new Pos(0, s, 0), *list = root;
	
	// List of indexes.
	for(; list; list = list->listNext()) {
		Pos *p;
		
		// Check if current list node is the destination position.
		if(list->index() == d) {
			// Find first movement after root.
			for(; ; list = p) {
				p = list->parent();
				if(p == root) {
					// This is our move.
					int nextSq = list->index();
					delete root;
					index(nextSq);
					return(nextSq);
				}
			}
			qFatal("Never here");
		}
		
		// Make possible moves.
		for(int n = firstN; n < lastN; n ++) {
			int i = getNext(n, list->index());
			int pri = list->price() + 1;
			
			// getNext returned valid place?
			if(! inBounds(i) || (! isEmpty(i) && i != d)) {
				// Or place is out of map or it's not empty,
				// so go to the next possible move.
				continue;
			}
			
			int pi = list->parent() ? list->parent()->index() : lastIndex;
			if(pi != -1 && direction(pi, list->index()) !=
					direction(list->index(), i)) {
				pri += 10;
			}
			
			// Check if position wasn't processed yet.
			if( (p = root->searchBTree(i))) {
				// Position already processed.
				// Check price of found position with current one.
				if(p->price() > pri) {
					// We found a cheapear way to reach the same
					// place, so let's change the parent and price of p.
					p->setPrice(list->price() + 1);
					p->setParent(list);
					list->addList(p);
				}
				continue;
			}
			
			// Create new Pos class instance.
			p = new Pos(list, i, pri);
			
			// Add.
			list->addList(p);
			root->addFList(p);
			root->addBTree(p);
		}
	}
	
	// Solution not found.
	delete root;
	return(-1);
}

int Board::direction(int s, int d)
{
	index(s);
	int scol = col, srow = row;
	index(d);
	if(scol > col) { // Left.
		if(srow < row)
			return(SW);
		else if(srow > row)
			return(NW);
		else
			return(W);
	}
	else if(scol < col) { // Right.
		if(srow < row)
			return(SE);
		else if(srow > row)
			return(NE);
		else
			return(E);
	}
	else { // X's the same.
		if(srow < row)
			return(S);
		else
			return(N);
	}
}
