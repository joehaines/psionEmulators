// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrSendClient — top-level API for beaming a file to an EPOC device over
// emulated infrared. Mirrors PlpClient in lib/plp/index.ts: construct it
// with a pair of byte-IO callbacks bound to the WASM UART1 bridge, then:
//
//   const client = new IrSendClient(read, write);
//   client.start();
//   await client.discoverAndConnect();   // throws IrNotListeningError if
//                                         // the device isn't in receive mode
//   await client.sendFile(name, bytes, onProgress);
//   client.stop();
//
// Layering: this owns IrLap + IrLmp and, per transfer, a TinyTp channel to
// the device's Eikon-IR file-receive server (whose LSAP-SEL is looked up via
// IAS, class "Epoc32:EikonIr:v2.0") and an EikonIrClient running the EPOC
// "FILE …" → "ACK Y" → body → graceful-disconnect handshake over it.
//
// NB: the 5mx "Infrared receive" dialog is EPOC's Eikon IR transfer app, not
// a generic IrOBEX inbox — see eikonir.ts and types.ts for the reverse-
// engineering notes and the captured device bytes that prove it.

import { IrTransport, type IrTransportConfig } from './transport.ts';
import { IrLap } from './irlap.ts';

// The reverse direction — the device beaming a file to us — lives in
// receive.ts and is re-exported here so callers import the whole IrDA API
// from 'lib/irda'.
export {
  IrReceiveClient,
  IrReceiveCanceledError,
  type IrReceiveConfig,
  type ReceivedFile,
  type ReceiveCallbacks,
} from './receive.ts';
import { IrLmp } from './irlmp.ts';
import { TinyTp } from './tinytp.ts';
import { EikonIrClient } from './eikonir.ts';
import { iasGetIntegerByClass } from './ias.ts';
import {
  IAS_EIKONIR_V2_CLASS,
  IAS_EIKONIR_V1_CLASS,
  IAS_TINYTP_LSAPSEL_ATTR,
  LSAP_CLIENT_OBEX,
} from './types.ts';

// Thrown when discovery finds no device — i.e. the EPOC machine is not in
// Infrared receive mode. The dialog keys on this to show its guidance.
export class IrNotListeningError extends Error {
  constructor() {
    super('No infrared device responding');
    this.name = 'IrNotListeningError';
  }
}

// Field-debug switch for IR transfers: `localStorage.irDebug = '1'` or
// `globalThis.__irDebug = true`. Wrapped in try/catch so a locked-down
// localStorage (private mode) can't throw during a beam.
function irDebugEnabled(): boolean {
  try {
    if ((globalThis as { __irDebug?: unknown }).__irDebug) return true;
    return typeof localStorage !== 'undefined' && localStorage.getItem('irDebug') === '1';
  } catch {
    return false;
  }
}

export interface IrSendConfig extends IrTransportConfig {
  // Discovery / connect window. ~4 s covers several IrLAP discovery slot
  // cycles before we conclude nobody is listening.
  discoveryTimeoutMs?: number;
  // Per request/response timeout for the upper layers.
  requestTimeoutMs?: number;
  // Grace period to wait for the receiver's graceful disconnect after the
  // whole body has been delivered. Devices whose Eikon receiver closes the
  // socket itself once it has the file send their LM-Disconnect within a
  // couple of seconds (the netBook measured ~2.1 s; the 5mx is sub-second),
  // which resolves the wait. The Series 7 (EPOC R5 v254) instead leaves its
  // "Infrared transfer complete" dialog up and waits for the *sender* to
  // close — it never sends an LM-Disconnect, so we fall through this grace
  // and disconnect ourselves, which dismisses its dialog. Previously this
  // reused requestTimeoutMs (10 s), so a Series 7 beam sat on "transfer
  // complete" for ten seconds and looked hung. Kept just above the slowest
  // self-closing device so they keep their clean teardown, but far below the
  // old 10 s. See eikonir.ts sendFile step 3.
  completionTimeoutMs?: number;
}

export class IrSendClient {
  private readonly transport: IrTransport;
  private readonly irlap: IrLap;
  private readonly irlmp: IrLmp;
  private readonly discoveryTimeoutMs: number;
  private readonly requestTimeoutMs: number;
  private readonly completionTimeoutMs: number;

