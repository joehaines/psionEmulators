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

#ifndef BOARD_H
#define BOARD_H

enum Square {empty, brick, Apple, Balle, snake, head};
enum Directions {N=0,E=2,S=1,W=3,NE,SE,NW,SW};

#define BoardWidth 35
extern int BRICKSIZE;
extern int MAPWIDTH;
extern int MAPHEIGHT;
#define OUT -1

typedef int Gate;
const int NORTH_GATE = BoardWidth/2;
const int SOUTH_GATE =(BoardWidth*BoardWidth)-(NORTH_GATE+1);

class Pos {
public:
	Pos(Pos *p, int i, int r);
	~Pos();
	int index() const;
	void setPrice(int p);
	int price() const;
	void setParent(Pos *p);
	Pos *parent() const;
	Pos *listNext();
	void addBTree(Pos *np);
	Pos *searchBTree(int i);
	void addFList(Pos *);
	void addList(Pos *np);
private:
	int _index;
	int _price; // Price to reach this position.
	Pos *_parent; // Way to reach destination.
	Pos *next; // Single linked list.
	Pos *fnext; // Link all Pos instances, so we can delete them all.
	Pos *left, *right; // BTree.
	bool inList;
};

class Board : public QMemArray<int>
{
public:
  Board  (int s);
  ~Board() {};
  QRect  rect(int i);

  void   set(int i, Square sq);

  bool   isBrick(int i);
  bool   isEmpty(int i);
  bool   isApple(int i);
  bool   isHead(int i);
  bool   isSnake(int i);

  int    getNext(int, int);
	int    getNextCloseToDumb(int, int);
	int    getNextCloseTo(int, int, bool, int = -1);
	int    direction(int, int);
	
	int    samyHeadIndex() const {
		return(samyIndex);
	}

private:
  void   index(int i);
  bool   inBounds(int i);
  int    row;
  int    col;
  int    sz;

	int    samyIndex;
};

#endif // BOARD_H
