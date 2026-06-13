// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal LCP per RFC 1661/1662.
//
// State machine is collapsed to: Closed → Req-Sent → Ack-Sent → Opened
// + Echo handler + Terminate handler. Configure-Request from the peer
// is acked option-by-option; anything we don't recognise gets
// Configure-Reject. Authentication-Protocol (option 3) defaulting to
// CHAP gets Configure-Nak'd back to PAP so we don't have to implement
// CHAP/MS-CHAP for the dial-up path.

export const LCP_CONF_REQ  = 1;
export const LCP_CONF_ACK  = 2;
export const LCP_CONF_NAK  = 3;
export const LCP_CONF_REJ  = 4;
export const LCP_TERM_REQ  = 5;
export const LCP_TERM_ACK  = 6;
export const LCP_CODE_REJ  = 7;
export const LCP_PROTO_REJ = 8;
export const LCP_ECHO_REQ  = 9;
export const LCP_ECHO_REP  = 10;
export const LCP_DISC_REQ  = 11;

export const OPT_MRU             = 1;
export const OPT_ASYNC_CTRL_MAP  = 2;
export const OPT_AUTH_PROTOCOL   = 3;
export const OPT_QUALITY         = 4;
export const OPT_MAGIC_NUMBER    = 5;
export const OPT_PFC             = 7;
export const OPT_ACFC            = 8;

export type LcpState = 'closed' | 'req-sent' | 'ack-sent' | 'opened';

export interface LcpEvent {
  // A frame the LCP wants the dispatcher to transmit (already wrapped
  // as Configure-Request / Ack / Nak / Reject / Echo-Reply).
  send?: Uint8Array;
  // State change signal — when state becomes 'opened', dispatcher
  // proceeds to PAP/IPCP.
  stateChanged?: LcpState;
  // Authentication protocol the peer ended up agreeing to (set when
  // Configure-Ack is processed).
  authProtocol?: number;
  // Negotiated peer ACCM (option 2), if any.
  peerAccm?: number;
}

export class Lcp {
  state: LcpState = 'closed';
  ourMagic: number = (Math.random() * 0xFFFFFFFF) >>> 0;
  peerAccm: number = 0xFFFFFFFF;
  authProto: number = 0; // 0 = none agreed yet
  private id: number = 1;

  // Called once when the host wants to bring the link up. Emits the
  // initial Configure-Request and moves into 'req-sent'.
  open(): Uint8Array {
    const ourReq = this.buildConfigureRequest();
    this.state = 'req-sent';
    return ourReq;
  }

  // Process an inbound LCP info-frame. Returns 0..N transmissions and
  // possible state-change signals.
  recv(info: Uint8Array): LcpEvent[] {
    if (info.length < 4) return [];
    const code = info[0];
    const id   = info[1];
    const len  = (info[2] << 8) | info[3];
    const body = info.subarray(4, len);

    switch (code) {
      case LCP_CONF_REQ:   return this.handleConfReq(id, body);
      case LCP_CONF_ACK:   return this.handleConfAck(id, body);
      case LCP_CONF_NAK:   return this.handleConfNak(id, body);
      case LCP_CONF_REJ:   return this.handleConfRej(id, body);
      case LCP_TERM_REQ:   return [{ send: this.encode(LCP_TERM_ACK, id, body) }, { stateChanged: 'closed' }];
      case LCP_ECHO_REQ:   return [{ send: this.encode(LCP_ECHO_REP, id, this.magicBytes(body.subarray(4))) }];
      case LCP_ECHO_REP:   return [];
      default:
        // Unknown code → Code-Reject (echo the offending packet).
        return [{ send: this.encode(LCP_CODE_REJ, this.nextId(), info.subarray(0, len)) }];
    }
  }

  // Compose a fresh Configure-Request advertising what we'd like.
  // We're minimalist: just our magic number. We don't ask for MRU
  // because we accept whatever the peer asks for.
  private buildConfigureRequest(): Uint8Array {
    // Option Magic-Number (5), length 6, 4-byte magic.
    const opts = new Uint8Array(6);
    opts[0] = OPT_MAGIC_NUMBER;
    opts[1] = 6;
    opts[2] = (this.ourMagic >>> 24) & 0xFF;
    opts[3] = (this.ourMagic >>> 16) & 0xFF;
    opts[4] = (this.ourMagic >>> 8) & 0xFF;
    opts[5] =  this.ourMagic & 0xFF;
    return this.encode(LCP_CONF_REQ, this.nextId(), opts);
  }

