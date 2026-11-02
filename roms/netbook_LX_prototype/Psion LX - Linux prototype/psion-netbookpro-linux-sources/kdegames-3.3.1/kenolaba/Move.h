/* Classes Move, MoveList
 * - represents a move in the game of Abalone
 *
 * Josef Weidendorfer, 28.8.97
*/

#ifndef _MOVE_H_
#define _MOVE_H_

#include <qstring.h>

class Move
{
 public:

	/* Type of move: Moves are searched in this order */
	enum MoveType { out2 = 0, out1with3, out1with2, push2,
		  push1with3, push1with2, move3, left3, right3,
		  left2, right2, move2, move1, none };
        enum { typeCount = none };

        Move() { type = none; }
	Move(short f, char d, MoveType t)
	  { field = f; direction = d, type = t; }		
	

	bool isOutMove() const
	  { return type <= out1with2; }
	bool isPushMove() const
	  { return type <= push1with2; }
	static int maxOutType()
	  { return out1with2; }
	static int maxPushType()
	  { return push1with2; }	       
	static int maxMoveType()
	  { return move1; }	       

	QString name() const;

	void print() const;

	/* Directions */
	enum { Right=1, RightDown, LeftDown, Left, LeftUp, RightUp };
	
	short field;
	unsigned char direction;
	MoveType type;
};

/**
 * Class MoveTypeCounter
 *
 * Used in Board evaluation to count types of board allowed
 * in a position
 */
class MoveTypeCounter
{
 public:
  MoveTypeCounter();
  ~MoveTypeCounter() {}

  void init();
  void incr(int t) { count[t]++; }
  int  get(int t) { return count[t]; }
  int  sum();

 private:
  int count[Move::typeCount];
};

/**
 * Class InARowCounter
 *
 * Used in Board evaluation to count connectiveness
 * of some degrees in a position
 */
class InARowCounter
{
 public:
  enum InARowType { inARow2 = 0, inARow3, inARow4, inARow5, inARowCount };

  InARowCounter();
  ~InARowCounter() {}

  void init();
  void incr(int c) { count[c]++; }
  int  get(int c) { return count[c]; }

 private:
  int count[inARowCount];
};


/* MoveList stores a fixed number of moves (for efficince) 
 * <getNext> returns reference of next move ordered according to type
 * <insert> does nothing if there isn't enough free space
 * 
 * Recommend usage (* means 0 or more times):
 *   [ clear() ; insert() * ; isElement() * ; getNext() * ] *
 */
class MoveList
{
 public:
	MoveList();
	
	enum { MaxMoves = 84 };

	/* for isElement: search for moves starting with 1/2/3 fields */
	enum { all , start1, start2, start3 };
		
	void clear();
	void insert(Move);
	bool isElement(int f);
	bool isElement(Move&, int startType, bool del=false);
	void insert(short f, char d, Move::MoveType t)
	  { insert( Move(f,d,t) ); }
	int getLength()
	  { return nextUnused; }		  
		  
	bool getNext(Move&,int maxType);  /* returns false if no more moves */

 private:
	Move move[MaxMoves];
	int  next[MaxMoves];
	int  first[Move::typeCount];
	int  last[Move::typeCount];
	int  actual[Move::typeCount];
	int  nextUnused, actualType;
};

#endif /* _MOVE_H_ */

