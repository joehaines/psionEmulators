// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Diagnostic bridge: like _ir_bridge.mts but taps every LM-PDU (IrLMP info
// field) in both directions and the upper-layer SDUs, so we can PROVE — from
// the real device's bytes — what the IAS query returns, what the TinyTP
// connect-confirm carries, and that the receiver answers "ACK Y". Drives the
// full Eikon-IR beam and dumps a labelled trace to stderr.

import net from 'node:net';
import { IrSendClient, IrNotListeningError } from '../irda/index.ts';

const sockPath = process.argv[2];
if (!sockPath) { console.error('usage: _ir_capture.mts <socketPath>'); process.exit(2); }

let conn: net.Socket | null = null;
let rxBuf: number[] = [];
const readBytes = (): Uint8Array => {
  if (rxBuf.length === 0) return new Uint8Array(0);
  const out = new Uint8Array(rxBuf); rxBuf = []; return out;
};
const writeBytes = (data: Uint8Array): number => {
  if (conn) conn.write(Buffer.from(data)); return data.length;
};
const hex = (u: Uint8Array) => Array.from(u).map((b) => b.toString(16).padStart(2, '0')).join(' ');
const printable = (u: Uint8Array) =>
  Array.from(u).map((b) => (b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : '.')).join('');

const server = net.createServer((socket) => {
  conn = socket;
  console.error('capture: harness connected');
  socket.on('data', (b: Buffer) => { for (const x of b) rxBuf.push(x); });
  runClient().catch((e) => { console.error('CLIENT ERROR:', e?.message ?? e); process.exitCode = 1; cleanup(); });
});
function cleanup() {
  try { server.close(); } catch { /* */ }
  try { conn?.destroy(); } catch { /* */ }
  setTimeout(() => process.exit(process.exitCode ?? 0), 50);
}
function sleep(ms: number) { return new Promise((r) => setTimeout(r, ms)); }

async function runClient() {
  const client = new IrSendClient(readBytes, writeBytes, {
    pollHz: Number(process.env.IR_POLL_HZ ?? 50),
    discoveryTimeoutMs: Number(process.env.IR_SEND_DISCOVER_MS ?? 1000),
    requestTimeoutMs: Number(process.env.IR_SEND_REQUEST_MS ?? 8000),
  });

  // ── Tap the LM-PDU layer (the IrLAP info field carries one LM-PDU). ──
  const irlmp = (client as any).irlmp as { handlePdu: (p: Uint8Array) => void; };
  const irlap = (client as any).irlap as { sendInfo: (p: Uint8Array, e?: boolean) => void; };
  const origHandle = irlmp.handlePdu.bind(irlmp);
  irlmp.handlePdu = (pdu: Uint8Array) => { console.error(`LM-PDU  RX  ${hex(pdu)}    |${printable(pdu)}|`); origHandle(pdu); };
  const origSend = irlap.sendInfo.bind(irlap);
  (client as any).irlmp = irlmp;
  (irlap as any).sendInfo = (pdu: Uint8Array, e?: boolean) => { console.error(`LM-PDU  TX  ${hex(pdu)}    |${printable(pdu)}|`); origSend(pdu, e); };
  // Re-point the IrLmp's sendInfo closure: IrLmp was constructed with a bound
  // (body)=>irlap.sendInfo. Patch by replacing the irlap method (closure calls
  // this.irlap.sendInfo via the captured `irlap` ref) — done above.

  client.start();
  try {
    const dl = Date.now() + Number(process.env.IR_DISCOVER_MS ?? 60000);
    let ok = false;
    while (Date.now() < dl && !ok) {
      try { await client.discoverAndConnect(); ok = true; }
      catch (e) { if (e instanceof IrNotListeningError) { await sleep(300); } else { throw e; } }
    }
    if (!ok) throw new IrNotListeningError();
    console.error('--- IrLAP link up; beaming file ---');
    // Payload size configurable via IR_FILE_BYTES (default: small smoke test).
    // Large files exercise TinyTP flow control / IrLAP windowing / RX FIFO.
    const nBytes = Number(process.env.IR_FILE_BYTES ?? 0);
    let payload: Uint8Array;
    if (nBytes > 0) {
      payload = new Uint8Array(nBytes);
      if (process.env.IR_FILE_BINARY) {
        // Binary content: cycles all 256 byte values, so it exercises the SIR
        // byte-stuffing/escape path (0xC0/0xC1/0x7D) that ASCII text never hits
        // — like a real file.
        for (let i = 0; i < nBytes; i++) payload[i] = i & 0xff;
      } else {
        // Deterministic, verifiable ASCII content (line-numbered).
        const enc = new TextEncoder();
        let off = 0, line = 0;
        while (off < nBytes) {
          const s = enc.encode(`line ${line++} the quick brown fox jumps over the lazy dog\n`);
          const n = Math.min(s.length, nBytes - off);
          payload.set(s.subarray(0, n), off);
          off += n;
        }
      }
    } else {
      payload = new TextEncoder().encode('Hello from PsionWeb infrared!\n');
    }
    console.error(`--- beaming ${payload.length} bytes ---`);
    await client.sendFile('PsionWeb.txt', payload, (n) => {
      if (n % 2048 < 60) console.error(`progress: ${n}/${payload.length}`);
    });
    console.error('RESULT: sendFile SUCCEEDED');
  } catch (e) {
    console.error('RESULT: sendFile FAILED —', (e as Error)?.message ?? e);
    process.exitCode = 1;
  } finally {
    client.stop();
    cleanup();
  }
}

import fs from 'node:fs';
try { fs.unlinkSync(sockPath); } catch { /* */ }
server.listen(sockPath, () => console.error(`capture: listening on ${sockPath}`));