  constructor(
    readBytes: () => Uint8Array,
    writeBytes: (data: Uint8Array) => number,
    config: IrSendConfig = {},
  ) {
    this.discoveryTimeoutMs = config.discoveryTimeoutMs ?? 4000;
    this.requestTimeoutMs = config.requestTimeoutMs ?? 10000;
    this.completionTimeoutMs = config.completionTimeoutMs ?? 4000;
    this.transport = new IrTransport(readBytes, writeBytes, config);
    this.irlap = new IrLap((body) => this.transport.sendFrame(body));
    this.irlmp = new IrLmp((pdu) => this.irlap.sendInfo(pdu));
    // IrLAP information fields are LM-PDUs.
    this.irlap.onInfo = (info) => this.irlmp.handlePdu(info);

    // Optional console tracing for field-debugging a stuck transfer. Enable in
    // the browser with `localStorage.irDebug = '1'` (or `globalThis.__irDebug
    // = true`) then reload and retry; paste the console output. Logs every
    // LM-PDU in both directions (incl. the TinyTP credit byte) so a stall is
    // visible. Off by default — zero overhead.
    if (irDebugEnabled()) {
      const hex = (u: Uint8Array) =>
        Array.from(u).map((b) => b.toString(16).padStart(2, '0')).join(' ');
      const txt = (u: Uint8Array) =>
        Array.from(u).map((b) => (b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : '.')).join('');
      const origHandle = this.irlmp.handlePdu.bind(this.irlmp);
      this.irlmp.handlePdu = (pdu: Uint8Array) => {
        // eslint-disable-next-line no-console
        console.debug(`[IR] RX ${hex(pdu)}  |${txt(pdu)}|`);
        origHandle(pdu);
      };
      const origSend = this.irlap.sendInfo.bind(this.irlap);
      this.irlap.sendInfo = (pdu: Uint8Array, expectReply?: boolean) => {
        // eslint-disable-next-line no-console
        console.debug(`[IR] TX ${hex(pdu)}  |${txt(pdu)}|`);
        origSend(pdu, expectReply);
      };
    }
  }

  start(): void {
    this.transport.start(
      (body) => this.irlap.handleFrame(body),
      () => this.irlap.tick(),
    );
  }

  stop(): void {
    try {
      this.irlap.disconnect();
    } catch {
      /* ignore */
    }
    this.transport.stop();
  }

  get badFcs() {
    return this.transport.badFcs;
  }
  get framingErrors() {
    return this.transport.framingErrors;
  }

  // Run IrLAP discovery and open the link. Throws IrNotListeningError when
  // no device answers (device not in Infrared receive mode).
  async discoverAndConnect(): Promise<void> {
    const devices = await this.irlap.discover(this.discoveryTimeoutMs);
    if (devices.length === 0) throw new IrNotListeningError();
    await this.irlap.connect(devices[0].deviceAddr, this.discoveryTimeoutMs);
  }

  // Beam `data` to the device as a file named `name`. Must be called after
  // a successful discoverAndConnect().
  //
  // The 5mx "Infrared receive" dialog is EPOC's Eikon IR transfer app, not a
  // generic IrOBEX inbox: we resolve its TinyTP LSAP via IAS (class
  // "Epoc32:EikonIr:v2.0", attr "IrDA:TinyTP:LsapSel" → 8), open a TinyTP
  // channel, send the "FILE …" line, await "ACK Y", then stream the body.
  async sendFile(name: string, data: Uint8Array, onProgress?: (n: number) => void): Promise<void> {
    if (!this.irlap.connected) throw new Error('IrLAP link not connected');

    // 1. Find the receiver's TinyTP LSAP-SEL via IAS. EPOC R5 registers v2.0;
    //    fall back to v1.0 the way the desktop sender does.
    let serverLsap: number;
    try {
      serverLsap = await iasGetIntegerByClass(
        this.irlmp,
        IAS_EIKONIR_V2_CLASS,
        IAS_TINYTP_LSAPSEL_ATTR,
        this.requestTimeoutMs,
      );
    } catch {
      serverLsap = await iasGetIntegerByClass(
        this.irlmp,
        IAS_EIKONIR_V1_CLASS,
        IAS_TINYTP_LSAPSEL_ATTR,
        this.requestTimeoutMs,
      );
    }

    // 2. Open a Tiny TP channel to it. Feed it the IrLAP proof-of-life clock so
    //    the send-credit wait only fails on genuine silence, not on a device
    //    that is merely slow to extend credit (e.g. the Series 5).
    const ttp = new TinyTp(this.irlmp, LSAP_CLIENT_OBEX, serverLsap);
    ttp.setActivityClock(() => this.irlap.lastInboundAt);
    await ttp.connect(this.requestTimeoutMs);

    // 3. The Eikon protocol has exactly one request/response exchange (the
    //    "FILE …" line → "ACK Y"); the body is then streamed one-way. A
    //    single pending resolver is sufficient.
    let pending: ((sdu: Uint8Array) => void) | null = null;
    ttp.onSdu = (sdu) => {
      const r = pending;
      pending = null;
      r?.(sdu);
    };
    const exchange = (packet: Uint8Array): Promise<Uint8Array> =>
      new Promise<Uint8Array>((resolve, reject) => {
        const timer = setTimeout(() => {
          pending = null;
          reject(new Error('Eikon IR response timed out'));
        }, this.requestTimeoutMs);
        pending = (sdu: Uint8Array) => {
          clearTimeout(timer);
          resolve(sdu);
        };
        ttp.send(packet, this.requestTimeoutMs).catch((e: unknown) => {
          clearTimeout(timer);
          pending = null;
          reject(e instanceof Error ? e : new Error(String(e)));
        });
      });

    const eikon = new EikonIrClient(ttp);
    try {
      // 4. FILE → ACK Y → stream body → wait for the device's graceful
      //    disconnect (it saves the file, then shuts the socket).
      await eikon.sendFile(name, data, exchange, onProgress, this.completionTimeoutMs);
    } finally {
      // If the device already disconnected this is a no-op; otherwise it tears
      // our side down cleanly.
      ttp.disconnect();
    }
  }
}
