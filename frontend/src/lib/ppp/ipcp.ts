// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IPCP (RFC 1332) — bring up the IP layer over PPP.
//
// Same Configure-Request/Ack/Nak/Reject dance as LCP. We hand the
// device a fixed IP and DNS, and our own end of the point-to-point
// link gets the gateway IP.
//
// Addresses:
//   device → 192.168.42.2
//   host   → 192.168.42.1   (gateway + DNS)
// VJ header compression (option 2) is rejected so the device drops
// it and sends plain IP.

export const IPCP_CONF_REQ = 1;
export const IPCP_CONF_ACK = 2;
export const IPCP_CONF_NAK = 3;
export const IPCP_CONF_REJ = 4;
export const IPCP_TERM_REQ = 5;
export const IPCP_TERM_ACK = 6;
export const IPCP_CODE_REJ = 7;

export const IPCP_OPT_VJ        = 0x02;
export const IPCP_OPT_IP        = 0x03;
export const IPCP_OPT_DNS_PRI   = 0x81;   // RFC 1877 primary DNS
export const IPCP_OPT_DNS_SEC   = 0x83;   // RFC 1877 secondary DNS

export const HOST_IP   = [192, 168, 42, 1] as const;
export const DEVICE_IP = [192, 168, 42, 2] as const;

export type IpcpState = 'closed' | 'req-sent' | 'ack-sent' | 'opened';

export interface IpcpEvent {
  send?: Uint8Array;
  stateChanged?: IpcpState;
  // The IP the device ended up with (informational, in case we
  // need to log or display it).
  deviceIp?: readonly number[];
}

export class Ipcp {
  state: IpcpState = 'closed';
  deviceIp: readonly number[] = DEVICE_IP;
  private id: number = 1;

  open(): Uint8Array {
    // Configure-Request: ask for our IP (192.168.42.1).
    const opt = new Uint8Array(6);
    opt[0] = IPCP_OPT_IP;
    opt[1] = 6;
    opt[2] = HOST_IP[0]; opt[3] = HOST_IP[1]; opt[4] = HOST_IP[2]; opt[5] = HOST_IP[3];
    this.state = 'req-sent';
    return this.encode(IPCP_CONF_REQ, this.nextId(), opt);
  }

  recv(info: Uint8Array): IpcpEvent[] {
    if (info.length < 4) return [];
    const code = info[0];
    const id   = info[1];
    const len  = (info[2] << 8) | info[3];
    const body = info.subarray(4, len);

    switch (code) {
      case IPCP_CONF_REQ: return this.handleConfReq(id, body);
      case IPCP_CONF_ACK: return this.handleConfAck();
      case IPCP_CONF_NAK: return this.handleConfNak();
      case IPCP_CONF_REJ: return this.handleConfRej();
      case IPCP_TERM_REQ: return [{ send: this.encode(IPCP_TERM_ACK, id, body) }, { stateChanged: 'closed' }];
      default: return [];
    }
  }

  private handleConfReq(id: number, body: Uint8Array): IpcpEvent[] {
    const ackOpts: number[] = [];
    const nakOpts: number[] = [];
    const rejOpts: number[] = [];

    let i = 0;
    while (i + 1 < body.length) {
      const type = body[i];
      const olen = body[i + 1];
      if (olen < 2 || i + olen > body.length) {
        for (let j = i; j < body.length; j++) rejOpts.push(body[j]);
        break;
      }
      const odata = body.subarray(i + 2, i + olen);
      switch (type) {
        case IPCP_OPT_IP: {
          // odata = 4-byte IP the device proposes. We Configure-Nak
          // with the IP we want them to use, unless they already
          // proposed the right one.
          const proposed = odata.length === 4
            ? `${odata[0]}.${odata[1]}.${odata[2]}.${odata[3]}`
            : '';
          const want = DEVICE_IP.join('.');
          if (proposed === want) {
            for (let j = 0; j < olen; j++) ackOpts.push(body[i + j]);
          } else {
            nakOpts.push(IPCP_OPT_IP, 6, DEVICE_IP[0], DEVICE_IP[1], DEVICE_IP[2], DEVICE_IP[3]);
          }
          break;
        }
        case IPCP_OPT_DNS_PRI: {
          const proposed = odata.length === 4
            ? `${odata[0]}.${odata[1]}.${odata[2]}.${odata[3]}`
            : '';
          const want = HOST_IP.join('.');
          if (proposed === want) {
            for (let j = 0; j < olen; j++) ackOpts.push(body[i + j]);
          } else {
            nakOpts.push(IPCP_OPT_DNS_PRI, 6, HOST_IP[0], HOST_IP[1], HOST_IP[2], HOST_IP[3]);
          }
          break;
        }
        case IPCP_OPT_DNS_SEC: {
          // We don't have a secondary DNS; nak it with the same as primary.
          nakOpts.push(IPCP_OPT_DNS_SEC, 6, HOST_IP[0], HOST_IP[1], HOST_IP[2], HOST_IP[3]);
          break;
        }
        case IPCP_OPT_VJ:
        default: {
          for (let j = 0; j < olen; j++) rejOpts.push(body[i + j]);
          break;
        }
      }
      i += olen;
    }

    const events: IpcpEvent[] = [];
    if (rejOpts.length > 0) {
      events.push({ send: this.encode(IPCP_CONF_REJ, id, Uint8Array.from(rejOpts)) });
    } else if (nakOpts.length > 0) {
      events.push({ send: this.encode(IPCP_CONF_NAK, id, Uint8Array.from(nakOpts)) });
    } else {
      events.push({ send: this.encode(IPCP_CONF_ACK, id, Uint8Array.from(ackOpts)) });
      if (this.state === 'ack-sent') {
        this.state = 'opened';
        events.push({ stateChanged: 'opened', deviceIp: this.deviceIp });
      } else if (this.state === 'req-sent') {
        this.state = 'ack-sent';
        events.push({ stateChanged: 'ack-sent' });
      }
    }
    return events;
  }

  private handleConfAck(): IpcpEvent[] {
    if (this.state === 'ack-sent') {
      this.state = 'opened';
      return [{ stateChanged: 'opened', deviceIp: this.deviceIp }];
    }
    if (this.state === 'req-sent') {
      this.state = 'ack-sent';
      return [{ stateChanged: 'ack-sent' }];
    }
    return [];
  }

  private handleConfNak(): IpcpEvent[] {
    // Peer wants different options for our request — we only ask for
    // HOST_IP, so re-send as-is (peer typically nak's with the IP they
    // want us to use; we ignore and keep ours).
    return [{ send: this.open() }];
  }

  private handleConfRej(): IpcpEvent[] {
    // Peer rejected our IP option — re-send with nothing, so the
    // negotiation completes on their terms.
    return [{ send: this.encode(IPCP_CONF_REQ, this.nextId(), new Uint8Array(0)) }];
  }

  private encode(code: number, id: number, data: Uint8Array): Uint8Array {
    const len = 4 + data.length;
    const out = new Uint8Array(len);
    out[0] = code;
    out[1] = id;
    out[2] = (len >>> 8) & 0xFF;
    out[3] = len & 0xFF;
    out.set(data, 4);
    return out;
  }

  private nextId(): number {
    const v = this.id;
    this.id = (this.id + 1) & 0xFF;
    return v;
  }
}
