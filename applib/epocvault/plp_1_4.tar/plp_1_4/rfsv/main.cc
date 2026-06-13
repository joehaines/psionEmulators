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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "bool.h"
#include "../defaults.h"
#include "ppsocket.h"
#include "rfsv.h"
#include "ftp.h"
#include "bufferstore.h"

void usage() {
  cout << "Version " << VERSION << endl;
  cout << "Usage : rfsv -s <socket number> <command> <parameters>\n";
  cout << "  " << FTP_COMMAND << " interactive, ftp like mode\n";
  cout << "  " << DIR_COMMAND << " <psion file (end directory names with a '/' to see the whole contents)>\n";
  cout << "  " << MKDIR_COMMAND << " <psion directory>\n";
  cout << "  " << DEL_COMMAND << " <psion file>\n";
  cout << "  " << READ_COMMAND << " <psion file> <unix file>\n";
  cout << "  " << WRITE_COMMAND << " <unix file> <psion file>\n";
}

void ftpHeader() {
  cout << "PLP Version " << VERSION;
  cout << " Copyright (C) 1999  Philip Proudman" << endl;
  cout << "PLP comes with ABSOLUTELY NO WARRANTY;" << endl;
  cout << "This is free software, and you are welcome to redistribute it" << endl;
  cout << "under certain conditions; see the COPYING file in the distribution" << endl;
  cout << endl;
  cout << "FTP like interface started. Type \"?\" for commands" << endl;
}

int main(int argc, char**argv) {
  ppsocket *skt;
  bool res;

  // Command line parameter processing
  int sockNum = DEFAULT_SOCKET;
  const char *com = NULL;
  const char *f1 = NULL;
  const char *f2 = NULL;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-s") && i+1 < argc) {
      sockNum = atoi(argv[++i]);
    }
    else if (!strcmp(argv[i], FTP_COMMAND) && i+1 == argc) {
      ftpHeader();
      com = argv[i++];
    }
    else if ((!strcmp(argv[i], DIR_COMMAND) ||
	      !strcmp(argv[i], DEL_COMMAND) ||
	      !strcmp(argv[i], MKDIR_COMMAND)) &&
             i+2 == argc) {
      com = argv[i++];
      f1 = argv[i];
    }
    else if ((!strcmp(argv[i], READ_COMMAND) ||
	      !strcmp(argv[i], WRITE_COMMAND)) &&
             i+3 == argc) {
      com = argv[i++];
      f1 = argv[i++];
      f2 = argv[i];
    }
    else {
      usage();
      exit(1);
    }
  }
  
  if (!com) {
    ftpHeader();
    com = FTP_COMMAND;
  }

  skt = new ppsocket();
  skt->startup();
  res = skt->connect(NULL, sockNum);
  if (!res) {
    delete skt;

    // Let's try to start a daemon
    char temp[200];
    sprintf(temp, "ncp -s %d > ncp.log &", sockNum);
    system(temp);
    
    for (int retry = 0; !res && retry < 40; retry++) {
      usleep(100000);
      skt = new ppsocket();
      skt->startup();
      res = skt->connect(NULL, sockNum);
      if (!res) delete skt;
    }
    
    if (!res) {
      delete skt;
      cout << "Can't connect to ncp daemon on socket " << sockNum << endl;
      return 1;
    }
    
    sleep(2); // Let the psion connect
  }
  
  rfsv a(skt);
  int status = 0;
  if (!strcmp(com, DIR_COMMAND)) status = a.dir(f1, NULL);
  if (!strcmp(com, READ_COMMAND)) status = a.read(f1, f2);
  if (!strcmp(com, WRITE_COMMAND)) status = a.write(f1, f2);
  if (!strcmp(com, MKDIR_COMMAND)) status = a.mkdir(f1);
  if (!strcmp(com, DEL_COMMAND)) status = a.del(f1);
  if (!strcmp(com, FTP_COMMAND)) {
    ftp f;
    status = f.session(a);
  }
  
  if (status != 0) cerr << "Command failed\n";
  
  return 0;
}
