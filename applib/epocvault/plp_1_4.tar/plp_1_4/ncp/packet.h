#ifndef _packet_h
#define _packet_h

#include <stdio.h>

#include "bool.h"
class psiEmul;
class bufferStore;
class IOWatch;

class packet {
public:
  packet(const char *fname, int baud, IOWatch &iow, bool verbose = false);
  ~packet();
  void send(unsigned char type, const bufferStore &b);
  bool get(unsigned char &type, bufferStore &b);
  
private:
  bool terminated();
  void addToCrc(unsigned short a);
  void opByte(unsigned char a);
  
  unsigned short crc;
  int inBufferPtr;
  unsigned char *inBuffer;
  int fd;
  bool verbose;
  char *devname;
  int baud;
  IOWatch &iow;
  
#ifdef _EMULATE_PSION_
  psiEmul* loader;
#endif
#ifdef _SAVE_PSION_FOR_EMULATION_
  psiEmul* saver;
#endif
};

#endif
