// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// End-to-end test of the desktop drive sync against a real ROM.
//
// The mirror engine's logic is covered by unit tests
// (frontend/src/lib/__tests__/hostsync.mirror.test.mts). What this proves is the
// part no unit test can: that the engine, driving the real PlpClient over the
// harness's serial bridge, actually moves files onto and off a real EPOC
// machine's own C: drive — and how fast, which decides whether the feature is
// worth having at all.
//
// The whole chain runs for real:
//   lib/hostsync/mirror.ts      → planMirror / applyMirror
//   lib/hostsync/deviceFsPlp.ts → DeviceFs over PLP
//   lib/plp/*                   → PLP / NCP / RFSV-32 over a unix socket
//   harness/run                 → the actual ROM
//
// Five things are checked, in order, because each depends on the last:
//   1. push       — three host files land on the device, byte for byte
//   2. no-op      — re-planning straight afterwards produces no work
//   3. host del   — deleting one here removes it there
//   4. device add — a file created on the device arrives here
//   5. conflict   — both sides edited keeps BOTH copies, nothing destroyed
//
// Run (needs harness/run built):
//   node --experimental-strip-types tests/integration/test-drive-sync.mts \
//        [--device 5mx] [--seconds 420]

import * as net from 'node:net';
import * as fs from 'node:fs';
import * as fsp from 'node:fs/promises';
import * as os from 'node:os';
import * as path from 'node:path';
import { spawn } from 'node:child_process';
import { PlpClient } from '../../frontend/src/lib/plp/client-spec.ts';
import { createDeviceFs, defaultMirrorRoot } from '../../frontend/src/lib/hostsync/deviceFsPlp.ts';
import { applyMirror, planMirror } from '../../frontend/src/lib/hostsync/mirror.ts';
import {
  createMirrorState, type HostFile, type HostFs, type MirrorState,
} from '../../frontend/src/lib/hostsync/types.ts';

const REPO = path.resolve(new URL('../..', import.meta.url).pathname);
const HARNESS = path.join(REPO, 'harness', 'run');

const arg = (flag: string, fallback: string): string => {
  const i = process.argv.indexOf(flag);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : fallback;
};
const DEVICE = arg('--device', '5mx');
const SECONDS = Number(arg('--seconds', '420'));

interface DeviceMeta {
  rom: string;
  uart: number;
  linkProtocol: number;
  conSeq?: number;
  attachAt: number;
}
// Same shape as test-epocdir-install.mts, and the same reasoning: the cable is
// plugged in before EPOC's RemoteLinkServer is up, so connect() drives the
// handshake exactly as the browser does.
const DEVICES: Record<string, DeviceMeta> = {
  '5mx': { rom: '5mx_v1.05(260)_eng.bin', uart: 2, linkProtocol: 1, conSeq: 4, attachAt: 8 },
  netpad: { rom: 'Netpad.img', uart: 3, linkProtocol: 1, conSeq: 4, attachAt: 18 },
};

const meta = DEVICES[DEVICE];
if (!meta) {
  console.log(`\nRESULT: FAIL — unknown --device ${DEVICE} (have ${Object.keys(DEVICES).join(', ')})`);
  process.exit(1);
}
if (!fs.existsSync(HARNESS)) {
  console.log('\nRESULT: FAIL — harness/run is not built (bash harness/build.sh)');
  process.exit(1);
}
const ROM = path.join(REPO, 'roms', meta.rom);
const ROOT = defaultMirrorRoot(meta.linkProtocol);

let failures = 0;
function check(cond: unknown, msg: string) {
  console.log(`  ${cond ? '✓' : '✗'} ${msg}`);
  if (!cond) failures++;
}

// ── A host folder on the real filesystem ─────────────────────────────────
const hostDir = fs.mkdtempSync(path.join(os.tmpdir(), 'psion-sync-host-'));

const hostFs: HostFs = {
  async list(): Promise<HostFile[]> {
    const out: HostFile[] = [];
    const walk = async (dir: string, prefix: string) => {
      for (const e of await fsp.readdir(dir, { withFileTypes: true })) {
        if (e.name.startsWith('.')) continue;
        const full = path.join(dir, e.name);
        const rel = prefix ? `${prefix}/${e.name}` : e.name;
        if (e.isDirectory()) { await walk(full, rel); continue; }
        const st = await fsp.stat(full);
        out.push({ rel, size: st.size, mtimeMs: st.mtimeMs });
      }
    };
    await walk(hostDir, '');
    return out;
  },
  async read(rel) { return new Uint8Array(await fsp.readFile(path.join(hostDir, rel))); },
  async write(rel, data, mtimeMs) {
    const full = path.join(hostDir, rel);
    await fsp.mkdir(path.dirname(full), { recursive: true });
    await fsp.writeFile(full, data);
    if (mtimeMs) await fsp.utimes(full, new Date(mtimeMs), new Date(mtimeMs));
  },
  async remove(rel) { await fsp.rm(path.join(hostDir, rel), { force: true }); },
  async mkdirp(rel) { await fsp.mkdir(path.join(hostDir, rel), { recursive: true }); },
};

