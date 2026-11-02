#ifndef MEETING_H
#define MEETING_H

#include <qstatusbar.h>
#include <kdialogbase.h>
#include "smanager.h"
#include "pline.h"
#include "types.h"

class MPOptionWidget;

/** Internal class : net meeting. */
class NetMeeting : public KDialogBase
{
 Q_OBJECT

 public:
	// "gameName" and "gameId" are QByteArray because they are
    // used for ID comparing between games.
	NetMeeting(const cId &id, Socket *, MPOptionWidget *option, bool server,
               QWidget *parent = 0, const char * name = 0);
	virtual ~NetMeeting();

 protected slots:
	void         readNotifier(int socket);
	virtual void textChanged(const QString &) = 0;
	virtual void typeChanged(MeetingCheckBox::Type) = 0;
	virtual void reject();
	virtual void accept();

 protected:
	enum PlayerState { NewPlayer, IdChecked, Accepted };
	QValueList<PlayerState>  players;
	bool                     server;
	MeetingLine             *spl;
	WidgetList<MeetingLine> *wl;
	SocketManager            sm;
	MPOptionWidget          *ow;
	cId                      id;
	bool                     socketRemoved;

	void appendLine(const MeetingLineData &pld, bool server);
	void removeLine(uint i);
	void setType(const TypeInfo &ti);
	void setText(const TextInfo &ti);

	void cleanReject(const QString &str = QString::null);
	bool checkState(uint i, PlayerState s);
	bool checkAndSetState(uint i, PlayerState os, PlayerState ns);
	bool ready() const;

	virtual void idFlag(uint i)      { dataError(i); }
	virtual void newFlag(uint i)     { dataError(i); }
	virtual void endFlag(uint i)     { dataError(i); }
	virtual void modTypeFlag(uint i) { dataError(i); }
	virtual void modTextFlag(uint i) { dataError(i); }
	virtual void delFlag(uint i)     { dataError(i); }
	virtual void modOptFlag(uint i)  { dataError(i); }
	virtual void playFlag(uint i)    { dataError(i); }

	virtual void netError(uint i, const QString &str) = 0;
	virtual void writeToAll(uint i=0) = 0;
	void readError(uint i);
	void writeError(uint i);
	void dataError(uint i);
	void brokeError(uint i);
	void message(const QString &str);

 private:
	QLabel     *labWait;
	QStatusBar *status;

	void waiting();
	void readData(uint i);
};

class ServerNetMeeting : public NetMeeting
{
 Q_OBJECT

 public:
     ServerNetMeeting(const cId &id,
					  const RemoteHostData &rhd, MPOptionWidget *options,
					  QPtrList<RemoteHostData> &arhd,
					  QWidget *parent = 0, const char * name = 0);

 private slots:
	void newHost(int);
	void textChanged(const QString &text);
	void typeChanged(MeetingCheckBox::Type);
	void accept();
	void optionsChanged();

 private:
	QPtrList<RemoteHostData> &rhd;

	void idFlag(uint i);
	void newFlag(uint i);
	void endFlag(uint i);
	void modTypeFlag(uint i);
	void modTextFlag(uint i);

	void netError(uint i, const QString &str);
	void writeToAll(uint i = 0);
	void disconnectHost(uint i, const QString &str);
};

class ClientNetMeeting : public NetMeeting
{
 Q_OBJECT

 public:
	ClientNetMeeting(const cId &id,
					 const RemoteHostData &rhd, MPOptionWidget *options,
					 QWidget *parent = 0, const char * name = 0);

 private slots:
	void textChanged(const QString &text);
	void typeChanged(MeetingCheckBox::Type);

 private:
	QValueList<BoardData> bds;

	void idFlag(uint);
	void newFlag(uint);
	void endFlag(uint);
	void delFlag(uint);
	void modTypeFlag(uint);
	void modTextFlag(uint);
	void modOptFlag(uint);
	void playFlag(uint);

	void writeToAll(uint i=0);
	void netError(uint, const QString &str);
};

#endif // MEETING_H
