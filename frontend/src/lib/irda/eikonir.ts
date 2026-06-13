// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// EPOC Eikon IR transfer — the protocol the Psion 5mx "Infrared receive"
// dialog actually speaks. Reverse-engineered from the reference Symbian
// source (reference/Epoc SDK/.../Eikon/src/EIKIRDA.CPP), which is the very
// app that runs behind that dialog.
//
// This is NOT IrOBEX. The receiver (CEikIrFileReceiver) registers IAS class
// "Epoc32:EikonIr:v2.0" (attribute "IrDA:TinyTP:LsapSel" → 8), accepts a
// TinyTP connection on LSAP 8, then:
//
//   1. reads one SDU and expects   "FILE <size> <att> <timeHi> <timeLo> <name>"
//      (CEikIrFileReceiver::NegotiateL → ParseFirstPacketL)
//   2. replies with                 "ACK Y"  (accept) or "ACK N" (reject)
//      (it rejects with "ACK N" if the packet isn't FILE, or no disk space)
//   3. reads <size> raw bytes as successive SDUs, each one Read() chunk, and
//      writes them straight to the file (CEikIrFileReceiver::DoReceiveL /
//      WriteChunkL); when iNumRecvd == iNumToRecv the transfer is complete.
//
// As the sender (CEikIrFileSender) we therefore: build the FILE command,
// send it as one SDU, await "ACK Y", then stream the file body in
// max-data-size chunks, then shut the TinyTP/MUX connection down (which the
// receiver sees as transfer-complete + closes/renames the file into the inbox).

import { TinyTp } from './tinytp.ts';
import { EIKONIR_FILE_ATTR_ARCHIVE } from './types.ts';

const ascii = (s: string): Uint8Array => {
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xff;
  return out;
};
const fromAscii = (b: Uint8Array): string => {
  let s = '';
  for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]);
  return s;
};

// Build the EPOC "FILE" negotiation line. `attr` defaults to the archive bit
// (matches what the desktop Eikon sender emits for a normal file); time is
// optional (0/0 is accepted by the receiver and just yields no timestamp).
export function encodeFileCommand(
  name: string,
  size: number,
  attr = EIKONIR_FILE_ATTR_ARCHIVE,
  timeHi = 0,
  timeLo = 0,
): Uint8Array {
  // Receiver parses size/att/timeHi/timeLo as unsigned decimals and takes the
  // remainder (after the 4th space) verbatim as the name — so the name may
  // contain spaces but must come last. We strip any path the caller passed.
  const bareName = name.replace(/^.*[\\/]/, '');
  return ascii(`FILE ${size >>> 0} ${attr >>> 0} ${timeHi >>> 0} ${timeLo >>> 0} ${bareName}`);
}

export function isAckYes(sdu: Uint8Array): boolean {
  // ParseAck(): split on first space; left must be "ACK"; "Y" (right 1 char)
  // ⇒ proceed. Compare case-insensitively as EPOC's CompareF does.
  const s = fromAscii(sdu).trim();
  const sp = s.indexOf(' ');
  if (sp < 0) return false;
  if (s.slice(0, sp).toUpperCase() !== 'ACK') return false;
  return s.slice(sp + 1).trim().toUpperCase().startsWith('Y');
}

// Drives the Eikon IR sender protocol over an already-connected TinyTp.
export class EikonIrClient {
  private readonly ttp: TinyTp;
  // Max payload per data chunk. The receiver reads one SDU per chunk into a
  // buffer sized to the IrLAP remote max-data-size (our QoS negotiates 64).
  // TinyTp segments anything larger transparently, but to keep one-SDU =
  // one-Read alignment we send body chunks that fit a single TinyTP segment.
  private readonly chunkSize: number;

  constructor(ttp: TinyTp, chunkSize = 60) {
    this.ttp = ttp;
    this.chunkSize = chunkSize;
  }

  // Send `data` as a file named `name`. Resolves once the whole body has been
  // handed to TinyTP and acknowledged at the link layer; the caller then
  // disconnects, which the receiver interprets as transfer-complete.
  async sendFile(
    name: string,
    data: Uint8Array,
    onAck: (sdu: Uint8Array) => Promise<Uint8Array>,
    onProgress?: (n: number) => void,
    completionTimeoutMs = 4000,
  ): Promise<void> {
    // 1. FILE negotiation line → expect "ACK Y".
    const ack = await onAck(encodeFileCommand(name, data.length));
    if (!isAckYes(ack)) {
      throw new Error(`Eikon IR receiver declined the file (got "${fromAscii(ack).trim()}")`);
    }
    onProgress?.(0);

    // 2. Stream the raw body. Each chunk is one TinyTP SDU = one receiver Read.
    let offset = 0;
    while (offset < data.length) {
      const end = Math.min(offset + this.chunkSize, data.length);
      await this.ttp.send(data.subarray(offset, end));
      offset = end;
      onProgress?.(offset);
    }

    // 3. Completion handshake. Once the receiver has read the whole file
    //    (iNumRecvd == iNumToRecv) it saves it. We wait here, rather than
    //    tearing the link down immediately, because disconnecting *before*
    //    the device has read the last bytes races its pending Read and makes
    //    it report "transfer interrupted" (KErrDisconnected) instead of
    //    saving — the desktop Eikon sender waits too (RequestDisconnect-
    //    Indication). Two device behaviours converge here:
    //      * 5mx / Series 5 / netBook close the socket themselves once saved,
    //        which arrives as an LM-Disconnect and resolves this wait in well
    //        under a second.
    //      * The Series 7 (EPOC R5 v254) leaves its "Infrared transfer
    //        complete" dialog up and waits for the *sender* to close. It never
    //        sends an LM-Disconnect, so this wait reaches its (short) timeout;
    //        the caller's teardown (ttp.disconnect) then dismisses the dialog.
    //    The body is already fully delivered before we get here, so timing out
    //    and disconnecting is safe — the file is saved either way. The timeout
    //    is kept short so the Series 7 path isn't perceived as a hang.
    await this.ttp.waitForPeerDisconnect(completionTimeoutMs);
  }
}
