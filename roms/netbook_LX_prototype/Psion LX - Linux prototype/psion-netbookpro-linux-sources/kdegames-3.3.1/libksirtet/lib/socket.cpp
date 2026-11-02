#include "socket.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <ctype.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h> // for FIONREAD
#endif

Socket::Socket(KExtendedSocket *s, bool createNotifier,
               QObject *parent, const char *name)
: _socket(s), _notifier(0)
{
    Q_ASSERT(s);
	if (createNotifier) {
		_notifier = new QSocketNotifier(s->fd(), QSocketNotifier::Read,
                                        parent, name);
		_notifier->setEnabled(FALSE);
	}
}

Socket::~Socket()
{
	delete _notifier;
    delete _socket;
}

bool Socket::write(const QByteArray &a)
{
	return ( _socket->writeBlock(a.data(), a.size())==(int)a.size() );
}

bool Socket::write()
{
	bool res = write(writing.buffer());
	writing.clear();
	return res;
}

int Socket::pendingData() const
{
	int size = 0;
	if ( ioctl(_socket->fd(), FIONREAD, (char *)&size)<0 ) return -1;
	return size;
}

int Socket::read()
{
	reading.clearRead();

	int size = pendingData();
	if ( size==-1 ) return -1;

	reading.device()->close();
	int dec = reading.size();
	reading.buffer().resize(dec + size);
	size = _socket->readBlock(reading.buffer().data() + dec, size);
	if ( size==-1 ) reading.buffer().resize(dec);
	reading.device()->open(IO_ReadOnly);

	return size;
}

int Socket::accept(KExtendedSocket *&s)
{
    return _socket->accept(s);
}
