/* Classes Move, MoveList
 * - represents a move in the game of Abalone
 *
 * Josef Weidendorfer, 28.8.97
*/

#include <assert.h>
#include <stdio.h>

#include <qstring.h>

#include <klocale.h>

#include "Move.h"
#include "Board.h"

const QString nameOfDir(int dir)
{
  dir = dir % 6;
  return 
    (dir == 1) ? i18n("Right") :
    (dir == 2) ? i18n("RightDown") :
    (dir == 3) ? i18n("LeftDown") :
    (dir == 4) ? i18n("Left") :
    (dir == 5) ? i18n("LeftUp") :
    (dir == 0) ? i18n("RightUp") : QString("??");
}

QString nameOfPos(int p)
{
  static char tmp[3];
  tmp[0] = 'A' + (p-12)/11;
  tmp[1] = '1' + (p-12)%11;
  tmp[2] = 0;

  return QString( tmp );
}

QString Move::name() const
{
  QString s,tmp;

  /* sideway moves... */
  if (type == left3 || type == right3) {
    int f1, f2, df;
    
    f1 = f2 = field;
    df = 2* Board::fieldDiffOfDir(direction);
    if (df > 0)
      f2 += df;    
    else
      f1 += df;
    
    s = nameOfPos( f1 );
    s += '-';
    s += nameOfPos( f2 );
    s+= '/';
    s+= (type == left3) ? nameOfDir(direction-1): nameOfDir(direction+1);
  }
  else if ( type == left2 || type == right2) {
    int f1, f2, df;

    f1 = f2 = field;
    df = Board::fieldDiffOfDir(direction);
    if (df > 0)
      f2 += df;    
    else
      f1 += df;
    
    s = nameOfPos( f1 );
    s += '-';
    s += nameOfPos( f2 );
    s+= '/';
    s+= (type == left2) ? nameOfDir(direction-1): nameOfDir(direction+1);
  }
  else if (type == none) {
    s = QString("??");
  }
  else {
    s = nameOfPos( field );
    s += '/';
    s += nameOfDir(direction);
    
    tmp = (type <3 ) ? i18n("Out") :
          (type <6 ) ? i18n("Push") : QString("");
    if (!tmp.isEmpty()) {
      s += '/';
      s += tmp;
    }
  }
  return s;
}

void Move::print() const
{
  printf("%s", name().ascii() );
}

MoveTypeCounter::MoveTypeCounter()
{
  init();
}

void MoveTypeCounter::init()
{
  for(int i=0;i < Move::typeCount;i++)
    count[i] = 0;
}

int MoveTypeCounter::sum()
{
  int sum = count[0];

  for(int i=1;i < Move::typeCount;i++)
    sum += count[i];

  return sum;
}


InARowCounter::InARowCounter()
{
  init();
}

void InARowCounter::init()
{
  for(int i=0;i < inARowCount;i++)
    count[i] = 0;
}

MoveList::MoveList()
{
	clear();
}

void MoveList::clear()
{
	int i;
	
	for(i=0;i<Move::typeCount;i++)
		first[i] = actual[i] = -1;
	
	nextUnused = 0;
	actualType = -1;
}

void MoveList::insert(Move m)
{
	int t = m.type;
	
	/* valid and possible ? */
	if (t <0 || t >= Move::typeCount) return;
	if (nextUnused == MaxMoves) return;

	assert( nextUnused < MaxMoves );

	/* adjust queue */
	if (first[t] == -1) {
		first[t] = last[t] = nextUnused;
	}
	else {
	        assert( last[t] < nextUnused );
		next[last[t]] = nextUnused;
		last[t] = nextUnused;
	}
	
	next[nextUnused] = -1;	
	move[nextUnused] = m;
	nextUnused++;
}

bool MoveList::isElement(int f)
{
	int i;
	
	for(i=0; i<nextUnused; i++) 
	  if (move[i].field == f) 
		return true;

	return false;
}


bool MoveList::isElement(Move &m, int startType,bool del)
{
	int i;
	
	for(i=0; i<nextUnused; i++) {
	  Move& mm = move[i];
	  if (mm.field != m.field) 
	    continue;

	  /* if direction is supplied it has to match */
	  if ((m.direction > 0) && (mm.direction != m.direction))
	    continue;

	  /* if type is supplied it has to match */	    
	  if ((m.type != Move::none) && (m.type != mm.type))
	    continue;
				      
	  if (m.type == mm.type) {
	    /* exact match; eventually supply direction */
	    m.direction = mm.direction;
	    if (del) mm.type = Move::none;
	    return true;
	  }

	  switch(mm.type) {
	  case Move::left3:
	  case Move::right3:
	    if (startType == start3 || startType == all) {
	      m.type = mm.type;
	      m.direction = mm.direction;
	      if (del) mm.type = Move::none;
	      return true;
	    }
	    break;
	  case Move::left2:
	  case Move::right2:
	    if (startType == start2 || startType == all) {
	      m.type = mm.type;
	      m.direction = mm.direction;
	      if (del) mm.type = Move::none;
	      return true;
	    }
	    break;
	  default:
	    if (startType == start1 || startType == all) {
	      /* unexact match: supply type */
	      m.type = mm.type;
	      m.direction = mm.direction;
	      if (del) mm.type = Move::none;
	      return true;
	    }
	  }
	}
	return false;
}


bool MoveList::getNext(Move& m, int maxType)
{
	if (actualType == Move::typeCount) return false;

	while(1) {
	  while(actualType < 0 || actual[actualType] == -1) {
	    actualType++;
	    if (actualType == Move::typeCount) return false;
	    actual[actualType] = first[actualType];
	    if (actualType > maxType) return false;
	  }
	  m = move[actual[actualType]];
	  actual[actualType] = next[actual[actualType]];
	  if (m.type != Move::none) break;
	}

	return true;
}


