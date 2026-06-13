//
//  PLP - An implementation of the PSION link protocol
//
//  Copyright (C) 1999  Philip Proudman
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  e-mail philip.proudman@btinternet.com

#include <stream.h>
#include <stdlib.h>
#include <fstream.h>
#include <iomanip.h>
#include <time.h>
#include <string.h>

#include "bool.h"
#include "bool.h"
#include "rfsv.h"
#include "bufferstore.h"
#include "ppsocket.h"
#include "../defaults.h"
#include "bufferarray.h"

rfsv::rfsv(ppsocket *_skt) {
  skt = _skt;
  bufferStore a;
  a.addStringT( getConnectName() );
  if (skt->sendBufferStore(a)) {
    if (skt->getBufferStore(a) == 1) {
      if (strcmp(a.getString(0), "Ok")) {
        cerr << "Not got ok over socket\n";
	abort();
      }
    }
  }
}

rfsv::~rfsv() {
  bufferStore a;
  a.addStringT("Close");
  skt->sendBufferStore(a);
  skt->closeSocket();
}

const char *rfsv::getConnectName() {
  return "SYS$RFSV.*";
}

int rfsv::fopen(fopen_attrib attr, const char* file, int &handle) { 
  bufferStore a;
  a.addWord(attr);
  a.addString(file);
  a.addByte(0);
  sendCommand(FOPEN, a);
  
  bool res = getResponse(a);
  if (res && a.getLen() == 2 && a.getWord(0) == 0xffd6) {
    cerr << "No such file "<<file <<endl;
    return NO_SUCH_FILE;
  }
  else if (res && a.getLen() == 4) {
    handle = a.getWord(2);
    return a.getWord(0);
  }
  cerr << "Unknown response to fopen " << attr << " " << file << " " << a <<endl;
  return UNKNOWN;
}

int rfsv::fclose(int fileHandle) {
  bufferStore a;
  a.addWord(fileHandle);
  sendCommand(FCLOSE, a);
  bool res = getResponse(a);
  if (res && a.getLen() == 2) return a.getWord(0);
  cerr << "Unknown response from fclose "<< a <<endl;
  return 1;
}

int rfsv::dir(const char* dirName, bufferArray *files) {
  int fileHandle;
  char realName[200];
  int rv = rfsv::convertName(dirName, realName);
  if (rv) return rv;

  int status = rfsv:: fopen(P_FDIR, realName, fileHandle);
  if (status != 0) {
    return status;
  }

  bufferStore a;
  a.addWord(fileHandle);
  sendCommand(FDIRREAD, a);
  
  bool res = getResponse(a);

  if (!files) {
    cout << "w = writeable" << endl;
    cout << "h = hidden" << endl;
    cout << "s = system" << endl;
    cout << "v = volume" << endl;
    cout << "d = directory" << endl;
    cout << "m = modified" << endl;
    cout << "r = readable" << endl;
    cout << "e = executable" << endl;
    cout << "s = stream" << endl;
    cout << "t = text" << endl << endl;
  }
  
  if (res) {
    a.discardFirstBytes(4); // Don't know what these mean!
    while (a.getLen() > 16) {
      int version = a.getWord(0);
      if (version != 2) return 1;
      int status = a.getWord(2);
      long size = a.getDWord(4);
      long date = a.getDWord(8);
      const char *s = a.getString(16);
      a.discardFirstBytes(17+strlen(s));

      if (!files) {
	cout << ((status & P_FAWRITE) ? "w" : "-");
	cout << ((status & P_FAHIDDEN) ? "h" : "-");
	cout << ((status & P_FASYSTEM) ? "s" : "-");
	cout << ((status & P_FAVOLUME) ? "v" : "-");
	cout << ((status & P_FADIR) ? "d" : "-");
	cout << ((status & P_FAMOD) ? "m" : "-");
	cout << ((status & P_FAREAD) ? "r" : "-");
	cout << ((status & P_FAEXEC) ? "e" : "-");
	cout << ((status & P_FASTREAM) ? "s" : "-");
	cout << ((status & P_FATEXT) ? "t" : "-");
	
	cout << " " << dec << setw(10) << setfill(' ') << size;
	
	{
	  char dateBuff[100];
	  struct tm *t;
	  t = localtime(&date);
	  strftime(dateBuff, 100, "%d/%m/%y", t);
	  cout << " " << dateBuff;
	}
	     
	cout << " " << s <<endl;
      }
      else if (!(status & P_FADIR)) {
	bufferStore temp;
	temp.addStringT(s);
	files->pushBuffer(temp);
      }
    }	
  }
  else {
    cerr << "Unknown response to dir "<<a<<endl;
    return 1;
  }
  
  rfsv::fclose(fileHandle);
  return 0;
}

