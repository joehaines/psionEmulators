#ifndef MP_SIMPLE_BOARD_H
#define MP_SIMPLE_BOARD_H

#include "mp_board.h"
#include "mp_simple_types.h"

class MPSimpleBoard : public MPBoard
{
 Q_OBJECT
		
 public:
    MPSimpleBoard(QWidget *parent = 0, const char *name = 0)
	: MPBoard(parent, name) {}
	virtual ~MPSimpleBoard() {}

	void init(bool AI, bool multiplayers, bool server, bool first,
			  const QString &name);
	void dataOut(QDataStream &s);
	void dataIn(QDataStream &s);
	
 protected:
	virtual void _init(bool AI, bool multiplayers, bool server, bool first,
					   const QString &name) = 0;
	virtual void _initFlag(QDataStream &s) = 0;
	virtual void _playFlag(QDataStream &s) = 0;
	virtual void _pauseFlag(bool pause)    = 0;
	virtual void _stopFlag(bool gameover)  = 0;
	virtual void _dataOut(QDataStream &s)  = 0;
	virtual void _gameOverDataOut(QDataStream &s) = 0;
	virtual void _initDataOut(QDataStream &s) = 0;

 private:
	BoardState state;
	
	void initFlag(QDataStream &s);
	void playFlag(QDataStream &s);
	void pauseFlag();
	void stopFlag();
	void gameOverFlag();
	void _stop(bool button);
};

#endif // MP_SIMPLE_BOARD_H
