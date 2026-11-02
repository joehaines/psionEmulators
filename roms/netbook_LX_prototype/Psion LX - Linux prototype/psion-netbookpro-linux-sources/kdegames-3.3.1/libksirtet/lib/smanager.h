#ifndef SMANAGER_H
#define SMANAGER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>		// Needed on some systems.
#endif

#include <sys/time.h>

#include "socket.h"

/**
 * The SocketManager class is useful to manage (rw, ro, wo) sockets.
 *
 * You must add the sockets you want to manage to this class before other
 * operations. The sockets are stored in an array and other methods reference
 * the sockets by their index in that array.
 */
class SocketManager
{
 public:
 	SocketManager();

	/** Be aware that unremoved sockets will be closed there. */
	~SocketManager();

	/** Remove all sockets and close them. */
	void clean();

	enum SocketProperty { ReadOnly, WriteOnly, ReadWrite };

	/** @return the number of sockets. */
	uint size() const { return sockets.size(); };

    const Socket *operator[](uint i) const { return sockets[i]; }
	Socket *operator [](uint i)            { return sockets[i]; }

	/** @return the index of the socket (-1 if not present). */
	int find(int fd);

	/**
	 * Append a socket at the end of the array of sockets.
	 * @param sp determines if the socket will be used for ReadWrite,
	 * ReadOnly or WriteOnly operations.
	 * @return the index of the socket.
	 */
	uint append(Socket *, SocketProperty sp = ReadWrite);

	/**
	 * Remove the socket indexed <I>i</I>. Note that the following sockets in
	 * the array will have their index decremented by one.
     * @param deleteSocket if true, the socket is deleted
	 */
	void remove(uint i, bool deleteSocket);

	/** @return TRUE if it is possible to write to all the writeable sockets. */
	bool canWriteAll(uint sec = 0, uint usec = 0);

    /** @return TRUE if it is possible to write to the specified socket. */
	bool canWrite(uint i, uint sec = 0, uint usec = 0);

	Stream &commonWritingStream() { return writing; }
	bool writeCommon(uint i); // do not clear stream

	/**
	 * Check if there are pending data on at least one of the readeable
	 * socket.
	 */
	bool checkPendingData(uint sec = 0, uint usec = 0);

	bool dataPending(uint i);

 private:
	QMemArray<Socket *> sockets;

	fd_set read_set, write_set, read_tmp, write_tmp;
	struct timeval tv;
	int max_fd;
	uint nbWriteable;

	WritingStream writing;
};

#endif // SMANAGER_H
