/* Class Board
 *
 *   with methods for
 *    - play/take back moves
 *    - generate allowed moves
 *    - calculate rating for position
 *    - search for best move
 *
 * Josef Weidendorfer, 28.8.97
*/

#include <stdio.h>

#include <qdatetime.h>
#include <qstrlist.h>

#include <kconfig.h>
#include <krandomsequence.h>

#include "Board.h"
#include "EvalScheme.h"

// #define MYTRACE 1

#if 0
#define CHECK(b)  Q_ASSERT(b)
#else
#define CHECK(b)
#endif


static int ratedPositions, wonPositions, searchCalled, moveCount;
static int normalCount, pushCount, outCount, cutoffCount;

/*********************** Class PrincipalVariation *************************/

void PrincipalVariation::clear(int d)
{
	int i,j;

	for(i=0;i<maxDepth;i++)
	  for(j=0;j<maxDepth;j++) {
		  move[i][j].type = Move::none;
	  }
	actMaxDepth = (d<maxDepth) ? d:maxDepth-1;
}

void PrincipalVariation::update(int d, Move& m)
{
	int i;

	if (d>actMaxDepth) return;
	for(i=d+1;i<=actMaxDepth;i++) {
		move[d][i]=move[d+1][i];
		move[d+1][i].type = Move::none;
	}
	move[d][d]=m;
}



/****************************** Class Board ****************************/


/* Board for start of a game */
int Board::startBoard[]={
                       10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	             10,  1,  1,  1,  1,  1, 10, 10, 10, 10, 10,
	           10,  1,  1,  1,  1,  1,  1, 10, 10, 10, 10,
                 10,  0,  0,  1,  1,  1,  0,  0, 10, 10, 10,
	       10,  0,  0,  0,  0,  0,  0,  0,  0, 10, 10,
             10,  0,  0,  0,  0,  0,  0,  0,  0,  0, 10,
           10, 10,  0,  0,  0,  0,  0,  0,  0,  0, 10,
         10, 10, 10,  0,  0,  2,  2,  2,  0,  0, 10,
       10, 10, 10, 10,  2,  2,  2,  2,  2,  2, 10,
     10, 10, 10, 10, 10,  2,  2,  2,  2,  2, 10,
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10          };


/* first centrum of board, then rings around (numbers are indexes) */
int Board::order[]={
          60,
	  61,72,71,59,48,49,
	  62,73,84,83,82,70,58,47,36,37,38,50,
	  63,74,85,96,95,94,93,81,69,57,46,35,24,25,26,27,39,51,
	  64,75,86,97,108,107,106,105,104,92,80,68,56,45,34,23,12,
	  13,14,15,16,28,40,52 };

/* See EvalScheme.{h|cpp}
 *
  // Ratings for fields are calculated out of these values
  // (see setFieldValues)
  int Board::ringValue[] = { 45, 35, 25, 10, 0 };
  int Board::ringDiff[]  = {  0, 10, 10,  8, 5 };

  // Value added to board rating according to the difference of
  // stones in game of player1 and player2
  int Board::stoneValue[]= { 0,-800,-1800,-3000,-4400,-6000 };

  // Default Values for moves values (see Move.h)
  int Board::moveValue[]= { 40,30,30, 15,14,13, 5,5,5, 2,2,2, 1 };

  // Default Values for inARow values (see Move.h)
  int Board::inARowValue[]= { 2, 5, 4, 3 };
*/

int Board::fieldValue[61];
int Board::direction[]= { -11,1,12,11,-1,-12,-11,1 };


Board::Board()
{
  clear();
  breakOut = bUpdateSpy = false;
  spyLevel = 0;
  spyDepth = 0;
  debug = 0;
  realMaxDepth = 1;
  _evalScheme = 0;
}

void Board::setEvalScheme(EvalScheme* scheme)
{
  if (!scheme)
    scheme = new EvalScheme( QString("Default") );

  _evalScheme = scheme;
  setFieldValues();
}

void Board::setFieldValues()
{
  if (!_evalScheme) return;

  int i, j = 0, k = 59;
  int ringValue[5], ringDiff[5];

  for(i=0;i<5;i++) {
    ringDiff[i]  = _evalScheme->ringDiff(i);
    ringValue[i] = _evalScheme->ringValue(i);
    if (ringDiff[i]<1) ringDiff[i]=1;
  }

  fieldValue[0] = ringValue[0];
  for(i=1;i<7;i++)
    fieldValue[i] = ringValue[1] + ((j+=k) % ringDiff[1]);
  for(i=7;i<19;i++)
    fieldValue[i] = ringValue[2] + ((j+=k) % ringDiff[2]);
  for(i=19;i<37;i++)
    fieldValue[i] = ringValue[3] + ((j+=k) % ringDiff[3]);
  for(i=37;i<61;i++)
    fieldValue[i] = ringValue[4] + ((j+=k) % ringDiff[4]);
}



