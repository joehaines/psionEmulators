// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrLMP / LM-MUX — multiplexes several logical LSAP connections over the
// single IrLAP link. Each LM-PDU rides in an IrLAP information field:
//
//   dstLSAP | srcLSAP | [control opcode | reserved] | data…
//
// dstLSAP bit7 set  → control PDU (Connect / Disconnect); the opcode byte
//                     follows. Cleared → data PDU; the rest is user data.
// srcLSAP bit7 set  → command (cleared = response).
//
// We open connections as the initiator (Connect command → Connect-confirm
// reply) and then pump data both ways. This is enough for the IAS query
// and the Tiny TP/OBEX channel the beam needs.

import {
  LMP_CONTROL_BIT,
  LMP_CR_BIT,
  LMP_CONNECT,
  LMP_CONNECT_CNF,
  LMP_DISCONNECT,
} from './types.ts';

const CH_IDLE = 0;
const CH_CONNECTING = 1;
const CH_OPEN = 2;

interface Channel {
  localLsap: number;
  remoteLsap: number;
  state: number;
  connectResolve: ((confirmData: Uint8Array) => void) | null;
  connectReject: ((e: Error) => void) | null;
  connectTimer: ReturnType<typeof setTimeout> | null;
  onData: ((data: Uint8Array) => void) | null;
  onDisconnect: (() => void) | null;
}

export class IrLmp {
  private readonly sendInfo: (pdu: Uint8Array) => void;
  private readonly channels = new Map<number, Channel>();

  constructor(sendInfo: (pdu: Uint8Array) => void) {
    this.sendInfo = sendInfo;
  }

  // Open an LM-MUX connection from our `localLsap` to the device's
  // `remoteLsap`. Optional `connectData` is appended after the reserved
  // byte (Tiny TP layers its initial-credit octet there). Resolves with
  // the Connect-confirm's user-data (empty if none).
  connect(
    localLsap: number,
    remoteLsap: number,
    timeoutMs: number,
    connectData: Uint8Array = new Uint8Array(0),
  ): Promise<Uint8Array> {
    const ch: Channel = {
      localLsap,
      remoteLsap,
      state: CH_CONNECTING,
      connectResolve: null,
      connectReject: null,
      connectTimer: null,
      onData: null,
      onDisconnect: null,
    };
    this.channels.set(localLsap, ch);
    return new Promise<Uint8Array>((resolve, reject) => {
      ch.connectResolve = resolve;
      ch.connectReject = reject;
      // Connect command PDU: control + command bits set, opcode, reserved,
      // then any upper-layer connect data.
      const pdu = new Uint8Array(4 + connectData.length);
      pdu[0] = remoteLsap | LMP_CONTROL_BIT;
      pdu[1] = localLsap | LMP_CR_BIT;
      pdu[2] = LMP_CONNECT;
      pdu[3] = 0x00;
      pdu.set(connectData, 4);
      this.sendInfo(pdu);
      ch.connectTimer = setTimeout(() => {
        if (ch.state === CH_CONNECTING) {
          ch.state = CH_IDLE;
          const rej = ch.connectReject;
          ch.connectResolve = null;
          ch.connectReject = null;
          rej?.(new Error(`LM-MUX connect to LSAP ${remoteLsap} timed out`));
        }
      }, timeoutMs);
    });
  }

  setDataHandler(localLsap: number, handler: (data: Uint8Array) => void): void {
    const ch = this.channels.get(localLsap);
    if (ch) ch.onData = handler;
  }

  // Notified when the peer sends an LM-Disconnect for this channel (the EPOC
  // file receiver does this once it has saved the file — our graceful-close
  // signal).
  setDisconnectHandler(localLsap: number, handler: () => void): void {
    const ch = this.channels.get(localLsap);
    if (ch) ch.onDisconnect = handler;
  }

  // Send a data PDU on an open channel.
  sendData(localLsap: number, data: Uint8Array): void {
    const ch = this.channels.get(localLsap);
    if (!ch || ch.state !== CH_OPEN) throw new Error(`LSAP ${localLsap} not open`);
    const pdu = new Uint8Array(2 + data.length);
    pdu[0] = ch.remoteLsap; // data PDU: control bit clear
    pdu[1] = ch.localLsap;
    pdu.set(data, 2);
    this.sendInfo(pdu);
  }

  disconnect(localLsap: number): void {
    const ch = this.channels.get(localLsap);
    if (!ch) return;
    if (ch.state === CH_OPEN || ch.state === CH_CONNECTING) {
      const pdu = new Uint8Array([
        ch.remoteLsap | LMP_CONTROL_BIT,
        ch.localLsap | LMP_CR_BIT,
        LMP_DISCONNECT,
        0x00, // disconnect reason: user request
      ]);
      this.sendInfo(pdu);
    }
    this.channels.delete(localLsap);
  }

  // Dispatch an inbound LM-PDU (the info field of an IrLAP I-frame).
  handlePdu(pdu: Uint8Array): void {
    if (pdu.length < 2) return;
    const dst = pdu[0];
    const localLsap = dst & 0x7f; // our LSAP is the destination of inbound PDUs
    const ch = this.channels.get(localLsap);
    if (!ch) return;

    if ((dst & LMP_CONTROL_BIT) !== 0) {
      // Control PDU: opcode at [2].
      if (pdu.length < 3) return;
      const opcode = pdu[2];
      if (opcode === LMP_CONNECT_CNF || opcode === LMP_CONNECT) {
        if (ch.state === CH_CONNECTING) {
          if (ch.connectTimer) clearTimeout(ch.connectTimer);
          ch.connectTimer = null;
          ch.state = CH_OPEN;
          const res = ch.connectResolve;
          ch.connectResolve = null;
          ch.connectReject = null;
          // Confirm user-data (Tiny TP credit octet) follows the reserved
          // byte at [3].
          res?.(pdu.subarray(4));
        }
      } else if (opcode === LMP_DISCONNECT) {
        ch.state = CH_IDLE;
        const cb = ch.onDisconnect;
        this.channels.delete(localLsap);
        cb?.();
      }
      return;
    }

    // Data PDU.
    ch.onData?.(pdu.subarray(2));
  }
}
