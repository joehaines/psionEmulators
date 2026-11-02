/* Class Board - represents a game state
 *
 * Josef Weidendorfer, 28.8.97
*/

#ifndef _BOARD_H_
#define _BOARD_H_

#include <qobject.h>
#include <krandomsequence.h>
#include "Move.h"

class KConfig;
class EvalScheme;

/* Class for best moves so far */
class PrincipalVariation
{
public:
	PrincipalVariation()
	  { clear(1); }

	enum { maxDepth = 10 };

	bool hasMove(int d)
	  {  return (d>actMaxDepth) ?
		    false : (move[0][d].type != Move::none); }

	Move& operator[](int i)
	  { return (i<0 || i>=maxDepth) ? move[0][0] : move[0][i]; }

	void update(int d, Move& m);
	void clear(int d);
	void setMaxDepth(int d)
	  { actMaxDepth = (d>maxDepth) ? maxDepth-1 : d; }

private:
	Move move[maxDepth][maxDepth];
	int actMaxDepth;

};


class Board : public QObject
{
  Q_OBJECT

 public:
  Board();
  ~Board() {};

  /* different states of one field */
  enum {
    out = 10, free = 0,
    color1, color2, color1bright, color2bright
  };
  enum { AllFields = 121, /* visible + ring of unvisible around */
         RealFields = 61, /* number of visible fields */
         MvsStored = 100 };

  int debug;

  /* fill Board with defined values */
  void begin(int startColor);  /* start of a game */
  void clear();                /* empty board     */

  /* fields can't be changed ! */
  int operator[](int no) const;

  int actColor() const
    { return color; }

  /* Generate list of allowed moves for player with <color>
   * Returns a calculated value for actual position */
  void generateMoves(MoveList& list);

  /* Functions handling moves
   * played moves can be taken back (<MvsStored> moves are remembered) */
  void playMove(const Move& m);
  bool takeBack();    /* if not remembered, do nothing */
  int movesStored();  /* return how many moves are remembered */

  Move& lastMove()
    { return storedMove[storedLast]; }

  void showHist();

  /* Evaluation Scheme to use */
  void setEvalScheme( EvalScheme* scheme = 0);
  EvalScheme* evalScheme() { return _evalScheme; }

  /* Calculate a value for actual position
   * (greater if better for color1) */
  int calcEvaluation();

  /* Evalution is based on values which can be changed
   * a little (so computer's moves aren't always the same) */
  void changeEvaluation();

  void setActColor(int c) { color=c; }
  void setColor1Count(int c) { color1Count = c; }
  void setColor2Count(int c) { color2Count = c; }
  void setField(int i, int v) { field[i] = v; }

  void setSpyLevel(int);

  int getColor1Count() 	  { return color1Count; }
  int getColor2Count() 	  { return color2Count; }

  enum { empty=0, valid, invalid };
  int validState();
  bool isValid() { return (color1Count>8 && color2Count>8); }

  /* Check that color1Count & color2Count is consisten with board */
  bool isConsistent();

  /* Searching best move: alpha/beta search */
  void setDepth(int d)
    { realMaxDepth = d+1; }
  Move& bestMove();

  /* next move in main combination */
  Move& nextMove() { return pv[1]; }

  Move randomMove();
  void stopSearch() { breakOut = true; }

  /* Compressed ASCII representation */
  QString getState(int);
  int setState(QString&);

  /* Readable ASCII representation */
  QString getASCIIState(int);
  int setASCIIState(const QString&);

  void updateSpy(bool b) { bUpdateSpy = b; }

  /* simple terminal view of position */
  void print(int);

  static int fieldDiffOfDir(int d) { return direction[d]; }

 signals:
  void searchBreak();
  void updateBestMove(Move&,int);

  void update(int,int,Move&,bool);
  void updateBest(int,int,Move&,bool);

 private:
  void setFieldValues();

  /* helper function for generateMoves */
  void generateFieldMoves(int, MoveList&);
  /* helper function for calcValue */
  void countFrom(int,int, MoveTypeCounter&, InARowCounter&);
  /* helper functions for bestMove (recursive search!) */
  int search(int, int, int);
  int search2(int, int, int);

  KRandomSequence random;       /* random generator */

  int field[AllFields];         /* actual board */
  int color1Count, color2Count;
  int color;                    /* actual color */
  Move storedMove[MvsStored];   /* stored moves */
  int storedFirst, storedLast;  /* stored in ring puffer manner */

  /* for search */
  PrincipalVariation pv;
  Move _bestMove;
  bool breakOut, inPrincipalVariation, show, bUpdateSpy;
  int maxDepth, realMaxDepth;

  int spyLevel, spyDepth;
  EvalScheme* _evalScheme;

  /* ratings; semi constant - are rotated by changeRating() */
  static int fieldValue[RealFields];

  /* constant arrays */
  static int startBoard[AllFields];
  static int order[RealFields];
  static int direction[8];

  //  static int stoneValue[6];
  //  static int moveValue[Move::typeCount];
  //  static int connectValue[ConnectCounter::connectCount];
  //  static int ringValue[5], ringDiff[5];
};


inline int Board::operator[](int no) const
{
  return (no<12 || no>120) ? out : field[no];
}

#endif
