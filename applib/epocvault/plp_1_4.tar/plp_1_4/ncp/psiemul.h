#ifndef _pisemul_h_
#define _pisemul_h_

#include "bool.h"

// The following code does NOT emulate a psion, but it persists "conversations"
// with a psion. This is probably only useful for my debugging only.

class psiEmul {
public:
  psiEmul(iostream* io);
  
  void check(unsigned char a); // Is this an expected byte?
  void op(unsigned char a); // Sent to psion
  bool canRead(); // A byte from psion
  unsigned char get(); // A byte from psion
  void got(unsigned char a); // A byte from psion
private:
  void getNextByte();
  
  iostream* io;
  bool used;
  unsigned char nextByte;
  unsigned char direction;
};

#endif
