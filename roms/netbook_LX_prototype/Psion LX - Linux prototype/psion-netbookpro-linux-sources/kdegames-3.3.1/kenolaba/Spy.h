/* Class Spion
 *
 * Josef Weidendorfer, 10/97
 */

#ifndef _SPY_H_
#define _SPY_H_


#include <qlayout.h>
#include "Board.h"


class BoardWidget;
class QLabel;

class Spy: public QWidget
{
  Q_OBJECT

public:
  Spy(Board&);
  ~Spy() {};
	
  enum { BoardCount = 5 };

  void clearActBoards();

  void keyPressEvent(QKeyEvent *e);

public slots:	
  void update(int,int,Move&,bool);
  void updateBest(int,int,Move&,bool);
  void nextStep();
	
private:
  bool next;
  Board &board;
  QBoxLayout *top;
  BoardWidget *actBoard[BoardCount], *bestBoard[BoardCount];
  QLabel *actLabel[BoardCount], *bestLabel[BoardCount];
};



#endif /* _SPION_H_ */
