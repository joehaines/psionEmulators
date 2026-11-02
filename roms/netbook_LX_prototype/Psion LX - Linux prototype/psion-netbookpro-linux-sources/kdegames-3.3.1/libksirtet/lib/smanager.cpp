#include "smanager.h"

#include <config.h>

#include <ctype.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <strings.h>


SocketManager::SocketManager()
{
	max_fd = 0;
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	nbWriteable = 0;
}

SocketManager::~SocketManager()
{
	clean();
}

void SocketManager::clean()
{
	for(uint i=0; i<sockets.size(); i++) delete sockets[i];
	sockets.resize(0);
}

int SocketManager::find(int fd)
{
	for(uint i=0; i<sockets.size(); i++)
		if ( sockets[i]->fd()==fd ) return i;
	return -1;
}

uint SocketManager::append(Socket *socket, SocketProperty sp)
{
	uint s = sockets.size();
	sockets.resize(s+1);
	sockets[s] = socket;

	max_fd = QMAX(max_fd, socket->fd());

	if ( sp==ReadWrite || sp==ReadOnly ) FD_SET(socket->fd(), &read_set);
	if ( sp==ReadWrite || sp==WriteOnly ) {
		nbWriteable++;
		FD_SET(socket->fd(), &write_set);
	}

	return s;
}

void SocketManager::remove(uint i, bool del)
{
	Socket *so = sockets[i];

	uint s = sockets.size()-1;
	for(uint j=i; j<s; j++) sockets[j] = sockets[j+1];
	sockets.resize(s);

	max_fd = 0;
	for(uint j=0; j<s; j++)
		max_fd = QMAX(max_fd, sockets[j]->fd());

	int fd = so->fd();
	if ( FD_ISSET(fd, &read_set) ) FD_CLR(fd, &read_set);
	if ( FD_ISSET(fd, &write_set) ) {
		nbWriteable--;
		FD_CLR(fd, &write_set);
	}

    if (del) delete so;
}

bool SocketManager::canWriteAll(uint sec, uint usec)
{
	write_tmp = write_set;
	tv.tv_sec = sec;
    tv.tv_usec = usec;
    return ( select(max_fd+1, 0, &write_tmp, 0, &tv)==(int)nbWriteable );
}

bool SocketManager::canWrite(uint i, uint sec, uint usec)
{
	int fd = sockets[i]->fd();
	FD_ZERO(&write_tmp);
	FD_SET(fd, &write_tmp);
	tv.tv_sec = sec;
	tv.tv_usec = usec;
	return ( select(fd+1, 0, &write_tmp, 0, &tv)==1 );
}

bool SocketManager::checkPendingData(uint sec, uint usec)
{
	read_tmp = read_set;
	tv.tv_sec = sec;
    tv.tv_usec = usec;
	return ( select(max_fd+1, &read_tmp, 0, 0, &tv)!=-1 );
}

bool SocketManager::dataPending(uint i)
{
	int fd = sockets[i]->fd();
	return FD_ISSET(fd, &read_tmp);
}

bool SocketManager::writeCommon(uint i)
{
	return sockets[i]->write(writing.buffer());
}