// EPOC16 will not accept anything but 8.3, and EPOC32's own short names are
// what a listing reports, so the test uses names that survive both unchanged —
// the mapping itself is unit-tested elsewhere.
const toDeviceName = (rel: string) => rel.toUpperCase().replace(/\//g, '\\');
const toHostName = (devRel: string) => devRel.toLowerCase();

// ── Harness + socket bridge ──────────────────────────────────────────────
const SOCK = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'psion-sock-')), 'plp.sock');
let rxBuf: number[] = [];
let sock: net.Socket | null = null;
const read = () => {
  if (rxBuf.length === 0) return new Uint8Array(0);
  const out = new Uint8Array(rxBuf);
  rxBuf = [];
  return out;
};
const write = (data: Uint8Array) => { sock?.write(Buffer.from(data)); return data.length; };

const LOG = arg('--log', path.join(os.tmpdir(), 'psion-drive-sync-harness.log'));
const logFd = fs.openSync(LOG, 'w');
const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(1);

let child: ReturnType<typeof spawn>;
let failure: string | null = null;
// Held so it can be closed afterwards: a listening server keeps the event loop
// alive, so without this the test prints its result and then hangs.
let bridgeServer: net.Server | null = null;

const childExited = new Promise<number>((resolve) => {
  const server = net.createServer((s) => {
    sock = s;
    s.on('data', (d) => { for (const b of d) rxBuf.push(b); });
    s.on('error', () => { /* harness gone */ });
    void run();
  });
  bridgeServer = server;
  server.listen(SOCK, () => {
    child = spawn(HARNESS, [
      ROM, '--device', DEVICE, '--quiet-logs',
      '--serial-bridge-socket', SOCK,
      '--serial-attach', String(meta.uart), String(meta.attachAt),
      '--serial-poll-until', String(SECONDS), String(meta.uart),
    ], {
      // Keeps sim time in step with the wall clock, so PLP's timeouts mean
      // what they say — and so the throughput number below is honest.
      env: { ...process.env, PSION_REALTIME: '1' },
      stdio: ['ignore', 'ignore', logFd],
    });
    child.on('exit', (code) => resolve(code ?? -1));
  });
});