void Board::begin(int startColor)
{
  int i;

  for(i=0;i<AllFields;i++)
    field[i] = startBoard[i];
  storedFirst = storedLast = 0;
  color = startColor;
  color1Count = color2Count = 14;
  random.setSeed(0); // Initialize random sequence
}

void Board::clear()
{
  int i;

  for(i=0;i<AllFields;i++)
    field[i] = (startBoard[i] == out) ? out: free;
  storedFirst = storedLast = 0;
  color1Count = color2Count = 0;
}

/* generate moves starting at field <startField> */
void Board::generateFieldMoves(int startField, MoveList& list)
{
  int d, dir, c, actField;
  bool left, right;
  int opponent = (color == color1) ? color2 : color1;

  Q_ASSERT( field[startField] == color );

  /* 6 directions	*/
  for(d=1;d<7;d++) {
    dir = direction[d];

    /* 2nd field */
    c = field[actField = startField+dir];
    if (c == free) {
      /* (c .) */
      list.insert(startField, d, Move::move1);
      continue;
    }
    if (c != color)
      continue;

    /* 2nd == color */

    left = (field[startField+direction[d-1]] == free);
    if (left) {
      left = (field[actField+direction[d-1]] == free);
      if (left)
	/* 2 left */
	list.insert(startField, d, Move::left2);
    }

    right = (field[startField+direction[d+1]] == free);
    if (right) {
      right = (field[actField+direction[d+1]] == free);
      if (right)
	/* 2 right */
	list.insert(startField, d, Move::right2);
    }

    /* 3rd field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c .) */
      list.insert(startField, d, Move::move2);
      continue;
    }
    else if (c == opponent) {

      /* 4th field */
      c = field[actField += dir];
      if (c == free) {
	/* (c c o .) */
	list.insert(startField, d, Move::push1with2);
      }
      else if (c == out) {
	/* (c c o |) */
	list.insert(startField, d, Move::out1with2);
      }
      continue;
    }
    if (c != color)
      continue;

    /* 3nd == color */

    if (left) {
      if (field[actField+direction[d-1]] == free)
	/* 3 left */
	list.insert(startField, d, Move::left3);
    }

    if (right) {
      if (field[actField+direction[d+1]] == free)
	/* 3 right */
	list.insert(startField, d, Move::right3);
    }

    /* 4th field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c .) */
      list.insert(startField, d, Move::move3);
      continue;
    }
    if (c != opponent)
      continue;

    /* 4nd == opponent */

    /* 5. field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c o .) */
      list.insert(startField, d, Move::push1with3);
      continue;
    }
    else if (c == out) {
      /* (c c c o |) */
      list.insert(startField, d, Move::out1with3);
      continue;
    }
    if (c != opponent)
      continue;

    /* 5nd == opponent */

    /* 6. field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c o o .) */
      list.insert(startField, d, Move::push2);
    }
    else if (c == out) {
      /* (c c c o o |) */
      list.insert(startField, d, Move::out2);
    }
  }
}


void Board::generateMoves(MoveList& list)
{
	int actField, f;

	for(f=0;f<RealFields;f++) {
		actField = order[f];
		if ( field[actField] == color)
		   generateFieldMoves(actField, list);
	}
}



