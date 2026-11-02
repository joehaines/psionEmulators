/*
 * Simple Network Support
 * Install a listening socket; receive positions on incoming
 * connections (incoming positions are treated as if pasted in)
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <sys/types.h>
#include <netinet/in.h>

#include <qobject.h>
#include <qptrlist.h>
#include <qsocketnotifier.h>

class Listener {
 public:
  Listener(const char*,int,struct sockaddr_in,bool=true);
  
  char host[100];
  int port;
  struct sockaddr_in sin;
  bool reachable;
};
  

class Network: public QObject
{
  Q_OBJECT
    
 public:
  enum { defaultPort = 23412 };

  /* install listening TCP socket on port */
  Network(int port = defaultPort);
  ~Network();

  bool isOK() { return (fd>=0); }
  void addListener(const char* host, int port);  
  void broadcast(const char* pos);

 signals:
  void gotPosition(const char* pos);

 private slots:
  void gotConnection();
  
 private:
  bool sendString(struct sockaddr_in sin, char* str, int len);

  QPtrList<Listener> listeners;
  struct sockaddr_in mySin;
  int fd, myPort;
  QSocketNotifier *sn;
};

#endif

  