async function run(): Promise<void> {
  const client = new PlpClient(read, write, {
    pollHz: 50, protocol: 'rfsv32', conSeq: meta.conSeq,
  });
  client.start();
  try {
    console.log(`[${elapsed()}s] connecting to the ${DEVICE} …`);
    await client.connect(90_000);
    console.log(`[${elapsed()}s] connected; mirroring ${ROOT}`);

    const device = createDeviceFs(client, { maxDepth: 1, excludeDirs: ['SYSTEM'] });
    await device.mkdirAll(ROOT).catch(() => undefined);

    let state: MirrorState = createMirrorState(ROOT);
    const cycle = async (label: string) => {
      const hostFiles = await hostFs.list();
      const deviceFiles = await device.list(ROOT);
      const plan = planMirror({ hostFiles, deviceFiles, state, toDeviceName, toHostName });
      console.log(`[${elapsed()}s] ${label}: ${plan.steps.length} step(s)` +
                  (plan.warnings.length ? ` — ${plan.warnings.join('; ')}` : ''));
      const result = await applyMirror(plan.steps, hostFs, device, state);
      state = result.state;
      for (const f of result.failed) {
        console.log(`    ! ${f.step.op} ${f.step.rel}: ${f.error}`);
      }
      return { plan, result };
    };

    // ── 1. Push ────────────────────────────────────────────────────────
    // Small on purpose: at single-digit KB/s a "realistic" folder would make
    // this test take a quarter of an hour.
    const seeded: Record<string, string> = {
      'NOTE.TXT': 'a note written on the computer',
      'TODO.TXT': 'buy milk\nfix the Psion\n',
      'DATA.TXT': 'x'.repeat(2048),
    };
    for (const [name, body] of Object.entries(seeded)) {
      await fsp.writeFile(path.join(hostDir, name), body);
    }

    const pushStart = Date.now();
    const push = await cycle('push');
    const pushBytes = Object.values(seeded).reduce((n, b) => n + b.length, 0);
    const pushSecs = (Date.now() - pushStart) / 1000;

    // A real 5mx boots with its stock Word and Sheet documents in C:\Documents
    // and holds them OPEN, so RFSV answers KErrInUse to any attempt to read
    // them. Every failure in this cycle must be that, and nothing else.
    const unexpected = push.result.failed
      .filter((f) => !/in use on the device|-14/.test(f.error));
    check(unexpected.length === 0,
          `no unexpected failures (${unexpected.map((f) => f.error).join('; ') || 'none'})`);
    check(push.result.completed >= 3,
          `the three seeded files were uploaded (${push.result.completed} steps completed)`);
    console.log(`[${elapsed()}s] MEASURED: ${pushBytes} B in ${pushSecs.toFixed(1)} s ` +
                `= ${(pushBytes / pushSecs / 1024).toFixed(2)} KB/s ` +
                `(${(pushSecs / 3).toFixed(1)} s per file, listings included)`);

    // Read each one back off the device through the client itself, so the
    // check does not trust the engine's own bookkeeping.
    for (const [name, body] of Object.entries(seeded)) {
      const got = await client.downloadFile(`${ROOT}\\${name}`);
      const want = new TextEncoder().encode(body);
      const same = got.length === want.length && got.every((b, i) => b === want[i]);
      check(same, `${name} is on the device byte-for-byte (${got.length} B)`);
    }

    // ── 2. No-op, on the files we own ──────────────────────────────────
    //
    // Not "no work at all": a real machine is not static. EPOC creates its
    // stock Word, Sheet, Data and Agenda documents in C:\Documents as it
    // settles, and holds them open, so new entries genuinely do keep appearing
    // and each is worth one attempt. What must hold is that the files the
    // mirror has already synced are left alone — if that fails it would
    // re-transfer the whole folder every cycle, forever, at under a KB/s.
    const quiet = await cycle('no-op');
    const touchedSeeded = quiet.plan.steps
      .filter((s) => Object.keys(seeded).some((n) => s.rel.toLowerCase() === n.toLowerCase()));
    check(touchedSeeded.length === 0,
          'a second cycle leaves the already-synced files alone ' +
          `(${touchedSeeded.map((s) => s.op + ':' + s.rel).join(', ') || 'nothing touched'})`);
    check(quiet.plan.warnings.some((w) => w.includes('in use on the device')),
          "and backs off the device's own locked documents rather than retrying them");

    // ── 3. A deletion here propagates there ────────────────────────────
    await fsp.rm(path.join(hostDir, 'TODO.TXT'));
    const afterDelete = await cycle('host delete');
    check(afterDelete.plan.steps.some((s) => s.op === 'delete-device'),
          'deleting a file here plans a device delete');
    const listed = await client.listDirectory(`${ROOT}\\`);
    check(!listed.some((e) => e.longName.toUpperCase() === 'TODO.TXT'),
          'and it is gone from the device');

    // ── 4. A file created on the device arrives here ───────────────────
    await client.uploadFile(`${ROOT}\\FROMDEV.TXT`,
                            new TextEncoder().encode('written on the Psion'));
    const afterDeviceAdd = await cycle('device add');
    check(afterDeviceAdd.plan.steps.some((s) => s.op === 'download'),
          'a file created on the device plans a download');
    const arrived = await fsp.readFile(path.join(hostDir, 'fromdev.txt'), 'utf8')
      .catch(() => '');
    check(arrived === 'written on the Psion',
          `it arrived in the host folder (${JSON.stringify(arrived.slice(0, 40))})`);

    // ── 5. A conflict keeps both copies ────────────────────────────────
    // The assertion that matters most: nothing the user wrote is destroyed.
    await fsp.writeFile(path.join(hostDir, 'NOTE.TXT'), 'edited on the computer');
    await client.uploadFile(`${ROOT}\\NOTE.TXT`,
                            new TextEncoder().encode('edited on the Psion'));
    const conflict = await cycle('conflict');
    const step = conflict.plan.steps.find((s) => s.op === 'conflict');
    check(!!step, 'both sides edited plans a conflict, not an overwrite');
    const names = await fsp.readdir(hostDir);
    const loser = names.find((n) => n.includes('from '));
    check(!!loser, `the losing copy was kept beside the winner (${names.join(', ')})`);
    if (loser) {
      const both = new Set([
        await fsp.readFile(path.join(hostDir, 'NOTE.TXT'), 'utf8'),
        await fsp.readFile(path.join(hostDir, loser), 'utf8'),
      ]);
      check(both.has('edited on the computer') && both.has('edited on the Psion'),
            'and both versions survive, one under each name');
    }
  } catch (err) {
    failure = err instanceof Error ? (err.stack ?? err.message) : String(err);
  } finally {
    try { await client.releaseQuiesced(); } catch { /* best effort */ }
    try { client.stop(); } catch { /* best effort */ }
    // The harness runs its own --serial-poll-until budget regardless, so it has
    // to be stopped rather than waited out: the checks finish in seconds and
    // the budget is minutes. SIGKILL after a grace period because a run mid-frame
    // does not always act on SIGTERM promptly.
    try { child?.kill('SIGTERM'); } catch { /* already gone */ }
    setTimeout(() => { try { child?.kill('SIGKILL'); } catch { /* gone */ } }, 2000).unref();
  }
}

await childExited;
await new Promise<void>((resolve) => {
  if (!bridgeServer) { resolve(); return; }
  bridgeServer.close(() => resolve());
});
fs.closeSync(logFd);
fs.rmSync(hostDir, { recursive: true, force: true });

if (failure) {
  console.log(`\nRESULT: FAIL — ${failure}`);
  console.log(`harness log: ${LOG}`);
  process.exit(1);
}
if (failures) {
  console.log(`\nRESULT: FAIL — ${failures} check(s) failed`);
  console.log(`harness log: ${LOG}`);
  process.exit(1);
}
console.log('\nRESULT: PASS — the drive sync works against a real ROM');