void Board::playMove(const Move& m)
{
	int f, dir, dir2;
	int opponent = (color == color1) ? color2:color1;

	CHECK( isConsistent() );

	if (++storedLast == MvsStored) storedLast = 0;

	/* Buffer full -> delete oldest entry */
	if (storedLast == storedFirst)
	  	if (++storedFirst == MvsStored) storedFirst = 0;

	storedMove[storedLast] = m;

	f = m.field;
	CHECK( (m.type >= 0) && (m.type < Move::none));
	CHECK( field[f] == color );
	field[f] = free;
	dir = direction[m.direction];

	switch(m.type) {
	 case Move::out2:        /* (c c c o o |) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == opponent );
		CHECK( field[f + 5*dir] == out );
		field[f + 3*dir] = color;
		break;
	 case Move::out1with3:   /* (c c c o |)   */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == out );
		field[f + 3*dir] = color;
		break;
	 case Move::move3:       /* (c c c .)     */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == free );
		field[f + 3*dir] = color;
		break;
	 case Move::out1with2:   /* (c c o |)     */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == opponent );
		CHECK( field[f + 3*dir] == out );
		field[f + 2*dir] = color;
		break;
	 case Move::move2:       /* (c c .)       */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == free );
		field[f + 2*dir] = color;
		break;
	 case Move::push2:       /* (c c c o o .) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == opponent );
		CHECK( field[f + 5*dir] == free );
		field[f + 3*dir] = color;
		field[f + 5*dir] = opponent;
		break;
	 case Move::left3:
		dir2 = direction[m.direction-1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		CHECK( field[f + 2*dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::right3:
		dir2 = direction[m.direction+1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		CHECK( field[f + 2*dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::push1with3:   /* (c c c o .) => (. c c c o) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == color );
		CHECK( field[f + 3*dir] == opponent );
		CHECK( field[f + 4*dir] == free );
		field[f + 3*dir] = color;
		field[f + 4*dir] = opponent;
		break;
	 case Move::push1with2:   /* (c c o .) => (. c c o) */
		CHECK( field[f + dir] == color );
		CHECK( field[f + 2*dir] == opponent );
		CHECK( field[f + 3*dir] == free );
		field[f + 2*dir] = color;
		field[f + 3*dir] = opponent;
		break;
	 case Move::left2:
		dir2 = direction[m.direction-1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::right2:
		dir2 = direction[m.direction+1];
		CHECK( field[f + dir] == color );
		CHECK( field[f + dir2] == free );
		CHECK( field[f + dir+dir2] == free );
		field[f+dir2] = color;
		field[f+=dir] = free;
		field[f+dir2] = color;
		break;
	 case Move::move1:       /* (c .) => (. c) */
		CHECK( field[f + dir] == free );
		field[f + dir] = color;
		break;
	default:
	  break;
	}

	if (m.isOutMove()) {
		if (color == color1)
		  color2Count--;
		else
		  color1Count--;
	}

	/* change actual color */
	color = opponent;

	CHECK( isConsistent() );

}

bool Board::takeBack()
{
  int f, dir, dir2;
  int opponent = color;
  Move& m = storedMove[storedLast];

  CHECK( isConsistent() );

  if (storedFirst == storedLast) return false;

  /* change actual color */
  color = (color == color1) ? color2:color1;

  if (m.isOutMove()) {
    if (color == color1)
      color2Count++;
    else
      color1Count++;
  }

  f = m.field;
  CHECK( field[f] == free );
  field[f] = color;
  dir = direction[m.direction];

  switch(m.type) {
  case Move::out2:        /* (. c c c o |) => (c c c o o |) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == opponent );
    CHECK( field[f + 5*dir] == out );
    field[f + 3*dir] = opponent;
    break;
  case Move::out1with3:   /* (. c c c |) => (c c c o |) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == out );
    field[f + 3*dir] = opponent;
    break;
  case Move::move3:       /* (. c c c) => (c c c .)     */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    field[f + 3*dir] = free;
    break;
  case Move::out1with2:   /* (. c c | ) => (c c o |)     */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == out );
    field[f + 2*dir] = opponent;
    break;
  case Move::move2:       /* (. c c) => (c c .)       */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    field[f + 2*dir] = free;
    break;
  case Move::push2:       /* (. c c c o o) => (c c c o o .) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == opponent );
    CHECK( field[f + 5*dir] == opponent );
    field[f + 3*dir] = opponent;
    field[f + 5*dir] = free;
    break;
  case Move::left3:
    dir2 = direction[m.direction-1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + 2*dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    CHECK( field[f + 2*dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::right3:
    dir2 = direction[m.direction+1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + 2*dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    CHECK( field[f + 2*dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::push1with3:   /* (. c c c o) => (c c c o .) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == color );
    CHECK( field[f + 4*dir] == opponent );
    field[f + 3*dir] = opponent;
    field[f + 4*dir] = free;
    break;
  case Move::push1with2:   /* (. c c o) => (c c o .) */
    CHECK( field[f + dir] == color );
    CHECK( field[f + 2*dir] == color );
    CHECK( field[f + 3*dir] == opponent );
    field[f + 2*dir] = opponent;
    field[f + 3*dir] = free;
    break;
  case Move::left2:
    dir2 = direction[m.direction-1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::right2:
    dir2 = direction[m.direction+1];
    CHECK( field[f + dir] == free );
    CHECK( field[f + dir2] == color );
    CHECK( field[f + dir+dir2] == color );
    field[f+dir2] = free;
    field[f+=dir] = color;
    field[f+dir2] = free;
    break;
  case Move::move1:       /* (. c) => (c .) */
    CHECK( field[f + dir] == color );
    field[f + dir] = free;
    break;
  default:
    break;
  }

  if (--storedLast < 0) storedLast = MvsStored-1;

  CHECK( isConsistent() );

  return true;
}

