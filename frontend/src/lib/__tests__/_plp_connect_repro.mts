// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Deterministic repro for the Remote Link CONNECT failure that the
// browser "Try it" flow hits randomly. Runs the REAL PlpClient against
// the REAL 5mx ROM via the native harness, but — unlike _plp_repro —
// attaches the host cable EARLY (during boot) and connects immediately,
// exactly as the app-library delivery does, so the host's handshake
// races the device's boot. Repeats the connect N times and reports how
// many succeed, so a fix can be measured.
//
// Usage:
//   node --experimental-strip-types frontend/src/lib/__tests__/_plp_connect_repro.mts [attachSec] [trials]
//   PDU=1 …   dump every PDU each way (single trial)

import * as net from 'node:net';
import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';
import type { Pdu } from '../plp/link.ts';

const REPO = new URL('../../../../', import.meta.url).pathname;
const HARNESS = `${REPO}harness/run`;
const ROM = `${REPO}roms/5mx_v1.05(260)_eng.bin`;
const ATTACH_SEC = process.argv[2] ?? '1';      // attach during boot
const TRIALS = Number(process.argv[3] ?? (process.env.PDU ? 1 : 8));
const PDU = !!process.env.PDU;

function pduStr(p: Pdu): string {
  return `cont=${p.cont} seq=${p.seq} len=${p.data.length}`;
}

async function oneTrial(i: number): Promise<{ ok: boolean; sawDisc: boolean; detail: string }> {
  const SOCK = `/tmp/psion-plp-connect-${i}.sock`;
  try { fs.unlinkSync(SOCK); } catch { /* not there */ }
  let rxBuf: number[] = [];
  let sock: net.Socket | null = null;
  let sawDisc = false;
  let lastState = '';

  return await new Promise(resolve => {
    let done = false;
    const finish = (r: { ok: boolean; sawDisc: boolean; detail: string }) => {
      if (done) return; done = true;
      try { child.kill('SIGKILL'); } catch { /* ignore */ }
      try { server.close(); } catch { /* ignore */ }
      resolve(r);
    };

    const server = net.createServer(s => {
      sock = s;
      s.on('data', d => { for (const b of d) rxBuf.push(b); });
      s.on('error', () => { /* ignore */ });
      void runClient();
    });
    server.listen(SOCK, () => {
      const child2 = spawn(HARNESS, [ROM, '--device', '5mx', '--quiet-logs',
        '--serial-bridge-socket', SOCK,
        '--serial-attach', '2', String(ATTACH_SEC),
        '--serial-poll-until', '90', '2'],
        { env: { ...process.env, PSION_REALTIME: '1' }, stdio: ['ignore', 'ignore', 'ignore'] });
      (server as unknown as { _child: unknown })._child = child2;
    });
    const child = { kill: (sig: string) => { const c = (server as unknown as { _child?: { kill: (s: string) => void } })._child; c?.kill(sig); } };

    async function runClient(): Promise<void> {
      const client = new PlpClient(
        () => { if (rxBuf.length === 0) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; },
        d => { sock?.write(Buffer.from(d)); return d.length; },
        {
          pollHz: 50, protocol: 'rfsv32', conSeq: 4,
          onState: s => { lastState = s; if (PDU) console.error('  STATE', s); },
          onPduIn: p => { if (p.cont === 1) sawDisc = true; if (PDU) console.error('  IN ', pduStr(p)); },
          onPduOut: p => { if (PDU) console.error('  OUT', pduStr(p)); },
        },
      );
      client.start();
      try {
        await client.connect(45_000);
        const drives = await client.listDrives();
        finish({ ok: true, sawDisc, detail: `drives=${drives.join('')}` });
      } catch (e) {
        finish({ ok: false, sawDisc, detail: `${(e as Error).message} (lastState=${lastState})` });
      } finally {
        try { client.disconnect(); } catch { /* ignore */ }
      }
    }
  });
}

(async () => {
  console.log(`connect repro: 5mx, attach@${ATTACH_SEC}s (during boot), ${TRIALS} trial(s)`);
  let ok = 0, disc = 0;
  for (let i = 0; i < TRIALS; i++) {
    const r = await oneTrial(i);
    if (r.ok) ok++;
    if (r.sawDisc) disc++;
    console.log(`  trial ${i + 1}: ${r.ok ? 'OK  ' : 'FAIL'} ${r.sawDisc ? '[Disc]' : '      '} ${r.detail}`);
  }
  console.log(`\nRESULT: ${ok}/${TRIALS} connected; ${disc}/${TRIALS} saw a device Disc_Pdu`);
  process.exit(0);
})();
