#include "internal.h"
#include "internal.moc"

#include <qptrlist.h>
#include <klocale.h>
#include "mp_interface.h"

#define DATA_ERROR(i)  { dataError(i);  return; }
#define READ_ERROR(i)  { readError(i);  return; }
#define WRITE_ERROR(i) { writeError(i); return; }
#define BROKE_ERROR(i) { brokeError(i); return; }
#define LAG_ERROR      { lagError();    return; }


//-----------------------------------------------------------------------------
void Local::dataError(uint i)
{
	qWarning("MP : Invalid data from board #%i", i);
}

void Local::readData(bool inverse)
{
	// read data from local boards
	for (uint i=0; i<ios.size(); i++) {
		boards[i].ptr->dataOut(ios[i]->writing);
		if (inverse) ios[i]->writingToReading();
	}
}

void Local::writeData(bool inverse)
{
	// write data to local boards
	for (uint i=0; i<ios.size(); i++) {
		if (inverse) ios[i]->writingToReading();
		boards[i].ptr->dataIn(ios[i]->reading);
		if ( !ios[i]->reading.readOk() ) DATA_ERROR(i);
	}
}

void Local::treatData()
{
	readData(TRUE);
	interface->treatData();
	// check reading stream
	for (uint i=0; i<ios.size(); i++)
		if ( !ios[i]->reading.readOk() ) DATA_ERROR(i);
	writeData(TRUE);
}

//-----------------------------------------------------------------------------
Server::Server(uint _interval)
: interval(_interval)
{
	timer.start(interval);
}

void Server::congestion()
{
	qWarning("MP : congestion occurred !!");
}

void Server::serverTimeout()
{
	ctimer.start(2*interval, TRUE);
	timeout();
}

//-----------------------------------------------------------------------------
Network::Network(MPInterface *_interface,
				 QValueList<MPInterface::Data> &_boards,
				 const QPtrList<RemoteHostData> &rhd)
: Local(_interface, _boards)
{
	RemoteData rd;
	QPtrListIterator<RemoteHostData> it(rhd);
	for (; it.current(); ++it) {
        rd.socket = it.current()->socket;
		rd.socket->notifier()->setEnabled(TRUE);
		connect(rd.socket->notifier(), SIGNAL(activated(int)),
                SLOT(notifier(int)));
		uint nb = it.current()->bds.count();
		Q_ASSERT( nb>=1 );
		rd.array  = new BufferArray(nb);
		for (uint k=0; k<it.current()->bds.count(); k++)
			rd.names += it.current()->bds[k].name;
		remotes += rd;
	}
}

Network::~Network()
{
	for (uint i=0; i<remotes.count(); i++) {
		delete remotes[i].socket;
		delete remotes[i].array;
	}
}

uint Network::nbPlayers() const
{
	uint nb = Local::nbPlayers();
	for (uint i=0; i<remotes.count(); i++) nb += remotes[i].array->size();
	return nb;
}

QString Network::playerName(uint i) const
{
	uint l = Local::nbPlayers();
	if ( i<l ) return Local::playerName(i);
	for (uint k=0; k<remotes.count(); k++) {
		uint ll = remotes[k].array->size();
		if ( i<(l+ll) ) return remotes[k].names[i-l];
		l += ll;
	}
	return QString::null; // dummy
}

IOBuffer *Network::ioBuffer(uint i) const
{
	if ( i<Local::nbPlayers() ) return Local::ioBuffer(i);
	i -= Local::nbPlayers();
	for (uint k=0; k<remotes.count(); k++) {
		if ( i<remotes[k].array->size() ) return (*remotes[k].array)[i];
		i -= remotes[k].array->size();
	}
	Q_ASSERT(FALSE);
	return 0;
}

void Network::readError(uint i)
{
	disconnectHost(i, i18n("Unable to read socket"));
}

void Network::writeError(uint i)
{
	disconnectHost(i, i18n("Unable to write to socket"));
}

void Network::brokeError(uint i)
{
	disconnectHost(i, i18n("Link broken"));
}

void Network::disconnectHost(uint i, const QString &msg)
{
	delete remotes[i].socket;
	delete remotes[i].array;
	remotes.remove(remotes.at(i));
	interface->hostDisconnected(i, msg);
}

//-----------------------------------------------------------------------------
LocalServer::LocalServer(MPInterface *_interface,
						 QValueList<MPInterface::Data> &_boards,
						 uint _interval)
: Local(_interface, _boards), Server(_interval)
{
	connect(&timer, SIGNAL(timeout()), SLOT(timeoutSlot()));
	connect(&ctimer, SIGNAL(timeout()), SLOT(congestionTimeoutSlot()));
	serverTimeout();
}