int Board::movesStored()
{
  int c = storedLast - storedFirst;
  if (c<0) c+= MvsStored;
  return c;
}


/** countFrom
 *
 * Used for board evaluation to count allowed move types and
 * connectiveness. VERY similar to move generation.
 *
 * Returns number of found moves
 */
void Board::countFrom(int startField, int color,
		      MoveTypeCounter& TCounter,
		      InARowCounter& CCounter)
{
  int d, dir, c, actField, c2;
  bool left, right;

  /* 6 directions	*/
  for(d=1;d<7;d++) {
    dir = direction[d];

    /* 2nd field */
    c = field[actField = startField+dir];
    if (c == free) {
      TCounter.incr( Move::move1 );
      continue;
    }

    if (c != color)
      continue;

    /* 2nd == color */

    CCounter.incr( InARowCounter::inARow2 );

    /* left side move 2 */
    left = (field[startField+direction[d-1]] == free);
    if (left) {
      left = (field[actField+direction[d-1]] == free);
      if (left)
	TCounter.incr( Move::left2 );
    }

    /* right side move 2 */
    right = (field[startField+direction[d+1]] == free);
    if (right) {
      right = (field[actField+direction[d+1]] == free);
      if (right)
	TCounter.incr( Move::right2 );
    }

    /* 3rd field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c .) */
      TCounter.incr( Move::move2 );
      continue;
    }
    else if (c == out) {
      continue;
    }
    else if (c != color) {

      /* 4th field */
      c = field[actField += dir];
      if (c == free) {
	/* (c c o .) */
	TCounter.incr( Move::push1with2 );
      }
      else if (c == out) {
	/* (c c o |) */
	TCounter.incr( Move::out1with2 );
      }
      continue;
    }

    /* 3nd == color */

    CCounter.incr( InARowCounter::inARow3 );

    /* left side move 3 */
    if (left) {
      left = (field[actField+direction[d-1]] == free);
      if (left)
	TCounter.incr( Move::left3 );
    }

    /* right side move 3 */
    if (right) {
      right = (field[actField+direction[d+1]] == free);
      if (right)
	TCounter.incr( Move::right3 );
    }

    /* 4th field */
    c = field[actField += dir];
    if (c == free) {
      /* (c c c .) */
      TCounter.incr( Move::move3 );
      continue;
    }
    else if (c == out) {
      continue;
    }
    else if (c != color) {

      /* 4nd == opponent */

      /* 5. field */
      c2 = field[actField += dir];
      if (c2 == free) {
	/* (c c c o .) */
	TCounter.incr( Move::push1with3 );
	continue;
      }
      else if (c2 == out) {
	/* (c c c o |) */
	TCounter.incr( Move::out1with3 );
	continue;
      }
      if (c2 != c)
	continue;

      /* 5nd == opponent */

      /* 6. field */
      c2 = field[actField += dir];
      if (c2 == free) {
	/* (c c c o o .) */
	TCounter.incr( Move::push2 );
      }
      else if (c2 == out) {
	/* (c c c o o |) */
	TCounter.incr( Move::out2 );
      }

      continue;
    }

    /* 4nd == color */

    CCounter.incr( InARowCounter::inARow4 );

    /* 5th field */
    c = field[actField += dir];
    if (c != color)
      continue;

    /* 4nd == color */

    CCounter.incr( InARowCounter::inARow5 );
  }
}

/** indent
 *
 * Internal: for debugging output only
 */
void indent(int d)
{
	char tmp[]="                                        ";
	tmp[d*3] = 0;
	printf(">        %s",tmp);
}



/** validState
 *
 * Check for a valid board position to play from:
 * (1) Number of balls for each color has to be between 9 and 14
 * (2) There must be a move possible for actual color
 */
int Board::validState()
{
  MoveTypeCounter tc;
  InARowCounter  cc;

  int c1 = 0, c2 = 0;
  int i,j, moveCount, res;

  for(i=0;i<AllFields;i++) {
    j=field[i];
    if (j == color1) c1++;
    if (j == color2) c2++;
    if (j == color)
      countFrom( i, color, tc, cc);
  }

  color1Count = c1;
  color2Count = c2;
  moveCount = tc.sum();

  if (c1==0 && c2==0)
    res = empty;
  else if (c1>8 && c1<15 && c2>8 && c2<15 && moveCount>0 )
    res = valid;
  else
    res = invalid;

#ifdef MYTRACE
  if (spyLevel>2) {
    indent(spyDepth);
    printf("Valid: %s (Color1 %d, Color2 %d, moveCount of %d: %d)\n",
	   (res == empty) ? "empty" : (res==valid) ? "valid":"invalid",
	   c1,c2,color,moveCount);
  }
#endif

  return res;
}



