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


// The following code does NOT emulate a psion, but it persists "conversations"
// with a psion. This is probably only useful for my debugging only.

#include <iostream.h>
#include <stdlib.h>

#include "psiemul.h"

#if defined(_EMULATE_PSION_) || defined(_SAVE_PSION_FOR_EMULATION_)

#define OUT (unsigned char)1
#define IN (unsigned char)0

psiEmul::psiEmul(iostream* _io) {
  io = _io;
  used = true;
}
  
void psiEmul::check(unsigned char a) {
  getNextByte();
  if (direction != OUT || nextByte != a) abort();
  used = true;
}

void psiEmul::op(unsigned char a) {
  io->put(OUT);
  io->put(a);
  io->flush();
}

bool psiEmul::canRead() {
  getNextByte();
  return direction == IN;
}

unsigned char psiEmul::get() {
  getNextByte();
  if (direction != IN) abort();
  used = true;
  return nextByte;
}

void psiEmul::got(unsigned char a) {
  io->put(IN);
  io->put(a);
  io->flush();
}

void psiEmul::getNextByte() {
  if (used) {
    used = false;
    io->get(direction);
    io->get(nextByte);
  }
}

#endif
