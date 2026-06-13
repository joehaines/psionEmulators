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

#include "bool.h"
#include "../defaults.h"
#include "ncp.h"
#include "linkchan.h"
#include "bufferstore.h"
#include "link.h"
#include "bufferarray.h"

ncp::ncp(const char *fname, int baud, IOWatch &iow) {
  l = new link(fname, baud, iow, LINK_LAYER_DIAGNOSTICS);
  gotLinkChan = false;
  failed = false;
  
  // init channels
  for (int i=0; i < 8; i++) channelPtr[i] = NULL;
}

ncp::~ncp() {
  delete l;
}

void ncp::poll() {
  bufferArray res( l->poll() );
  if (!res.empty()) {
    do {
      bufferStore s = res.popBuffer();
      if (s.getLen() > 1) {
        int channel = s.getByte(0);
        s.discardFirstBytes(1);
        if (channel == 0) {
          decodeControlMessage(s);
        }
        else {
          /* int remChan = */ s.getByte(0);
          int allData = s.getByte(1);
          s.discardFirstBytes(2);
          if (channelPtr[channel] == NULL) {
            cerr << "Got message for unknown channel\n";
          }
          else {
            messageList[channel].addBuff(s);
            if (allData == LAST_MESS) {
              channelPtr[channel]->ncpDataCallback(messageList[channel]);
              messageList[channel].init();
            }
            else if (allData != NOT_LAST_MESS) {
              cerr << "ncp: bizarre third byte!\n";
            }
          }
        }
      }
      else {
        cerr << "Got null message\n"; 
      }
    } while (!res.empty());
  }
}

void ncp::controlChannel(int chan, enum interControllerMessageType t, bufferStore &command) {
  bufferStore open;
  open.addByte(0); // control
  open.addByte(chan);
  open.addByte(t);
  open.addBuff(command);
  l->send(open);
}

void ncp::decodeControlMessage(bufferStore &buff) {
  int remoteChan = buff.getByte(0);
  interControllerMessageType imt = (interControllerMessageType)buff.getByte(1);
  buff.discardFirstBytes(2);
  switch (imt) {
  case NCON_MSG_DATA_XOFF:
    cout << "NCON_MSG_DATA_XOFF " << remoteChan << "  " << buff << endl;
    break;
  case NCON_MSG_DATA_XON:
    cout << "NCON_MSG_DATA_XON " << remoteChan << "  " << buff << endl;
    break;
  case NCON_MSG_CONNECT_TO_SERVER: {
    cout << "NCON_MSG_CONNECT_TO_SERVER " << remoteChan << "  " << buff << endl;
    int localChan;
    {
      // Ack with connect response
      bufferStore b;
      localChan = getFirstUnusedChan();
      b.addByte(remoteChan);
      b.addByte(0x0);
      controlChannel(localChan, NCON_MSG_CONNECT_RESPONSE, b);
    }
    if (!strcmp(buff.getString(0), "LINK.*")) {
      if (gotLinkChan) failed = true;
      cout << "Accepted link channel" <<endl;
      channelPtr[localChan] = new linkChan(this);
      channelPtr[localChan]->setNcpChannel(localChan);
      channelPtr[localChan]->ncpConnectAck();
      gotLinkChan = true;
    }
    else {
      cout << "Disconnecting channel" <<endl;
      bufferStore b;
      b.addByte(remoteChan);
      controlChannel(localChan, NCON_MSG_CHANNEL_DISCONNECT, b);
    }
  }
  break;
  case NCON_MSG_CONNECT_RESPONSE: {
    int forChan = buff.getByte(0);
    cout << "NCON_MSG_CONNECT_RESPONSE " << remoteChan << "  for channel " << forChan << " status ";
    if (buff.getByte(1) == 0) {
      cout << "OK" <<endl;
      if (channelPtr[forChan]) {
        remoteChanList[forChan] = remoteChan;
        channelPtr[forChan]->ncpConnectAck();
      }
      else {
        cerr << "Got message for unknown channel\n";
      }
    }
    else {
      cout << "Unknown status "<<(int)buff.getByte(1) << endl;
      channelPtr[forChan]->ncpConnectTerminate();
    }
  }
  break;
  case NCON_MSG_CHANNEL_CLOSED:
    cout << "NCON_MSG_CHANNEL_CLOSED " << remoteChan << "  " << buff << endl;
    break;
  case NCON_MSG_NCP_INFO:
    cout << "NCON_MSG_NCP_INFO " << remoteChan << "  " << buff << endl;
    {
      // Send time info
      bufferStore b;
      b.addByte(2);
      b.addByte(0x0);
      b.addByte(0x0);
      b.addByte(0x0);
      b.addByte(0x0);
      controlChannel(0, NCON_MSG_NCP_INFO, b);
    }
    break;
  case NCON_MSG_CHANNEL_DISCONNECT:
    cout << "NCON_MSG_CHANNEL_DISCONNECT " << remoteChan << "  channel " << (int)buff.getByte(0) << endl;
    disconnect(buff.getByte(0));
    break;
  case NCON_MSG_NCP_END:
    cout << "NCON_MSG_NCP_END " << remoteChan << "  " << buff << endl;
    break;
  default:
    cerr << "Unknown Inter controller message type\n";
  }
}

int ncp::getFirstUnusedChan() {
  for (int cNum=1; cNum < 8; cNum++) {
    if (channelPtr[cNum] == NULL) {
      return cNum;
    }
  }
  return 0;
}

int ncp::connect(channel *ch) {
  // look for first unused chan
  int cNum = getFirstUnusedChan();
  if (cNum > 0) {
    channelPtr[cNum] = ch;
    ch->setNcpChannel(cNum);
    bufferStore b;
    b.addString(ch->getNcpConnectName());
    b.addByte(0);
    controlChannel(cNum, NCON_MSG_CONNECT_TO_SERVER, b);
    return cNum;
  }
  return -1;
}

void ncp::send(int channel, bufferStore &a) {
  bool last;
  do {
    last = true;
    
    if (a.getLen() > NCP_SENDLEN)
      last = false;

    bufferStore out;
    out.addByte(remoteChanList[channel]);
    out.addByte(channel);

    if (last) {
      out.addByte(LAST_MESS);
    }
    else {
      out.addByte(NOT_LAST_MESS);
    }

    out.addBuff(a, NCP_SENDLEN);
    a.discardFirstBytes(NCP_SENDLEN);
    l->send(out);
  } while (!last);
}

void ncp::disconnect(int channel) {
  channelPtr[channel]->terminateWhenAsked();
  channelPtr[channel] = NULL;
  bufferStore b;
  b.addByte(remoteChanList[channel]);
  controlChannel(channel, NCON_MSG_CHANNEL_DISCONNECT, b);
}

bool ncp::stuffToSend() {
  return l->stuffToSend();
}

bool ncp::hasFailed() {
  if (failed) return true;
  return l->hasFailed();
}

bool ncp::gotLinkChannel() {
  return gotLinkChan;
}
