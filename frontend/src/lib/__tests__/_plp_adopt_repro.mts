// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Deterministic repro for the "Remote Link fails at some point every time"
// report when repeatedly using App Library → "Try it" on one device. Each
// delivery after the first does NOT re-attach the cable; it re-connects by
// ADOPTING the session the previous delivery parked (releaseQuiesced). This
// drives that exact cycle against the REAL 5mx ROM via the native harness:
//   for each cycle: new PlpClient → connect (adopt) → small RFSV op → park
// and reports which cycle (if any) fails.
//
// Usage:
//   node --experimental-strip-types frontend/src/lib/__tests__/_plp_adopt_repro.mts [cycles]
//   PDU=1 …  dump PDUs

import * as net from 'node:net';
import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';
import type { Pdu } from '../plp/link.ts';

const REPO = new URL('../../../../', import.meta.url).pathname;
const HARNESS = `${REPO}harness/run`;
const ROM = `${REPO}roms/5mx_v1.05(260)_eng.bin`;
const CYCLES = Number(process.argv[2] ?? 10);
const PDU = !!process.env.PDU;
const SOCK = `/tmp/psion-plp-adopt.sock`;

const pduStr = (p: Pdu) => `cont=${p.cont} seq=${p.seq} len=${p.data.length}`;
const sleep = (ms: number) => new Promise<void>(r => setTimeout(r, ms));

try { fs.unlinkSync(SOCK); } catch { /* not there */ }
let rxBuf: number[] = [];
let sock: net.Socket | null = null;

const read = () => { if (rxBuf.length === 0) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; };
const write = (d: Uint8Array) => { sock?.write(Buffer.from(d)); return d.length; };

async function cycle(i: number): Promise<{ ok: boolean; disc: boolean; detail: string }> {
  let disc = false; let lastState = '';
  const client = new PlpClient(read, write, {
    pollHz: 50, protocol: 'rfsv32', conSeq: 4,
    onState: s => { lastState = s; if (PDU) console.error(`  [c${i}] STATE`, s); },
    onPduIn: p => { if (p.cont === 1) disc = true; if (PDU) console.error(`  [c${i}] IN `, pduStr(p)); },
    onPduOut: p => { if (PDU) console.error(`  [c${i}] OUT`, pduStr(p)); },
  });
  client.start();
  try {
    await client.connect(30_000);
    const drives = await client.listDrives();
    // Park the session for the next cycle to adopt (what appLibrary does).
    await client.releaseQuiesced();
    return { ok: drives.length > 0, disc, detail: `drives=${drives.join('')}` };
  } catch (e) {
    try { client.disconnect(); } catch { /* ignore */ }
    return { ok: false, disc, detail: `${(e as Error).message} (lastState=${lastState})` };
  }
}

const server = net.createServer(s => {
  sock = s;
  s.on('data', d => { for (const b of d) rxBuf.push(b); });
  s.on('error', () => { /* ignore */ });
  void run();
});

let child: ReturnType<typeof spawn>;
server.listen(SOCK, () => {
  child = spawn(HARNESS, [ROM, '--device', '5mx', '--quiet-logs',
    '--serial-bridge-socket', SOCK,
    '--serial-attach', '2', '8',          // attach after boot → clean first connect
    '--serial-poll-until', '600', '2'],
    { env: { ...process.env, PSION_REALTIME: '1' }, stdio: ['ignore', 'ignore', 'ignore'] });
});

async function run(): Promise<void> {
  console.log(`adopt repro: 5mx, ${CYCLES} connect→park→adopt cycles`);
  let ok = 0;
  for (let i = 0; i < CYCLES; i++) {
    const r = await cycle(i);
    if (r.ok) ok++;
    console.log(`  cycle ${i + 1}: ${r.ok ? 'OK  ' : 'FAIL'} ${r.disc ? '[Disc]' : '      '} ${r.detail}`);
    await sleep(300);   // brief gap, mimics the user clicking the next "Try it"
    if (!r.ok && i > 0) { console.log('  (stopping after first failure)'); break; }
  }
  console.log(`\nRESULT: ${ok}/${CYCLES} cycles connected`);
  try { child.kill('SIGKILL'); } catch { /* ignore */ }
  process.exit(0);
}
