#include "mp_simple_interface.h"
#include "mp_simple_interface.moc"

#include <kmessagebox.h>
#include <qtimer.h>
#include <klocale.h>
#include <kaction.h>
#include <kmainwindow.h>


#define PAUSE_ACTION \
    ((KToggleAction *)((KMainWindow *)topLevelWidget())->action("game_pause"))

MPSimpleInterface::MPSimpleInterface(const MPGameInfo &gi,
                                     uint nbActions, const ActionData *data,
									 QWidget *parent, const char *name)
: MPInterface(gi, nbActions, data, parent, name), state(SS_Standby)
{}

void MPSimpleInterface::init()
{
	if ( server() ) {
		state = SS_Standby;
		first_init = TRUE;
	}
	_init();
}

void MPSimpleInterface::start()
{
	// WARNING : multiple calls can happen here (because button
	// hiding is delayed)
	state = SS_Init;
}

void MPSimpleInterface::stop()
{
	state = SS_Standby;
	SC_Flag f1(SC_Flag::Stop);
	if ( server() ) dataToClientsStream() << f1;
	IO_Flag f2(IO_Flag::Stop);
	for (uint i=0; i<nbPlayers(); i++) writingStream(i) << f2;
	immediateWrite();
}

void MPSimpleInterface::addKeys(KKeyDialog &d)
{
    if ( !isPaused() ) pause();
    MPInterface::addKeys(d);
}

void MPSimpleInterface::pause()
{
	// WARNING : multiple calls can happen here (because button
	// hiding is delayed)
	switch (state) {
	 case SS_Play:
         state = SS_PauseAsked;
         break;
	 case SS_Pause:
         state = SS_UnpauseAsked;
         break;
	 default: break;
	}
}

void MPSimpleInterface::dataFromServer(QDataStream &s)
{
	if ( s.atEnd() ) return; // no data

	SC_Flag scf;
	s >> scf;
	switch (scf.value()) {
	 case SC_Flag::Stop:
		KMessageBox::information(this, i18n("Server has left game!"));
		QTimer::singleShot(0, this, SLOT(singleHuman()));
		return;
	 case SC_Flag::GameOver:
		_readGameOverData(s);
		_showGameOverData();
		return;
	}
}

void MPSimpleInterface::treatData()
{
	switch (state) {
	 case SS_Init:         treatInit(); break;
	 case SS_Play:         treatPlay(); break;
	 case SS_Pause:        break;
	 case SS_Stop:         treatStop(); break;
	 case SS_Standby:      break;
	 case SS_PauseAsked:   treatPause(TRUE); break;
	 case SS_UnpauseAsked: treatPause(FALSE); break;
	}
}

void MPSimpleInterface::treatInit()
{
	state = SS_Play;

	if (first_init) {
		_firstInit();
		first_init = FALSE;
	}

	IO_Flag f(IO_Flag::Init);
	for (uint i=0; i<nbPlayers(); i++) writingStream(i) << f;
	_treatInit();
}

void MPSimpleInterface::treatPlay()
{
    PAUSE_ACTION->setEnabled(true);
    PAUSE_ACTION->setChecked(false);

	bool end = _readPlayData();
	if (end) {
		state = SS_Stop;
		IO_Flag f(IO_Flag::GameOver);
		for (uint i=0; i<nbPlayers(); i++) writingStream(i) << f;
		return;
	}
	if ( nbPlayers()==1 ) return; // no need to send data for singleplayer game
	IO_Flag f(IO_Flag::Play);
	for(uint i=0; i<nbPlayers(); i++) writingStream(i) << f;
	_sendPlayData();
}

void MPSimpleInterface::treatPause(bool pause)
{
	state = (pause ? SS_Pause : SS_Play);
	IO_Flag f(IO_Flag::Pause);
	for (uint i=0; i<nbPlayers(); i++) writingStream(i) << f;

    PAUSE_ACTION->setChecked(pause);
}

void MPSimpleInterface::treatStop()
{
	state = SS_Standby;

	// read game over data + send them to all clients
	QDataStream &s = dataToClientsStream();
	SC_Flag f(SC_Flag::GameOver);
	s << f;
	_sendGameOverData(s);
	_showGameOverData();

    PAUSE_ACTION->setEnabled(false);
    PAUSE_ACTION->setChecked(false);
}
