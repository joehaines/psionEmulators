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

#include <sys/types.h>
#include <dirent.h>
#include <stream.h>
#include <string.h>
#include <ctype.h>

#include "../defaults.h"
#include "ftp.h"
#include "rfsv.h"
#include "bufferarray.h"
#include "bufferstore.h"

ftp::ftp() {
  strcpy(localDir, DEFAULT_FTP_DIR);
  strcpy(psionDir, DEFAULT_DRIVE);
  strcat(psionDir, DEFAULT_BASE_DIRECTORY);
}

ftp::~ftp() {
}

int ftp::session(rfsv &a) {
  char command[300];
  do {
    getCommand(command, 300);
    
    if (!strcmp(command, "pwd")) {
      cout << "Local dir: \"" << localDir << "\"" << endl;
      cout << "Psion dir: \"" << psionDir << "\"" << endl;
    }
    
    else if (!strcmp(command, "dir")) {
      if (a.dir(psionDir, NULL) != 0) cerr << "Command failed\n";
    }
    
    else if (!strncmp(command, "lcd", 3)) {
      if (command+3 == 0) {
	strcpy(localDir, DEFAULT_FTP_DIR);
      }
      else {
	char temp[300];
	cd(localDir, command+4, temp);
	strcpy(localDir, temp);
      }
    }
    
    else if (!strncmp(command, "cd", 2)) {
      if (command+2 == 0) {
	strcpy(psionDir, DEFAULT_DRIVE);
	strcat(psionDir, DEFAULT_BASE_DIRECTORY);
      }
      else {
	char temp[300];
	cd(psionDir, command+3, temp);
	bufferArray files;
	if (a.dir(temp, &files) == 0) {
	  strcpy(psionDir, temp);
	}
	else
	  cerr << "Keeping original directory \"" << psionDir << "\"" << endl;
      }
    }

    else if (!strncmp(command, "get", 3)) {
      char f1[300];
      char f2[300];
      strcpy(f1, psionDir); strcat(f1, command+4);
      strcpy(f2, localDir); strcat(f2, command+4);
      if (a.read(f1, f2) != 0)
	cerr << "Command failed\n";
      else
	cout << "Transfer complete\n";
    }
    
    else if (!strcmp(command, "mget")) {
      char f1[300];
      char f2[300];
      bufferArray files;
      a.dir(psionDir, &files);
      while (!files.empty()) {
	bufferStore s;
	s = files.popBuffer();
	char temp[100];
	do {
	  cout << "Get \"" << s.getString() << "\" y,n, or l (lowercase filename): ";
	  cout.flush();
	  cin.getline(temp, 100);
	} while (temp[1] != 0 || (temp[0] != 'y' && temp[0] != 'n' && temp[0] != 'l'));
	if (temp[0] != 'n') {
	  strcpy(f1, psionDir); strcat(f1, s.getString());
	  strcpy(f2, localDir); strcat(f2, s.getString());
	  if (temp[0] == 'l') {
	    for (char*p = f2; *p; p++) *p = tolower(*p);
	  }
	  if (a.read(f1, f2) != 0)
	    cerr << "Command failed\n";
	  else
	    cout << "Transfer complete\n";
	}
      }
    }
    
    else if (!strncmp(command, "put", 3)) {
      char f1[300];
      char f2[300];
      strcpy(f1, psionDir); strcat(f1, command+4);
      strcpy(f2, localDir); strcat(f2, command+4);
      if (a.write(f2, f1) != 0)
	cerr << "Command failed\n";
      else
	cout << "Transfer complete\n";
    }
    
    else if (!strcmp(command, "mput")) {
      DIR* d = opendir(localDir);
      if (d) {
	struct dirent *de;
	do {
	  de = readdir(d);
	  if (de) {
	    char temp[100];
	    do {
	      cout << "Put \"" << de->d_name << "\" y,n: ";
	      cout.flush();
	      cin.getline(temp, 100);
	    } while (temp[1] != 0 || (temp[0] != 'y' && temp[0] != 'n'));
	    if (temp[0] == 'y') {
	      char f1[300];
	      char f2[300];
	      strcpy(f1, psionDir); strcat(f1, de->d_name);
	      strcpy(f2, localDir); strcat(f2, de->d_name);
	      if (a.write(f2, f1) != 0)
		cerr << "Command failed\n";
	      else
		cout << "Transfer complete\n";
	    }
	  }
	} while (de);
	closedir(d);
      }
      else {
	cerr << "Error in directory name \"" << localDir << "\"\n";
      }
    }
    
    else if (!strncmp(command, "del", 3)) {
      char f1[300];
      strcpy(f1, psionDir); strcat(f1, command+4);
      if (a.del(f1) != 0)
	cerr << "No such file\n";
      else
	cout << "Ok";
    }
    
    else if (!strncmp(command, "mkdir", 5)) {
      char f1[300];
      strcpy(f1, psionDir); strcat(f1, command+6);
      if (a.mkdir(f1) != 0)
	cerr << "Command failed\n";
      else
	cout << "Ok";
    }
    
    else if (strcmp(command, "bye")) {
      cerr << "Unknown command" << endl;
      cerr << "  pwd" << endl;
      cerr << "  dir" << endl;
      cerr << "  cd <dir>" << endl;
      cerr << "  lcd <dir>" << endl;
      cerr << "  get <psion file>" << endl;
      cerr << "  put <unix file>" << endl;
      cerr << "  mget (works on whole directort, interactively, no wildcarding)" << endl;
      cerr << "  mput (works on whole directory, interactively, no wildcarding)" << endl;
      cerr << "  del <psion file>" << endl;
      cerr << "  mkdir <psion dir>" << endl;
      cerr << "  bye" << endl;
    }
    
  } while (strcmp(command, "bye"));
  return 0;
}

void ftp::getCommand(char* buf, int bufLen) {
  cout << "> "; cout.flush();
  cin.getline(buf, bufLen);
}

  // Unix utilities
bool ftp::unixDirExists(const char* dir) {
  return false;
}

void ftp::getUnixDir(bufferArray &files) {
}

void ftp::cd(const char *source, const char* cdto, char *dest) {
  if (cdto[0] == '/' || cdto[0] == '\\' || cdto[1] == ':') {
    strcpy(dest, cdto);
    char cc = dest[strlen(dest)-1];
    if (cc != '/' && cc != '\\')
      strcat(dest, "/");
  }
  else {
    char start[200];
    strcpy(start, source);
    
    while (*cdto) {
      char bit[200];
      int j;
      for (j=0; cdto[j] && cdto[j] != '/' && cdto[j] != '\\'; j++)
	bit[j] = cdto[j];
      bit[j] = 0;
      cdto += j; if (*cdto) cdto++;
      
      if (!strcmp(bit, "..")) {
	strcpy(dest, start);
	int i;
	for (i=strlen(dest)-2; i>=0; i--) {
	  if (dest[i] == '/' || dest[i] == '\\') {
	    dest[i+1] = 0;
	    break;
	  }
	}
      }
      else {
	strcpy(dest, start);
	strcat(dest, bit);
	strcat(dest, "/");
      }
      strcpy(start, dest);
    }
  }
}
