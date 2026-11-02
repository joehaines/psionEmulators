#ifndef SOCKET_H
#define SOCKET_H

#include <qsocketnotifier.h>

#include <kextsock.h>

#include "types.h"


class Socket
{
 public:
 	Socket(KExtendedSocket *, bool createNotifier = FALSE,
		   QObject *parent = 0, const char *name = 0);

	/** close the socket */
    ~Socket();

	int fd() const { return _socket->fd(); }

    /**
     * Accept a new socket.
     */
    int accept(KExtendedSocket *&);

	/**
	 * @return the socket notifier associated with the socket
	 * (0 if none).
	 */
	QSocketNotifier *notifier() const { return _notifier; }

	/**
	 * Write data contained in the writing stream to the socket.
	 * It clears the stream.
	 * @return TRUE if all was written without error.
	 */
	bool write();
	bool write(const QByteArray &a);

	/** @return the QDataStream for writing. */
	WritingStream &writingStream() { return writing; }

	/** @return the size of pending data. */
	int pendingData() const;

	/**
	 * Read data from socket and append them to reading stream for the specified socket.
	 * The portion of the stream that has been read is cleared.
	 * @return the read size or -1 on error
	 */
	int read();

	/** @return the reading stream. */
	ReadingStream &readingStream() { return reading; }

 private:
    KExtendedSocket *_socket;
	QSocketNotifier *_notifier;

	WritingStream writing;
	ReadingStream reading;
};

#endif // SOCKET_H
