#include "mp_simple_board.h"
#include "mp_simple_board.moc"


void MPSimpleBoard::init(bool AI, bool multiplayers, bool server, bool first,
						 const QString &name)
{
	state = BS_Init;
	_init(AI, multiplayers, server, first, name);
}

void MPSimpleBoard::dataIn(QDataStream &s)
{
	if ( s.atEnd() ) return; // no data

	IO_Flag f;
	s >> f;
	switch ( f.value() ) {
	 case IO_Flag::Init:      initFlag(s);    break;
	 case IO_Flag::Play:      playFlag(s);    break;
	 case IO_Flag::Pause:     pauseFlag();    break;
	 case IO_Flag::GameOver:  gameOverFlag(); break;
	 case IO_Flag::Stop:      stopFlag();     break;
	}
}

void MPSimpleBoard::initFlag(QDataStream &s)
{
	state = BS_Play;
	emit enableKeys(true);
	_initFlag(s);
}

void MPSimpleBoard::playFlag(QDataStream &s)
{
	Q_ASSERT( state==BS_Play );
	_playFlag(s);
}

void MPSimpleBoard::pauseFlag()
{
	Q_ASSERT( state==BS_Play || state==BS_Pause );
	bool p = ( state==BS_Pause );
	state  = (p ? BS_Play : BS_Pause);
	emit enableKeys(p);
	_pauseFlag(!p);
}

void MPSimpleBoard::gameOverFlag()
{
	Q_ASSERT( BS_Play );
	_stop(TRUE);
	state = BS_Stop;
}

void MPSimpleBoard::stopFlag()
{
	_stop(FALSE);
	state = BS_Standby;
}

void MPSimpleBoard::_stop(bool gameover)
{
	if ( state==BS_Pause ) _pauseFlag(FALSE);
	emit enableKeys(false);
	_stopFlag(gameover);
}

void MPSimpleBoard::dataOut(QDataStream &s)
{
	switch (state) {
	 case BS_Init:
		_initDataOut(s);
		state = BS_Standby;
		return;
	 case BS_Play:  _dataOut(s); return;
	 case BS_Stop:
		_gameOverDataOut(s);
		state = BS_Standby;
		return;
	 case BS_Pause:   return;
	 case BS_Standby: return;
	}
}