//-----------------------------------------------------------------------------
NetworkServer::NetworkServer(MPInterface *_interface,
							 QValueList<MPInterface::Data> &_boards,
							 const QPtrList<RemoteHostData> &rhd, uint _interval)
: Network(_interface, _boards, rhd), Server(_interval),
  nbReceived(remotes.count())
{
	connect(&timer, SIGNAL(timeout()), SLOT(timeoutSlot()));
	connect(&ctimer, SIGNAL(timeout()), SLOT(congestionTimeoutSlot()));
	// to catch unexpected data
	for (uint i=0; i<remotes.count(); i++) remotes[i].received = TRUE;
	nbReceived = remotes.count();
//  let the client the time to create itself ...
//	serverTimeout();
}

void NetworkServer::timeout()
{
	if ( nbReceived<remotes.count() ) LAG_ERROR;
	nbReceived = 0;
	for (uint i=0; i<remotes.count(); i++) remotes[i].received = FALSE;
	// send MF_ASK : asking for data from clients
	for (uint i=0; i<remotes.count(); i++) {
		remotes[i].socket->writingStream() << MF_Ask;
//		debug("SERVER : send ask flag");
		if ( !remotes[i].socket->write() ) WRITE_ERROR(i);
	}
}

void NetworkServer::notifier(int fd)
{
	uint i;
	for (i=0; i<remotes.count(); i++)
		if ( remotes[i].socket->fd()==fd ) break;
	Q_ASSERT( i<remotes.count() );

	if ( remotes[i].received ) READ_ERROR(i);
	switch ( remotes[i].socket->read() ) {
	 case -1: READ_ERROR(i);
	 case  0: BROKE_ERROR(i);
	}

	remotes[i].received = TRUE;
	nbReceived++;

	ReadingStream &s = remotes[i].socket->readingStream();
//	debug("SERVER : notifier + read (fd=%i i=%i size=%i)", fd, i,
//		  s.size());
	s >> *remotes[i].array;
	if ( !s.readOk() ) DATA_ERROR(i);

	// all data from clients received
	if ( nbReceived==remotes.count() ) treatData();
}

void NetworkServer::writeData(bool inverse)
{
	Local::writeData(inverse);
	for (uint i=0; i<remotes.count(); i++) {
		WritingStream &s = remotes[i].socket->writingStream();
		s << MF_Data;
		s << *remotes[i].array;
		s.writeRawBytes(globalStream()->buffer().data(),
						globalStream()->size());
//		debug("SERVER : write data (size= 1 + %i + %i=%i)",
//			  globalStream()->size(), s.size()-globalStream()->size()-1,
//			  s.size());
		if ( !remotes[i].socket->write() ) WRITE_ERROR(i);
	}
	globalStream()->clear();
}

void NetworkServer::lagError()
{
	for (uint i=0; i<remotes.count(); i++)
		if ( !remotes[i].received )
            disconnectHost(i, i18n("Client has not answered in time"));
}

//-----------------------------------------------------------------------------
void Client::notifier(int)
{
	switch ( remotes[0].socket->read() ) {
	 case -1: READ_ERROR(0);
	 case  0: BROKE_ERROR(0);
	}
	ReadingStream &s = remotes[0].socket->readingStream();
	MetaFlag mf;
	s >> mf;
	if ( !s.readOk() ) DATA_ERROR(0);
//	debug("CLIENT : reading stream (size=%i flag=%i)", s.size(),
//		  (int)mf);
	switch(mf) {
	 case MF_Ask:
		// write data from local boards to server socket (cleaning
		// of writing stream is done in write())
		readData(FALSE);
		remotes[0].socket->writingStream() << ios;
//		debug("CLIENT : send ios (size=%i)",
//			  remotes[0].socket->writingStream().size());
		if ( !remotes[0].socket->write() ) WRITE_ERROR(0);
		break;
	 case MF_Data:
		// read data from server to interface & local boards
//		debug("CLIENT : before receive ios (at=%i)", s.device()->at());
		s >> ios;
//		debug("CLIENT : after receive ios (at=%i)", s.device()->at());
		interface->dataFromServer(s);
//		debug("CLIENT : after dataFromServer (at=%i)", s.device()->at());
		if ( !s.readOk() ) DATA_ERROR(0);
		writeData(FALSE);
		break;
	 default: DATA_ERROR(0);
	}
	if ( !s.atEnd() ) qWarning("CLIENT : remaining data");
}
