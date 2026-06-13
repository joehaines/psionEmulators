// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Repro for the device-switch Remote Link wedge (issue: reconnect fails
// after switching away from and back to an EPOC device).
//
// Drives the REAL PlpClient against the REAL 5mx ROM over the harness
// socket bridge, and reproduces the *core mechanism* of the bug without
// a browser: a parked (established, host-silent) link session, then a
// reconnect. The variable under test is whether a redundant
// serialAttachHost() fires on the already-attached UART just before the
// reconnect — which is exactly what the worker-mode hook does after a
// restore (it resets its JS attach-tracking to false, so connect()
// re-attaches), versus the main-thread hook (which reads the WASM
// hostAttached flag and skips the re-attach).
//
// serialAttachHost() clears the UART rx/tx FIFOs and raises a
// modem-status ("cable just plugged in") IRQ. On a device whose link
// session is established that edge is the unplug/replug the ROM treats
// as session-fatal.
//
// Usage:
//   node --experimental-strip-types frontend/src/lib/__tests__/_plp_switch_repro.mts        # FIXED: no redundant attach
//   REDUNDANT_ATTACH=1 node … _plp_switch_repro.mts                                          # BUGGY: redundant attach mid-gap
//
// Exits 0 if the post-gap reconnect adopts and lists drives; 1 otherwise.

import * as net from 'node:net';
import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';
import type { Pdu } from '../plp/link.ts';

const REPO = new URL('../../../../', import.meta.url).pathname;
const SOCK = `/tmp/psion-plp-switch.sock`;
const HARNESS = `${REPO}harness/run`;
const ROM = `${REPO}roms/5mx_v1.05(260)_eng.bin`;
const UART = 2;

const REDUNDANT = !!process.env.REDUNDANT_ATTACH;

// Sim-time schedule (PSION_REALTIME → ~wall time after boot):
//   t=8   initial host attach (cable plug; ROM starts its Req_Req burst)
//   t=40  redundant attach (BUGGY only) — fires during the park gap
const REDUNDANT_AT = 40;

try { fs.unlinkSync(SOCK); } catch { /* not there */ }

const t0 = Date.now();
const log = (...a: unknown[]) => console.error(`[switch ${((Date.now() - t0) / 1000).toFixed(2)}s]`, ...a);

let rxBuf: number[] = [];
let sock: net.Socket | null = null;

function pduStr(dir: string, p: Pdu): string {
  const prev = Array.from(p.data.subarray(0, 12)).map(b => b.toString(16).padStart(2, '0')).join(' ');
  return `${dir} cont=${p.cont} seq=${p.seq} len=${p.data.length} ${prev}`;
}

function makeClient(tag: string): PlpClient {
  return new PlpClient(
    () => { if (rxBuf.length === 0) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; },
    d => { sock?.write(Buffer.from(d)); return d.length; },
    {
      pollHz: 50,
      protocol: 'rfsv32',
      conSeq: 4,
      onState: s => log(`[${tag}] STATE →`, s),
      onPduIn: p => log(`[${tag}]`, pduStr(' IN ', p)),
      onPduOut: p => log(`[${tag}]`, pduStr('OUT ', p)),
    },
  );
}

const server = net.createServer(s => {
  sock = s;
  log('harness connected');
  s.on('data', d => { for (const b of d) rxBuf.push(b); });
  s.on('close', () => log('socket closed'));
  s.on('error', e => log('socket error', (e as Error).message));
  void run();
});

server.listen(SOCK, () => {
  const args = [ROM, '--device', '5mx', '--quiet-logs',
                '--serial-bridge-socket', SOCK,
                '--serial-attach', String(UART), '8',
                ...(REDUNDANT ? ['--serial-attach', String(UART), String(REDUNDANT_AT)] : []),
                '--serial-poll-until', '90', String(UART)];
  log(`spawning harness (${REDUNDANT ? 'BUGGY: redundant attach @' + REDUNDANT_AT + 's' : 'FIXED: no redundant attach'})`);
  const child = spawn(HARNESS, args,
    { env: { ...process.env, PSION_REALTIME: '1' }, stdio: ['ignore', 'ignore', 'inherit'] });
  child.on('exit', code => log(`harness exited (${code})`));
});

async function run(): Promise<void> {
  // ── Phase 1: establish a session (device A, link connected) ──
  const c1 = makeClient('A');
  c1.start();
  try {
    log('connecting #1…');
    await c1.connect(60_000);
    const drives = await c1.listDrives();
    log('SESSION ESTABLISHED — drives:', drives.join(' '));
  } catch (e) {
    log('phase-1 connect FAILED:', (e as Error).message);
    console.error('\n*** REPRO INCONCLUSIVE (could not establish baseline session) ***');
    process.exit(2);
  }

  // ── Switch away: park the session (releaseQuiesced — no Disc, cable
  // stays attached, host stops polling). Mirrors quiesceActiveSessions.
  log('parking session (releaseQuiesced)…');
  await c1.releaseQuiesced();

  // ── The device-switch gap: host silent while device B runs. We wait
  // past REDUNDANT_AT so the redundant attach (BUGGY) lands here. ──
  log('idling through the device-switch gap…');
  await new Promise(r => setTimeout(r, 45_000));

  // ── Phase 2: return to device A, reconnect (adoption probe). ──
  rxBuf = [];
  const c2 = makeClient('B');
  c2.start();
  let ok = false;
  try {
    log('reconnecting #2 (adoption probe)…');
    await c2.connect(30_000);
    const drives = await c2.listDrives();
    log('RECONNECT ADOPTED — drives:', drives.join(' '));
    ok = drives.length > 0;
  } catch (e) {
    log('reconnect FAILED:', (e as Error).message);
  }

  console.error(ok
    ? '\n*** RECONNECT SUCCEEDED (session adopted after switch) ***'
    : '\n*** RECONNECT FAILED (session lost after switch) ***');
  await c2.releaseQuiesced().catch(() => {});
  process.exit(ok ? 0 : 1);
}