/* Calculate a evaluation for actual position
 *
 * A higher value means a better position for opponent
 * NB: This means a higher value for better position of
 *     'color before last move'
 */
int Board::calcEvaluation()
{
  MoveTypeCounter tcColor, tcOpponent;
  InARowCounter  ccColor, ccOpponent;

  int f,i,j;

  /* different evaluation types */
  int fieldValueSum=0, stoneValueSum=0;
  int moveValueSum=0, inARowValueSum=0;
  int valueSum;

  /* First check simple winner condition */
  if (color1Count <9)
    valueSum = (color==color1) ? 16000 : -16000;
  else if (color2Count <9)
    valueSum = (color==color2) ? 16000 : -16000;
  else {

    /* Calculate fieldValueSum and count move types and connectivity */
    for(i=0;i<RealFields;i++) {
      j=field[f=order[i]];
      if (j == free) continue;
      if (j == color) {
	countFrom( f, j, tcColor, ccColor );
	fieldValueSum -= fieldValue[i];
      }
      else {
	countFrom( f, j, tcOpponent, ccOpponent );
	fieldValueSum += fieldValue[i];
      }
    }

    /* If color can't do any moves, opponent wins... */
    if (tcColor.sum() == 0)
      valueSum = 16000;
    else {

      for(int t=0;t < Move::typeCount;t++)
	moveValueSum += _evalScheme->moveValue(t) *
	  (tcOpponent.get(t) - tcColor.get(t));

      for(int i=0;i < InARowCounter::inARowCount;i++)
	inARowValueSum += _evalScheme->inARowValue(i) *
	  (ccOpponent.get(i) - ccColor.get(i));

      if (color == color2)
	stoneValueSum = _evalScheme->stoneValue(14 - color1Count) -
	  _evalScheme->stoneValue(14 - color2Count);
      else
	stoneValueSum = _evalScheme->stoneValue(14 - color2Count) -
	  _evalScheme->stoneValue(14 - color1Count);

      valueSum = fieldValueSum + moveValueSum +
	inARowValueSum + stoneValueSum;
    }
  }

#ifdef MYTRACE
  if (spyLevel>2) {
    indent(spyDepth);
    printf("Eval %d (field %d, move %d, inARow %d, stone %d)\n",
	   valueSum, fieldValueSum, moveValueSum,
	   inARowValueSum, stoneValueSum );
  }
#endif

  return valueSum;
}

bool Board::isConsistent()
{
  int c1 = 0, c2 = 0;
  int i,j;

  for(i=0;i<RealFields;i++) {
    j=field[order[i]];
    if (j == color1) c1++;
    if (j == color2) c2++;
  }
  return (color1Count == c1 && color2Count == c2);
}

