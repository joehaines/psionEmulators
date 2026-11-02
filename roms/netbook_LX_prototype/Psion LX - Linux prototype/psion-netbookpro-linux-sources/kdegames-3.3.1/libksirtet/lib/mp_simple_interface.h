#ifndef MP_SIMPLE_INTERFACE_H
#define MP_SIMPLE_INTERFACE_H

#include "mp_interface.h"
#include "mp_simple_types.h"

class MPSimpleInterface : public MPInterface
{
 Q_OBJECT

 public:
	MPSimpleInterface(const MPGameInfo &gi,
                      uint nbActions, const ActionData *data,
                      QWidget *parent = 0, const char *name = 0);

    bool isPaused() const { return state==SS_Pause; }

 public slots:
	void start();
	void pause();
	void addKeys(KKeyDialog &);

 protected:
	virtual void _init() = 0;
	virtual void _readGameOverData(QDataStream &s) = 0;
	virtual void _sendGameOverData(QDataStream &s) = 0;
	virtual void _showGameOverData() = 0;
	virtual void _firstInit() = 0;
	virtual void _treatInit() = 0;
	virtual bool _readPlayData() = 0;
	virtual void _sendPlayData() = 0;

 private:
	ServerState state;
	bool        first_init;

	void treatData();
	void treatInit();
	void treatPlay();
	void treatPause(bool pause);
	void treatStop();

	void init();
	void stop();
	void dataFromServer(QDataStream &);
};

#endif // MP_SIMPLE_INTERFACE_H
