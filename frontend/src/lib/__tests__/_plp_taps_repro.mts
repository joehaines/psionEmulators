// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Repro: large (2 MB) Remote Link upload while the user keeps tapping
// icons on the device. Field report: "interacting with the device during
// a transfer makes it stop."
//
// Runs the REAL PlpClient against the REAL 5mx (Windermere) ROM through
// the native harness, relaying UART2 over a unix socket, and injects a
// periodic stream of screen taps (--tap-seq) across the whole transfer
// window so WServ + the digitiser compete with the RemoteLinkServer
// serial thread — exactly what happens in the browser when you poke the
// device mid-upload.
//
// Usage:
//   node --experimental-strip-types frontend/src/lib/__tests__/_plp_taps_repro.mts
//   SIZE=2097152 TAP_EVERY=1.5 …   override payload size / tap cadence
//   NO_TAPS=1 …                    control run: same transfer, no taps
//   CHUNK=1024 …                   client chunk size
//
// Requires harness/build.sh built and the 5mx ROM present.

import * as net from 'node:net';
import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';

const REPO = new URL('../../../../', import.meta.url).pathname;
const SOCK = `/tmp/psion-plp-taps.sock`;
const HARNESS = `${REPO}harness/run`;
const ROM = `${REPO}roms/5mx_v1.05(260)_eng.bin`;

const SIZE = Number(process.env.SIZE ?? 2 * 1024 * 1024);
const TAP_EVERY = Number(process.env.TAP_EVERY ?? 1.5);
const CHUNK = Number(process.env.CHUNK ?? 1024);
const ATTACH_SEC = 8;
const POLL_UNTIL = Number(process.env.POLL_UNTIL ?? 900);
const NO_TAPS = !!process.env.NO_TAPS;

const t0 = Date.now();
const log = (...a: unknown[]) => console.error(`[taps ${((Date.now() - t0) / 1000).toFixed(2)}s]`, ...a);

// Tap the System-screen icon row repeatedly from just after attach to the
// end of the poll window. (640×240 panel; y≈220 is the bottom icon bar.)
const TAP_UNTIL = Number(process.env.TAP_UNTIL ?? POLL_UNTIL);  // stop tapping after this sim-second
const taps: string[] = [];
if (!NO_TAPS) {
  for (let s = ATTACH_SEC + 2; s < TAP_UNTIL; s += TAP_EVERY) {
    const x = 40 + ((taps.length * 60) % 560);  // walk across the icon bar
    taps.push('--tap-seq', String(s), String(x), '220');
  }
}

try { fs.unlinkSync(SOCK); } catch { /* not there */ }

let rxBuf: number[] = [];
let sock: net.Socket | null = null;

const server = net.createServer(s => {
  sock = s;
  log('harness connected to relay socket');
  s.on('data', d => { for (const b of d) rxBuf.push(b); });
  s.on('close', () => log('socket closed'));
  s.on('error', e => log('socket error', (e as Error).message));
  void runClient();
});

server.listen(SOCK, () => {
  log(`listening on ${SOCK}; spawning 5mx harness (taps=${!NO_TAPS}, ${taps.length / 4} taps)`);
  const args = [ROM, '--device', '5mx', '--quiet-logs',
    '--serial-bridge-socket', SOCK,
    '--serial-attach', '2', String(ATTACH_SEC),
    '--serial-poll-until', String(POLL_UNTIL), '2',
    ...taps];
  const child = spawn(HARNESS, args,
    { env: { ...process.env, PSION_REALTIME: '1' }, stdio: ['ignore', 'ignore', 'inherit'] });
  child.on('exit', code => { log(`harness exited (${code})`); process.exit(code ?? 0); });
});

async function runClient(): Promise<void> {
  const client = new PlpClient(
    () => { if (rxBuf.length === 0) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; },
    d => { sock?.write(Buffer.from(d)); return d.length; },
    {
      pollHz: Number(process.env.POLLHZ ?? 50), protocol: 'rfsv32', conSeq: 4,
      requestTimeoutMs: process.env.REQ_TIMEOUT ? Number(process.env.REQ_TIMEOUT) : undefined,
      // A mid-transfer transition back to 'link-up' means the device
      // re-handshaked the data-link (onReset) — i.e. a link bounce, the
      // restart-from-0 trigger. 'rfsv-opening' = withRfsv re-opening.
      onState: s => log('STATE →', s),
    },
  );
  client.setChunkSize(CHUNK);
  client.start();
  try {
    log('connecting…');
    await client.connect(90_000);
    log('CONNECTED — listing drives…');
    const drives = await client.listDrives();
    log('DRIVES →', drives.join(' '));

    const payload = new Uint8Array(SIZE);
    for (let i = 0; i < payload.length; i++) payload[i] = (i * 31 + 7) & 0xFF;

    log(`UPLOADING ${SIZE} bytes to C:\\BIG.BIN (chunk=${CHUNK})…`);
    const upStart = Date.now();
    let lastPct = -1;
    await client.uploadFile('C:\\BIG.BIN', payload, n => {
      const pct = Math.floor((n / SIZE) * 100);
      if (pct !== lastPct && pct % 5 === 0) { lastPct = pct; log(`  upload ${pct}% (${n}/${SIZE})`); }
    });
    const upSec = (Date.now() - upStart) / 1000;
    log(`UPLOAD ok in ${upSec.toFixed(1)}s (${(SIZE / 1024 / upSec).toFixed(1)} KiB/s)`);

    log('DOWNLOADING back to verify…');
    const back = await client.downloadFile('C:\\BIG.BIN', n => {
      const pct = Math.floor((n / SIZE) * 100);
      if (pct !== lastPct && pct % 10 === 0) { lastPct = pct; log(`  download ${pct}%`); }
    });
    let mismatch = -1;
    if (back.length !== payload.length) mismatch = -2;
    else for (let i = 0; i < back.length; i++) { if (back[i] !== payload[i]) { mismatch = i; break; } }
    if (mismatch === -1) {
      log(`VERIFY ok — ${back.length} bytes round-tripped intact`);
      console.error('\n*** SUCCESS ***');
    } else {
      log(`VERIFY FAILED — len=${back.length} want ${payload.length}, first mismatch ${mismatch}`);
      console.error('\n*** CORRUPT/TRUNCATED ***');
    }
    try { await client.deleteFile('C:\\BIG.BIN'); } catch { /* ignore */ }
  } catch (e) {
    log('FAILED:', (e as Error).message);
    console.error('\n*** STALLED ***');
  } finally {
    try { client.disconnect(); } catch { /* ignore */ }
    setTimeout(() => process.exit(0), 200);
  }
}