void Board::changeEvaluation()
{
  int i,tmp;

  /* innermost ring */
  tmp=fieldValue[1];
  for(i=1;i<6;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[6] = tmp;

  tmp=fieldValue[7];
  for(i=7;i<18;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[18] = tmp;

  tmp=fieldValue[19];
  for(i=19;i<36;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[36] = tmp;

  /* the outermost ring */
  tmp=fieldValue[37];
  for(i=37;i<60;i++)
    fieldValue[i] = fieldValue[i+1];
  fieldValue[60] = tmp;
}

/*
void Board::showHist()
{
	Move m1, m2;

	printf("After playing ");
	(m1=lastMove()).print();
	print();
	printf("TakeBack "); m1.print();
	takeBack(); print();
	printf("TakeBack ");
	(m2=lastMove()).print();
	takeBack(); print();
	printf("Play "); m2.print();
	playMove(m2); print();
	printf("Play "); m1.print();
	playMove(m1); print();
	getchar();
}
*/



/*
 * We always try the maximize the board value
 */
int Board::search(int depth, int alpha, int beta)
{
	int actValue= -14999+depth, value;
	Move m;
	MoveList list;
	bool depthPhase, doDepthSearch;

	searchCalled++;

	/* We make a depth search for the following move types... */
	int maxType = (depth < maxDepth-1)  ? Move::maxMoveType() :
	              (depth < maxDepth)    ? Move::maxPushType() :
	                                      Move::maxOutType();

	generateMoves(list);

#ifdef MYTRACE

        int oldRatedPositions;
        int oldWonPositions;
        int oldSearchCalled;
        int oldMoveCount;
        int oldNormalCount;
        int oldPushCount;
        int oldOutCount;
        int oldCutoffCount;

	spyDepth = depth;

	moveCount += list.getLength();

	/*
	  if (spyLevel>1) {
	  indent(depth);
	  printf("%s (%6d .. %6d) MaxType %s\n", depth,
	  (color==color1)?"O":"X", alpha,beta,
		 (depth < maxDepth-1) ? "Moving" :
		 (depth < maxDepth)? "Pushing" : "PushOUT" );
	}
	*/
#endif

	/* check for a old best move in main combination */
	if (inPrincipalVariation) {
	  m = pv[depth];

	  if ((m.type != Move::none) &&
              (!list.isElement(m, 0, true)))
	    m.type = Move::none;

          if (m.type == Move::none)
	    inPrincipalVariation = false;

#ifdef MYTRACE
          else {
	    if (spyLevel>1) {
	      indent(spyDepth);
	      printf("Got from pv !\n" );
	    }
	  }
#endif
        }

        // first, play all moves with depth search
        depthPhase = true;

        while (1) {

          // get next move
          if (m.type == Move::none) {
            if (depthPhase)
              depthPhase = list.getNext(m, maxType);
            if (!depthPhase)
              if (!list.getNext(m, Move::none)) break;
          }
          // we could start with a non-depth move from principal variation
          doDepthSearch = depthPhase && (m.type <= maxType);

#ifdef MYTRACE

	  if (m.isOutMove()) outCount++;
	  else if (m.isPushMove()) pushCount++;
	  else normalCount++;

          if (doDepthSearch) {
            oldRatedPositions = ratedPositions;
            oldWonPositions = wonPositions;
            oldSearchCalled = searchCalled;
            oldMoveCount = moveCount;
            oldNormalCount = normalCount;
            oldPushCount = pushCount;
            oldOutCount = outCount;
            oldCutoffCount = cutoffCount;

            if (spyLevel>1) {
              indent(spyDepth);
              printf("%s [%6d .. %6d] ",
                     (color==color1)?"O":"X", alpha,beta);
              m.print();
              printf("\n");
            }

#ifdef SPION
            if (bUpdateSpy) emit update(depth, 0, m, false);
#endif
          }
#endif

	  playMove(m);
	  if (!isValid()) {
	    /* Possibility (1) to win: Ball Count <9 */
	    value = 14999-depth;
	    //  value = ((depth < maxDepth) ? 15999:14999) - depth;
#ifdef MYTRACE
	    wonPositions++;
#endif
	  }
	  else {

            if (doDepthSearch) {
              /* opponent searches for his maximum; but we want the
               * minimum: so change sign (for alpha/beta window too!)
               */
              value = - search(depth+1,-beta,-alpha);
            }
            else {
              ratedPositions++;

              value = calcEvaluation();
            }
	  }
	  takeBack();

          /* For GUI response */
          if (doDepthSearch && (maxDepth - depth >2))
            emit searchBreak();

#ifdef MYTRACE

          if (doDepthSearch) {
            spyDepth = depth;

            if (spyLevel>1) {

              indent(spyDepth);
              if (oldSearchCalled < searchCalled) {
                printf("  %d Calls", searchCalled-oldSearchCalled);
                if (cutoffCount>oldCutoffCount)
                  printf(" (%d Cutoffs)", cutoffCount-oldCutoffCount);
                printf(", GenMoves %d (%d/%d/%d played)",
                       moveCount - oldMoveCount,
                       normalCount - oldNormalCount,
                       pushCount-oldPushCount,
                       outCount-oldOutCount);
                printf(", Rate# %d",
                       ratedPositions+wonPositions
                       - oldRatedPositions - oldWonPositions);
                if (wonPositions > oldWonPositions)
                  printf(" (%d Won)", wonPositions- oldWonPositions);
                printf("\n");
                indent(spyDepth);
              }

              printf("  => Rated %d%s\n",
                     value,
                     (value>14900)  ? ", WON !":
                     (value>=beta)  ? ", CUTOFF !":
                     (value>actValue)  ? ", Best !": "");
            }
          }
          else {
            if (spyLevel>2) {
              indent(spyDepth);
              printf("%s (%6d .. %6d) %-25s => Rating %6d%s\n",
                     (color==color1)?"O":"X", alpha,beta,
                     m.name().latin1(),
                     value,
                     (value>14900)  ? ", WON !":
                     (value>=beta)  ? ", CUTOFF !":
                     (value>actValue)  ? ", Best !": "");
            }
          }

          if (value>=beta) cutoffCount++;
#endif

#ifdef SPION
	  if (bUpdateSpy) {
	    if (value > actValue)
	      emit updateBest(depth, value, m, value >= beta);
	    emit update(depth, value, m, true);
	  }
#endif
	  if (value > actValue) {
	    actValue = value;
	    pv.update(depth, m);

            // Only update best move if not stopping search
            if (!breakOut && (depth == 0)) {
              _bestMove = m;

              if (bUpdateSpy) {
                emit updateBestMove(m, actValue);
#ifdef MYTRACE
                if (spyLevel>0) {
                  int i;
                  printf(">      New pv (Rating %d):", actValue);
                  for(i=0;i<=maxDepth;i++) {
                    printf("\n>          D %d: %s",
                           i, pv[i].name().latin1() );
                  }
                  printf("\n>\n");
                }
#endif
              }
            }

	    if (actValue>14900 || actValue >= beta)
	      return actValue;

	    /* maximize alpha */
	    if (actValue > alpha) alpha = actValue;
	  }

          if (breakOut) depthPhase=false;
          m.type = Move::none;
	}

	return actValue;
}


Move& Board::bestMove()
{
	int alpha=-15000,beta=15000;
	int nalpha,nbeta, actValue;

	if (!_evalScheme) {
	  // Use default values if not set
	  setEvalScheme();
	}

	pv.clear(realMaxDepth);
        _bestMove.type = Move::none;

	maxDepth=1;
	show = false;
	breakOut = false;
	spyDepth = 0;

	if (spyLevel>0)
	  printf("\n> New Search\n>\n");

	do {
	  if (spyLevel>0)
	    printf(">   MaxDepth: %d\n>\n", maxDepth);

		// ShowTiefe(maxtiefe);
		do {
		  if (spyLevel>0)
		    printf(">     AB-Window: (%d ... %d)\n>\n", alpha, beta);

		  nalpha=alpha, nbeta=beta;
		  inPrincipalVariation = (pv[0].type != Move::none);

		  /* Statistics */
		  searchCalled = 0;
		  moveCount = 0;
		  ratedPositions = 0;
		  wonPositions = 0;
		  normalCount = 0;
		  pushCount = 0;
		  outCount = 0;
		  cutoffCount = 0;

		  actValue = search(0,alpha,beta);

		  if (spyLevel>0)
		    {
		      int i;
		      if (spyLevel>1)
			printf(">\n");
		      printf(">      Got PV with Rating %d:",actValue);
		      for(i=0;i<=maxDepth;i++) {
			printf("\n>          D %d: ", i);
			pv[i].print();
		      }
		      printf("\n>\n");

		      printf(">      Search called    : %6d / %d Cutoffs\n",
			     searchCalled, cutoffCount);
		      printf(">       Moves generated : %6d / %d Played\n",
			     moveCount, normalCount+pushCount+outCount);
		      printf(">        Nrml/Push/Out  : %6d / %d / %d\n",
			     normalCount,pushCount,outCount);
		      printf(">       Positions rated : %6d / %d Won\n>\n",
			     ratedPositions+wonPositions, wonPositions);

		    }

                  if (actValue > 14900 || actValue < -14900)
		    breakOut=true;

		  /* Don't break out if we haven't found a move */
		  if (_bestMove.type == Move::none)
		    breakOut=false;

                  // widen alpha-beta window if needed
		  if (actValue <= nalpha) {
		    alpha = -15000;
                    if (beta<15000) beta=actValue+1;
                  }
		  if (actValue >= nbeta) {
                    if (alpha > -15000) alpha = actValue-1;
                    beta=15000;
                  }
		}
		while(!breakOut && (actValue<=nalpha || actValue>=nbeta));

		/* Window in both directions cause of deepening */
		alpha=actValue-200, beta=actValue+200;
		/*
		  if ( (maxDepth+((color == color2)?1:0)) %2 ==1)
		  alpha=actValue-200, beta=actValue+1;
		  else
		  alpha=actValue-1, beta=actValue+200;
		*/
		// ShowHv2();
		/*
		  if (spyLevel>0) {
		  printf(">>> Best Move so far: %d - ", actValue);
		  pv[0].print();
		  printf("\n");
		  }
		*/
	}
	while(++maxDepth< realMaxDepth && !breakOut);

	/* If Spy is On, we want replayable search: don't change rating! */
	if (spyLevel==0)
	  changeEvaluation();
	else {
	  printf(">>> Got Move : ");
	  pv[0].print();
	  printf("\n\n");
	}

	spyDepth = 0;

	return _bestMove;
}

Move Board::randomMove()
{
	Move m;
	MoveList list;

	generateMoves(list);
	int l = list.getLength();

	int j = random.getLong(l) +1;

	while(j != 0) {
		list.getNext(m, Move::none);
		j--;
	}

	return m;
}


void Board::print(int )
{
	int row,i;
	char spaces[]="      ";
	const char *z[]={". ","O ","X ", "o ", "x "};

	printf("\n       -----------\n");
	for(row=0;row<4;row++) {
		printf("%s/ ",spaces+row);
		for(i=0;i<5+row;i++) printf("%s",z[field[row*11+12+i]]);
		printf("\\\n");
	}
	printf("  | ");
	for(i=0;i<9;i++) printf("%s",z[field[56+i]]);
	printf("|\n");
	for(row=0;row<4;row++) {
		printf("%s\\ ",spaces+3-row);
		for(i=0;i<8-row;i++) printf("%s",z[field[68+row*12+i]]);
		printf("/\n");
	}
	printf("       -----------     O: %d  X: %d\n",
	       color1Count, color2Count);
}

QString Board::getASCIIState(int moveNo)
{
  QString state, tmp;

  int row,i;
  char spaces[]="      ";
  const char *z[]={". ","O ","X ", "o ", "x "};

  state += tmp.sprintf("\n #%-3d  -----------     O: %d  X: %d\n",
		       moveNo, color1Count, color2Count);
  for(row=0;row<4;row++) {
    state += tmp.sprintf("%s/ ",spaces+row);
    for(i=0;i<5+row;i++)
      state += tmp.sprintf("%s",z[field[row*11+12+i]]);
    state += "\\\n";
  }
  state += "  | ";
  for(i=0;i<9;i++)
    state += tmp.sprintf("%s",z[field[56+i]]);
  state += "|\n";
  for(row=0;row<4;row++) {
    state += tmp.sprintf("%s\\ ",spaces+3-row);
    for(i=0;i<8-row;i++)
      state += tmp.sprintf("%s",z[field[68+row*12+i]]);
    state += "/\n";
  }
  state += "       -----------\n\n";

  return state;
}

int Board::setASCIIState(const QString& state)
{
  int moveNo=-1, index;
  int len = state.length();
  int color1Count = 0;
  int color2Count = 0;

  /* get moveNo if supplied */
  if ((index = state.find("#"))>=0)
    moveNo = state.mid(index+1,3).toInt();

  int f=12, row=0, rowEnd = 17;
  char c = ' ';

  index=state.find("/");

  while(index>=0) {

    while(++index<len) {
      c = state[index].latin1();
      if (c !=' ') break;
    }

    if (c=='.') field[f++] = 0;
    else if (c=='o' || c=='O') {
      //      printf(".. F %d: O\n", f);
      field[f++] = 1;
      color1Count++;
    }
    else if (c=='x' || c=='X') {
      //      printf(".. F %d: X\n", f);
      field[f++] = 2;
      color2Count++;
    }
    else {
      if (index >= len) index=-1;
      continue;
    }

    if (f == rowEnd) {
      row++;
      if (row <4) {
	index = state.find("/",index);
	f = 12 + row*11;
	rowEnd = row*12+17;
      }
      else if (row==4) {
	index = state.find("|",index);
	f = 56;
	rowEnd = 65;
      }
      else if (row <9) {
	index = state.find("\\",index);
	f = 8 + row*12;
	rowEnd = 21 + row*11;
      }
      else
	break;
      //      printf("Row %d: %d - %d, Idx %d\n", row, f, rowEnd, index);
    }
  }
  return moveNo;
}


QString Board::getState(int moveNo)
{
  QString state;
  QString entry, tmp;
  int i;

  /* Color + Counts */
  state += (char) ('A' + moveNo /25 );
  state += (char) ('A' + moveNo %25 );
  state += (char) ('A' + color1Count);
  state += (char) ('A' + color2Count);
  state += (char) ('A' + 4*color + field[order[0]]);

  /* Board (field values can be 0-3; 2 fields coded in one char */
  for(i=1;i<61;i+=2)
    state+= (char) ('A' + 4*field[order[i]] + field[order[i+1]] );

  /* -> 35 chars */
  return state;
}

int Board::setState(QString& _state)
{
  int moveNo;
  const char *state = _state.ascii();

  if (_state.length() != 35) return 0;

  moveNo = 25*(state[0] - 'A') + (state[1] - 'A');
  color1Count = state[2] - 'A';
  color2Count = state[3] - 'A';
  color = (state[4] - 'A') / 4;
  field[order[0]] = (state[4] - 'A') %4;

  int i = 1;
  for(int j = 5; j<35; j++) {
    int w = state[j] - 'A';
    field[order[i++]] = w/4;
    field[order[i++]] = w % 4;
  }
  return moveNo;
}

void Board::setSpyLevel(int level)
{
  spyLevel = level;
}
#include "Board.moc"