bool rfsv::sendCommand(enum commands cc, bufferStore &data) {
  bufferStore a;
  a.addWord(cc);
  a.addWord(data.getLen());
  a.addBuff(data);
  return skt->sendBufferStore(a);
}


bool rfsv::getResponse(bufferStore &a) {
  if (skt->getBufferStore(a) == 1 &&
      a.getWord(0) == 0x2a &&
      a.getWord(2) == a.getLen()-4) {
    a.discardFirstBytes(4);
    return true;
  }
  cout << a.getWord(2) << " " << a.getLen()-4<<endl;
  return false;
}

int rfsv::read(const char* psionName, const char* localName) {
  int fileHandle;
  char realName[200];
  int rv = rfsv::convertName(psionName, realName);
  if (rv) return rv;

  int status = rfsv:: fopen(P_FOPEN, realName, fileHandle);
  if (status != 0) {
    return status;
  }
  ofstream op(localName);
  if (!op) {
    cerr << "Could not open file\n";
    return 1;
  }
    
  do {
    bufferStore a;
    a.addWord(fileHandle);
    a.addWord(300); /* ? */
    sendCommand(FREAD, a);
      
    bool res = getResponse(a);
    if (res) {
      long status = a.getWord(0);
      a.discardFirstBytes(2);
      if (status != 0) break;
      op.write( a.getString(), a.getLen() );
    }
    else {
      cerr << "Unknown response to fread "<<a<<endl;
      return 1;
    }
  } while (true);
  
  rfsv::fclose(fileHandle);
  op.close();
  return 0;
}

int rfsv::write(const char* localName, const char* psionName) {
  int fileHandle;
  char realName[200];
  int rv = rfsv::convertName(psionName, realName);
  if (rv) return rv;

  int status = rfsv:: fopen(P_FREPLACE, realName, fileHandle);
  if (status != 0) {
    return status;
  }
  ifstream ip(localName);
  if (!ip) {
    cerr << "Could not open unix file\n";
    return 1;
  }
  unsigned char * buff = new unsigned char [RFSV_SENDLEN];
  while (ip &&!ip.eof()) {
    ip.read(buff, RFSV_SENDLEN);
    bufferStore tmp(buff, ip.gcount());
    if (tmp.getLen() == 0) break;
    bufferStore a;
    a.addWord(fileHandle);
    a.addBuff(tmp);
    sendCommand(FWRITE, a);    
      
    bool res = getResponse(a);
    if (res) {
      long status = a.getWord(0);
      if (status != 0) {
	cerr << "Send failed\n";
	return 1;
      }
    }
    else {
      cerr << "Unknown response to fwrite "<<a<<endl;
      return 1;
    }
  }
  delete [] buff;
  
  rfsv::fclose(fileHandle);
  ip.close();
  return 0;
}

int rfsv::mkdir(const char* dirName) {
  char realName[200];
  int rv = rfsv::convertName(dirName, realName);
  if (rv) return rv;
  bufferStore a;
  a.addString(realName);
  sendCommand(MKDIR, a);
  bool res = getResponse(a);
  if (res && a.getLen() == 2) {
    // Correct response return a.getWord(0);
  }
  cerr << "Unknown response from mkdir "<< a <<endl;
  return 1;
}

int rfsv::del(const char* psionName) {
  char realName[200];
  int rv = rfsv::convertName(psionName, realName);
  if (rv) return rv;
  bufferStore a;
  a.addString(realName);
  sendCommand(DELETE, a);
  bool res = getResponse(a);
  if (res && a.getLen() == 2) {
    // Correct response
    return a.getWord(0);
  }
  cerr << "Unknown response from delete "<< a <<endl;
  return 1;
}

int rfsv::convertName(const char* orig, char *retVal) {
  int len = strlen(orig);
  char *temp = new char [len+1];

  for (int i=0; i <= len; i++) {
    if (orig[i] == '/')
      temp[i] = '\\';
    else
      temp[i] = orig[i];
  }

  if (len == 0 || temp[1] != ':') {
    // We can automatically supply a drive letter
    strcpy(retVal, DEFAULT_DRIVE);

    if (len == 0 || temp[0] != '\\') {
      strcat(retVal, DEFAULT_BASE_DIRECTORY);
    }

    strcat(retVal, temp);
  }
  else {
    strcpy(retVal, temp);
  }

  delete [] temp;
  return 0;
}


