// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Live IrDA bridge: listens on a unix socket, relays bytes to/from the
// native harness (which connects via --serial-bridge-socket), and runs the
// real IrSendClient against the device. Used for manual native validation
// of the full beam against a harnessed 5mx in Infrared receive mode.
//
// Usage (the harness must connect to SOCK after we start listening):
//   node --experimental-strip-types _ir_bridge.mts <SOCK>
// Exits 0 if discovery+connect (and optionally sendFile) succeed.

import net from 'node:net';
import { IrSendClient, IrNotListeningError } from '../irda/index.ts';
import { IrLap } from '../irda/irlap.ts';
import { IrTransport } from '../irda/transport.ts';

const sockPath = process.argv[2];
if (!sockPath) {
  console.error('usage: _ir_bridge.mts <socketPath>');
  process.exit(2);
}

let conn: net.Socket | null = null;
let rxBuf: number[] = [];

const readBytes = (): Uint8Array => {
  if (rxBuf.length === 0) return new Uint8Array(0);
  const out = new Uint8Array(rxBuf);
  rxBuf = [];
  return out;
};
const writeBytes = (data: Uint8Array): number => {
  if (conn) conn.write(Buffer.from(data));
  return data.length;
};

const server = net.createServer((socket) => {
  conn = socket;
  console.error('bridge: harness connected');
  socket.on('data', (b: Buffer) => {
    for (const x of b) rxBuf.push(x);
  });
  socket.on('close', () => {
    console.error('bridge: harness disconnected');
  });
  runClient().catch((e) => {
    console.error('CLIENT ERROR:', e?.message ?? e);
    process.exitCode = 1;
    cleanup();
  });
});

function cleanup() {
  try { server.close(); } catch { /* */ }
  try { conn?.destroy(); } catch { /* */ }
  setTimeout(() => process.exit(process.exitCode ?? 0), 50);
}

async function runClient() {
  // Drive IrLAP directly so we can report DISCOVERY success independently
  // of the (separate) SNRM connect step.
  const transport = new IrTransport(readBytes, writeBytes, { pollHz: 1000 });
  const irlap = new IrLap((body) => transport.sendFrame(body));
  transport.start(
    (body) => irlap.handleFrame(body),
    () => irlap.tick(),
  );

  // The harness boots the 5mx and navigates to the Infrared receive dialog
  // over several wall-seconds; the device only answers discovery once that
  // dialog is up. Retry discovery on a wall-clock loop until it answers.
  // Slower devices (Osaris/Series 5 boot + menu-walk runs longer than the
  // 5mx) need a bigger window — override with IR_DISCOVER_MS.
  const discoverMs = Number(process.env.IR_DISCOVER_MS ?? 45000);
  const deadline = Date.now() + discoverMs;
  let device = null as null | { deviceAddr: number; nickname: string };
  try {
    while (Date.now() < deadline && !device) {
      const found = await irlap.discover(1000);
      if (found.length > 0) device = found[0];
      else await sleep(300);
    }
    if (!device) {
      console.error('RESULT: discovery FAILED — device never answered XID');
      process.exitCode = 1;
      return;
    }
    console.error(
      `RESULT: discovery SUCCEEDED — device answered XID ` +
        `(addr=0x${(device.deviceAddr >>> 0).toString(16)}, nickname="${device.nickname}")`,
    );
    // Now attempt the IrLAP connection (SNRM -> UA). On the slower CL-PS711x
    // sims the device needs more wall-time to turn SNRM around — IR_CONNECT_MS
    // overrides the default 3 s window.
    const connectMs = Number(process.env.IR_CONNECT_MS ?? 3000);
    try {
      await irlap.connect(device.deviceAddr, connectMs);
      console.error('RESULT: connect SUCCEEDED — device answered UA (link up)');
      process.exitCode = 0;
    } catch (e) {
      console.error('RESULT: connect FAILED —', (e as Error)?.message ?? e);
      process.exitCode = 1;
      return;
    }

    // Optionally exercise the full beam (IAS -> Tiny TP -> OBEX). We tear
    // down the bare IrLAP probe link first and let IrSendClient run its own
    // discovery+connect+transfer over the same transport callbacks.
    if (process.env.IR_SEND_FILE === '1') {
      irlap.disconnect();
      transport.stop();
      await sleep(200);
      // discoveryTimeoutMs also bounds IrSendClient's IrLAP connect; bump it
      // (and the request/retry windows) for the slower CL-PS711x sims.
      const sendDiscMs = Number(process.env.IR_SEND_DISCOVER_MS ?? 1000);
      const sendReqMs = Number(process.env.IR_SEND_REQUEST_MS ?? 8000);
      const sendRetryMs = Number(process.env.IR_SEND_RETRY_MS ?? 15000);
      const client = new IrSendClient(readBytes, writeBytes, {
        pollHz: Number(process.env.IR_POLL_HZ ?? 1000),
        discoveryTimeoutMs: sendDiscMs,
        requestTimeoutMs: sendReqMs,
      });
      client.start();
      try {
        // Retry discovery until the device answers again post-teardown.
        const dl = Date.now() + sendRetryMs;
        let ok = false;
        while (Date.now() < dl && !ok) {
          try { await client.discoverAndConnect(); ok = true; }
          catch (e) { if (e instanceof IrNotListeningError) { await sleep(300); } else { throw e; } }
        }
        if (!ok) throw new IrNotListeningError();
        // IR_SEND_BYTES lets us beam a large synthetic file (the real-world
        // case) instead of the tiny default, to exercise the data-phase /
        // flow-control path on slow CL-PS711x sims.
        const nBytes = Number(process.env.IR_SEND_BYTES ?? 0);
        let payload: Uint8Array;
        let fname = 'PsionWeb.txt';
        if (nBytes > 0) {
          payload = new Uint8Array(nBytes);
          for (let i = 0; i < nBytes; i++) payload[i] = 0x41 + (i % 26);
          fname = `PsionWeb${nBytes}.txt`;
        } else {
          payload = new TextEncoder().encode('Hello from PsionWeb infrared!\n');
        }
        const t0 = Date.now();
        await client.sendFile(fname, payload, (n) =>
          console.error(`  progress ${n}/${payload.length} @ ${Date.now() - t0}ms`));
        console.error(
          `RESULT: sendFile SUCCEEDED — ${payload.length} bytes in ${Date.now() - t0}ms`,
        );
      } catch (e) {
        console.error('RESULT: sendFile FAILED —', (e as Error)?.message ?? e);
        process.exitCode = 1;
      } finally {
        client.stop();
      }
    }
  } catch (e) {
    console.error('RESULT: FAILED at', (e as Error)?.message ?? e);
    process.exitCode = 1;
  } finally {
    transport.stop();
    cleanup();
  }
}

function sleep(ms: number) {
  return new Promise((r) => setTimeout(r, ms));
}

import fs from 'node:fs';
try { fs.unlinkSync(sockPath); } catch { /* */ }
server.listen(sockPath, () => {
  console.error(`bridge: listening on ${sockPath}`);
});
