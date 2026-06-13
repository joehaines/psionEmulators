#ifndef _ftp_h_
#define _ftp_h_

#include "bool.h"

class rfsv;
class bufferStore;
class bufferArray;

class ftp {
public:
  ftp();
  ~ftp();
  int session(rfsv &a);

private:
  void getCommand(char* buf, int bufLen);

  // Unix utilities
  bool unixDirExists(const char* dir);
  void getUnixDir(bufferArray &files);

  void cd(const char *source, const char* cdto, char *dest);
  
  char localDir[300];
  char psionDir[300];
};

#endif

