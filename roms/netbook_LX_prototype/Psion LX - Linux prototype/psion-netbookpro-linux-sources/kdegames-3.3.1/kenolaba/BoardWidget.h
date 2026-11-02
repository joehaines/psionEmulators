#ifndef _BOARDWIDGET_H_
#define _BOARDWIDGET_H_

#include <qwidget.h>
#include <qpixmap.h>

#include "Ball.h"
#include "Move.h"
#include "Board.h"

#ifdef HAVE_KIR
class KIRenderer;
#endif

class BoardWidget : public BallWidget
{
	Q_OBJECT
	
 public:
	BoardWidget(Board&, QWidget *parent = 0, const char *name = 0);
	~BoardWidget();

	void createPos(int , int , int , Ball*);
	void initBalls();
	void updateBalls();     

	virtual void resizeEvent(QResizeEvent *);
	virtual void moveEvent(QMoveEvent *);
	virtual void paintEvent(QPaintEvent *);
	virtual void mousePressEvent( QMouseEvent* pEvent );
	virtual void mouseReleaseEvent( QMouseEvent* pEvent );
	virtual void mouseMoveEvent( QMouseEvent* pEvent );

	void renderBalls(bool r);

	void draw();
	
	void choseMove(MoveList*);

  /* Show a move with highlighting
   *  step 0: state before move
   *       1: state after move
   *       2: remove all marks (for blinking)
   *       3: highlight marks (before move)
   *       4: highlight marks (after move)
   * Always call with step 0 before actual playing the move !! */
  void showMove(const Move& m, int step, bool updateGUI = true);
  
  /* Only highlight start
   * Step 0: original state
   *      1: highlight start
   * Always call with step 0 before actual playing the move !! */
  void showStart(const Move& m, int step, bool updateGUI = true);
  

	/* enable/disable Edit Mode: 
	 * On disabling & valid position it's actually copied to Board
	 */
	bool setEditMode(bool);
	//	int validState() { return board->validState(); }
	//	bool isValid() { return validState()==Board::valid; }
	
	/* copy actual board position */
	void updatePosition(bool updateGUI = false);
	void clearPosition();

	int getColor1Count()      { return color1Count; }
	int getColor2Count()      { return color2Count; }

 public slots:
	void configure();
	void drawBoard();

 signals:
	void moveChoosen(Move&);
	void rightButtonPressed(int,const QPoint&);
	void updateSpy(QString);
	void edited(int);
 protected:
	virtual QSize sizeHint() const;

 private:
	int positionOf(int x, int y);
	bool isValidStart(int pos, bool);
	bool isValidEnd(int pos);

	QPixmap pm, boardPM;
	Board& board;
	int actValue;

	bool editMode, renderMode;
	int editColor;

	/* copied position */
	int field[Board::AllFields];
	int color1Count, color2Count, color;       
	bool boardOK;

	/* for getting user Move */
	MoveList *pList;
	Move actMove;
	bool gettingMove, mbDown, startValid, startShown;
	int startPos, actPos, oldPos, shownDirection;
	int startField, startField2, actField, oldField, startType;
	QColor *boardColor, *redColor, *yellowColor, *redHColor, *yellowHColor;
	QCursor *arrowAll, *arrow[7];

	Ball *n1, *n2, *h1, *h2, *d1, *d2; //, *e;

#ifdef HAVE_KIR
	KIRenderer *m_backRenderer;
#endif
};

#endif /* _BOARDWIDGET_H_ */

