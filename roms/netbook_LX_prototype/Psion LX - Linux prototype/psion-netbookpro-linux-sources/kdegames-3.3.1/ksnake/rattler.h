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

#ifndef RATTLER_H
#define RATTLER_H

#include <qwidget.h>
#include <kaction.h>
#include "ball.h"
#include "snake.h"

class	Board;
class PixServer;
class Level;
class Basket;
class SamySnake;


enum { Init, Playing, Demo, Paused, Over };

class Rattler : public QWidget
{
	Q_OBJECT

public:
	Rattler ( QWidget *parent=0, const char *name=0 );
	~Rattler();
  void setActionCollection(KActionCollection *a){ actionCollection = a;};


	void setBalls(int);
	void setBallsAI(int);
	void setCompuSnakes(int);
	void setSnakesAI(int);
	void setSkill(int);
	void setRoom(int);

	void reloadRoomPixmap();
	
	QStringList backgroundPixmaps;
	
public slots:
	void closeGate(int);
	void openGate();
	void loadSettings();
	
	void scoring(bool, int);

	void restart();
	void newTry();
	void levelUp();

	void pause();
	void restartTimer();

	void speedUp();

	void run();
	void demo();

	void killedComputerSnake();

private slots:
	void start();
	void stop();
	void showRoom();
	void restartDemo();

signals:
	void setPoints(int);
	void setTrys(int);

	void setScore(int);
	// Is this used?  Maybe remove?
	void togglePaused();

	// progress
	void rewind();
	void advance();

protected:
	void timerEvent( QTimerEvent * );
	void paintEvent( QPaintEvent * );
	void keyPressEvent( QKeyEvent * );
	void mousePressEvent( QMouseEvent * );
	void focusOutEvent( QFocusEvent * ) { ; }
	void focusInEvent( QFocusEvent * )  { ; }
	KActionCollection *actionCollection;

private:
	Board	 *board;
	PixServer *pix;
	Level	 *level;
	Basket	*basket;
	SamySnake *samy;

	int  timerCount;
	bool leaving;

	int check;
	int points;
	int trys;

	int direction;

	QPtrList<Ball> *balls;
	void restartBalls(bool);
	int numBalls;
	int ballsAI;

	QPtrList<CompuSnake> *computerSnakes;
	void restartComputerSnakes(bool);
	int numSnakes;
	int snakesAI;

	int room;
	int skill;

	int  gameTimer;
	bool timerHasRunOut;
	void start(int);
	void resetFlags();
	void init(bool);

	void score(int);
	void cleanLabel();

	void resizeEvent( QResizeEvent * );
};


#endif // RATTLER_H