  // Peer wants to configure their side. Walk options, ack what we
  // understand, nak Authentication-Protocol=CHAP → PAP, reject the
  // rest.
  private handleConfReq(id: number, body: Uint8Array): LcpEvent[] {
    const ackOpts: number[] = [];
    const nakOpts: number[] = [];
    const rejOpts: number[] = [];
    let peerAccm = 0xFFFFFFFF;
    let acceptedAuth = 0;

    let i = 0;
    while (i + 1 < body.length) {
      const type = body[i];
      const olen = body[i + 1];
      if (olen < 2 || i + olen > body.length) {
        // Malformed option — reject the rest verbatim.
        for (let j = i; j < body.length; j++) rejOpts.push(body[j]);
        break;
      }
      const odata = body.subarray(i + 2, i + olen);
      switch (type) {
        case OPT_MRU: {
          // Accept any MRU up to 1500; if peer asks for more, nak with 1500.
          const requested = odata.length === 2 ? (odata[0] << 8) | odata[1] : 1500;
          if (requested > 1500) {
            nakOpts.push(OPT_MRU, 4, 0x05, 0xDC);
          } else {
            for (let j = 0; j < olen; j++) ackOpts.push(body[i + j]);
          }
          break;
        }
        case OPT_ASYNC_CTRL_MAP: {
          if (odata.length === 4) {
            peerAccm = ((odata[0] << 24) | (odata[1] << 16) | (odata[2] << 8) | odata[3]) >>> 0;
          }
          for (let j = 0; j < olen; j++) ackOpts.push(body[i + j]);
          break;
        }
        case OPT_AUTH_PROTOCOL: {
          // odata is a 2-byte protocol field, optionally followed by
          // protocol-specific data (CHAP carries the algorithm byte).
          const proto = odata.length >= 2 ? (odata[0] << 8) | odata[1] : 0;
          if (proto === 0xC023 /* PAP */) {
            for (let j = 0; j < olen; j++) ackOpts.push(body[i + j]);
            acceptedAuth = 0xC023;
          } else {
            // Nak: counter-propose PAP.
            nakOpts.push(OPT_AUTH_PROTOCOL, 4, 0xC0, 0x23);
          }
          break;
        }
        case OPT_MAGIC_NUMBER: {
          // Peer's magic — accept as-is.
          for (let j = 0; j < olen; j++) ackOpts.push(body[i + j]);
          break;
        }
        case OPT_PFC:
        case OPT_ACFC: {
          for (let j = 0; j < olen; j++) ackOpts.push(body[i + j]);
          break;
        }
        case OPT_QUALITY:
        default: {
          // Reject — we don't speak this option.
          for (let j = 0; j < olen; j++) rejOpts.push(body[i + j]);
          break;
        }
      }
      i += olen;
    }

    const events: LcpEvent[] = [];

    if (rejOpts.length > 0) {
      events.push({ send: this.encode(LCP_CONF_REJ, id, Uint8Array.from(rejOpts)) });
    } else if (nakOpts.length > 0) {
      events.push({ send: this.encode(LCP_CONF_NAK, id, Uint8Array.from(nakOpts)) });
    } else {
      events.push({ send: this.encode(LCP_CONF_ACK, id, Uint8Array.from(ackOpts)) });
      this.peerAccm = peerAccm;
      if (acceptedAuth !== 0) this.authProto = acceptedAuth;
      // Acking the peer brings us closer to 'opened' — if we've
      // already had our Conf-Req acked, the link is up now.
      if (this.state === 'ack-sent') {
        this.state = 'opened';
        events.push({ stateChanged: 'opened', authProtocol: this.authProto, peerAccm: this.peerAccm });
      } else if (this.state === 'req-sent') {
        this.state = 'ack-sent';
        events.push({ stateChanged: 'ack-sent' });
      }
    }
    return events;
  }

  private handleConfAck(_id: number, _body: Uint8Array): LcpEvent[] {
    // The peer accepted our Configure-Request. Promote state.
    if (this.state === 'ack-sent') {
      this.state = 'opened';
      return [{ stateChanged: 'opened', authProtocol: this.authProto, peerAccm: this.peerAccm }];
    }
    if (this.state === 'req-sent') {
      this.state = 'ack-sent';
      return [{ stateChanged: 'ack-sent' }];
    }
    return [];
  }

  private handleConfNak(_id: number, _body: Uint8Array): LcpEvent[] {
    // Peer wants different options for our Configure-Request. We're
    // minimalist — re-send our request with a new ID; we don't
    // change anything.
    return [{ send: this.buildConfigureRequest() }];
  }

  private handleConfRej(_id: number, _body: Uint8Array): LcpEvent[] {
    // Peer rejected our magic — strip it and re-send.
    return [{ send: this.encode(LCP_CONF_REQ, this.nextId(), new Uint8Array(0)) }];
  }

  private magicBytes(rest: Uint8Array): Uint8Array {
    // Echo-Reply body: 4-byte magic-number (ours) + rest of Echo-Request data.
    const out = new Uint8Array(4 + rest.length);
    out[0] = (this.ourMagic >>> 24) & 0xFF;
    out[1] = (this.ourMagic >>> 16) & 0xFF;
    out[2] = (this.ourMagic >>> 8) & 0xFF;
    out[3] =  this.ourMagic & 0xFF;
    out.set(rest, 4);
    return out;
  }

  // Wrap a code/ID/data triplet in the LCP packet format.
  encode(code: number, id: number, data: Uint8Array): Uint8Array {
    const len = 4 + data.length;
    const out = new Uint8Array(len);
    out[0] = code;
    out[1] = id;
    out[2] = (len >>> 8) & 0xFF;
    out[3] = len & 0xFF;
    out.set(data, 4);
    return out;
  }

  // Build an Echo-Request — used as a 30 s keepalive.
  buildEchoRequest(): Uint8Array {
    const body = new Uint8Array(4);
    body[0] = (this.ourMagic >>> 24) & 0xFF;
    body[1] = (this.ourMagic >>> 16) & 0xFF;
    body[2] = (this.ourMagic >>> 8) & 0xFF;
    body[3] =  this.ourMagic & 0xFF;
    return this.encode(LCP_ECHO_REQ, this.nextId(), body);
  }

  private nextId(): number {
    const v = this.id;
    this.id = (this.id + 1) & 0xFF;
    return v;
  }
}
