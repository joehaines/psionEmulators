#ifndef INTERNAL_H
#define INTERNAL_H

#include <qtimer.h>
#include <qstringlist.h>

#include "socket.h"
#include "mp_interface.h"

class MPBoard;
class RemoteHostData;

//-----------------------------------------------------------------------------
class Local
{
 public:
	Local(MPInterface *_interface, QValueList<MPInterface::Data> &_boards)
	: interface(_interface), ios(_boards.count()), boards(_boards) {}
	virtual ~Local() {}

	virtual uint nbPlayers() const           { return boards.count();  }
	virtual QString playerName(uint i) const { return boards[i].name; }
	virtual IOBuffer *ioBuffer(uint i) const { return ios[i]; }
	virtual void writeData(bool inverse);
	virtual WritingStream *globalStream() { return 0; }

 protected:
	MPInterface *interface;
	BufferArray  ios;

	void dataError(uint i);
	void readData(bool inverse);
	void treatData();

 private:
	QValueList<MPInterface::Data> boards;
};

//-----------------------------------------------------------------------------
class Server
{
 public:
	Server(uint _interval);
	virtual ~Server() {}

 protected:
	WritingStream stream;
	QTimer timer, ctimer;

	virtual void timeout() = 0;
	void serverTimeout();
	void congestion();

 private:
	uint   interval;
};

//-----------------------------------------------------------------------------
class Network : public QObject, public Local
{
 Q_OBJECT

 public:
	Network(MPInterface *_interface, QValueList<MPInterface::Data> &_boards,
			const QPtrList<RemoteHostData> &rhd);
	virtual ~Network();

	virtual uint nbPlayers() const;
	QString playerName(uint i) const;
	IOBuffer *ioBuffer(uint i) const;

 protected slots:
	virtual void notifier(int fd) = 0;

 protected:
	class RemoteData {
	public:
		RemoteData() {}
		Socket      *socket;
		BufferArray *array;
		bool         received;
		QStringList  names;
	};
	QValueList<RemoteData> remotes;

	void readError(uint i);
	void writeError(uint i);
	void brokeError(uint i);
	void disconnectHost(uint i, const QString &msg);
};

//-----------------------------------------------------------------------------
class LocalServer : public QObject, public Local, public Server
{
 Q_OBJECT

 public:
	LocalServer(MPInterface *_interface,
				QValueList<MPInterface::Data> &_boards, uint _interval);

	WritingStream *globalStream() { return &stream; }

 private slots:
	void timeoutSlot()           { serverTimeout(); }
	void congestionTimeoutSlot() { congestion(); }

 private:
	void timeout() { treatData(); }
};

//-----------------------------------------------------------------------------
class NetworkServer : public Network, public Server
{
 Q_OBJECT

 public:
	NetworkServer(MPInterface *_interface,
				  QValueList<MPInterface::Data> &_boards,
				  const QPtrList<RemoteHostData> &rhd, uint _interval);

	void writeData(bool inverse);
	WritingStream *globalStream() { return &stream; }

 private slots:
	void timeoutSlot()           { serverTimeout(); }
	void congestionTimeoutSlot() { congestion(); }
	void notifier(int fd);

 private:
	uint nbReceived;

	void lagError();
	void timeout();
};

//-----------------------------------------------------------------------------
class Client : public Network
{
 Q_OBJECT

 public:
	Client(MPInterface *_interface, QValueList<MPInterface::Data> &_boards,
		   const QPtrList<RemoteHostData> &rhd)
	: Network(_interface, _boards, rhd) {}

	uint nbPlayers() const { return Local::nbPlayers(); }

 private slots:
	void notifier(int fd);
};

#endif // INTERNAL_H
