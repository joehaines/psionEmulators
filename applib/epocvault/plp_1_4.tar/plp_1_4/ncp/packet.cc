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


#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <fstream.h>
#include <iomanip.h>

#include "bool.h"
#include "mp_serial.h"
#include "bufferstore.h"
#include "packet.h"
#include "iowatch.h"
#include "psiemul.h"

#define BUFFERLEN 2000

packet::packet(const char *fname, int _baud, IOWatch &_iow, bool _verbose) :
  iow(_iow)
{
  verbose = _verbose;
  devname = strdup(fname);
  baud = _baud;
  inBuffer = new unsigned char [BUFFERLEN];
  inBufferPtr = 0;
  
#ifdef _EMULATE_PSION_
  iostream *i = new fstream("psionemul", ios::in);
  loader = new psiEmul(i);
#else
#ifdef _SAVE_PSION_FOR_EMULATION_
  iostream *o = new fstream("psionemul", ios::out | ios::trunc);
  saver = new psiEmul(o);
#endif
  fd = init_serial(devname, baud, 0, 1);
  iow.addIO(fd);
#endif
}

packet::~packet() {
  iow.remIO(fd);
  ser_exit(fd);
  usleep(100000);

  delete [] inBuffer;
  free(devname);
}

void packet::send(unsigned char type, const bufferStore &b) {
  if (verbose) cout <<  "packet: send ";
  opByte(0x16);
  opByte(0x10);
  opByte(0x02);

  crc = 0;
  opByte(type);
  addToCrc(type);

  long len = b.getLen();
  for (int i=0; i<len; i++) {
    unsigned char c = b.getByte(i);
    if (c == 0x10) {
      opByte(c);
    }
    opByte(c);
    addToCrc(c);
  }

  opByte(0x10);
  opByte(0x03);

  opByte(crc >> 8);
  opByte(crc & 0xff);
  if (verbose) cout << endl;
}

void packet::addToCrc(unsigned short c) {
  c <<= 8;
  for (int i=0; i<8; i++) {
    if ( (crc ^ c) & 0x8000) {
      crc = (crc << 1) ^ 0x1021;
    }
    else {
      crc <<= 1;
    }
    
    c <<= 1;
  }
}

void packet::opByte(unsigned char a) {
#ifdef _EMULATE_PSION_
  loader->check(a);
#else
#ifdef _SAVE_PSION_FOR_EMULATION_
  saver->op(a);
#endif
  write(fd, &a, 1);
  if (verbose) cout <<  hex << setw(2) << setfill('0') << (int)a << " ";
#endif
}

bool packet::get(unsigned char &type, bufferStore &ret) {
  while (!terminated()) {
    int res;
#ifdef _EMULATE_PSION_
    if (!loader->canRead()) return false;
    inBuffer[inBufferPtr] = loader->get();
#else
    do {
      fd_set io;
      FD_ZERO(&io);
      FD_SET(fd, &io);
      struct timeval t;
      t.tv_usec = 0;
      t.tv_sec = 0;
      if (!select(fd+1, &io, NULL, NULL, &t)) return false;
      res = read(fd, &(inBuffer[inBufferPtr]), 1);
    } while (res != 1);
#ifdef _SAVE_PSION_FOR_EMULATION_    
    saver->got(inBuffer[inBufferPtr]);
#endif
#endif
    inBufferPtr++;
    if (inBufferPtr == 1 && inBuffer[0] != 0x16) inBufferPtr = 0;
    else if (inBufferPtr == 2 && inBuffer[1] != 0x10) inBufferPtr = 0;
    else if (inBufferPtr == 3 && inBuffer[2] != 0x2) inBufferPtr = 0;
    else if (inBufferPtr == BUFFERLEN) inBufferPtr = 0; // Wow, that shouldn't happen!
  }
  if (terminated()) {
    crc = 0;
    bool escaped = false;
    ret.init();
    for (int i = 3; i < inBufferPtr-4; i++) {
      unsigned char cc = inBuffer[i];
      if (escaped) {
	addToCrc(cc);
	ret.addByte(cc);
	escaped = false;
      }	
      else if (cc != 0x10) {
	addToCrc(cc);
	ret.addByte(cc);
      }
      else
	escaped = true;
    }
    if (verbose) {
      cout << "packet: get ";
      for (int i = 0; i < inBufferPtr; i++)
	cout <<  hex << setw(2) << setfill('0') << (int)inBuffer[i] << " ";
      cout << endl;
    }
    if (inBuffer[inBufferPtr-2] == ((crc >> 8) & 0xff) &&
	inBuffer[inBufferPtr-1] == (crc & 0xff)) {
      type = inBuffer[3];
      ret.discardFirstBytes(1);
      inBufferPtr = 0;
      return true;
    }
    else
      cout << "packet::Warning - bad crc packet " <<endl;
  }
  inBufferPtr = 0;
  return false;
}

bool packet::terminated() {
  if (inBufferPtr <= 5 ||
      inBuffer[inBufferPtr - 3] != 0x03 ||
      inBuffer[inBufferPtr - 4] != 0x10)
    return false;
  
  for (int i = inBufferPtr - 5; i > 0; i-=2) {
    if (inBuffer[i] != 0x10) return true;
    if (inBuffer[i-1] != 0x10) return false;
  }
  return false;
}
