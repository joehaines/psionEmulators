// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// End-to-end test of the 'epocdir' install path against a real ROM: an
// app bundle that is an already-installed \System\Apps\<App>\ folder
// (rather than a .SIS) is delivered over the Remote Link and must land,
// byte-for-byte, in C:\System\Apps\<App>\ on the device's internal disk.
//
// The whole production chain runs for real:
//   scripts/build-app-library.mts  → manifest entry + app zip
//   lib/appLibrary.ts deliverApp() → MkDirAll + per-file RFSV upload
//   lib/plp/*                      → PLP/NCP/RFSV-32 over the harness's
//                                    unix-socket serial bridge
//   harness/run                    → the actual 5mx ROM
//
// It then reads every file back off the device and compares it with the
// bundle, and (with --snapshot PATH) saves the device's RAM so the
// companion script can boot the machine again and check the app really
// shows up in Extras — see tests/integration/test-epocdir-install.sh.
//
// Run (needs harness/run built):
//   node --experimental-strip-types tests/integration/test-epocdir-install.mts \
//        [--app epocgames/wallinstalled] [--snapshot ram.bin] [--seconds 300]

import * as net from 'node:net';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';
import { deliverApp, epocAppFolder, epocDirPathFor, type AppEntry, type DeviceProfileLike }
  from '../../frontend/src/lib/appLibrary.ts';
import { PlpClient } from '../../frontend/src/lib/plp/client-spec.ts';
import { unzipAll } from '../../frontend/src/lib/zip.ts';

const REPO = path.resolve(new URL('../..', import.meta.url).pathname);
const HARNESS = path.join(REPO, 'harness', 'run');
const ROM = path.join(REPO, 'roms', '5mx_v1.05(260)_eng.bin');

const arg = (flag: string, fallback: string): string => {
  const i = process.argv.indexOf(flag);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : fallback;
};
const APP_ID = arg('--app', 'epocgames/wallinstalled');
const SNAPSHOT = arg('--snapshot', '');
// Sim seconds the harness runs for. It only writes the RAM snapshot and
// the screenshot when the poll loop ends, so this has to outlast the
// boot (~35 s) plus the transfer; the run below reports how much of the
// budget it actually used.
const SECONDS = Number(arg('--seconds', '300'));

// The 5mx's link-relevant DeviceProfile fields (core/device_registry.cpp).
// hasCFSlot is deliberately false: the real profile has one, but here a
// stalled cable must fail the test rather than silently fall back to
// card delivery, which is a different code path with its own test.
const PROFILE: DeviceProfileLike = {
  id: '5mx', hasCFSlot: false, ssdSlotCount: 0, remoteLinkUart: 2, linkProtocol: 1,
};

function fail(msg: string): never {
  console.log(`\nRESULT: FAIL — ${msg}`);
  process.exit(1);
}

// ── The app bundle, built by the real pipeline ──────────────────────
const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'psion-applib-'));
const category = APP_ID.split('/')[0];
console.log(`Building the app library (${category}) …`);
const build = spawnSync(process.execPath,
  ['--experimental-strip-types', path.join(REPO, 'scripts', 'build-app-library.mts'),
   '--out', outDir, '--category', category],
  { encoding: 'utf8' });
if (build.status !== 0) fail(`build-app-library failed: ${build.stderr}`);

const manifestJson = fs.readFileSync(path.join(outDir, 'manifest.json'), 'utf8');
const manifest = JSON.parse(manifestJson) as { apps: AppEntry[] };
const entry = manifest.apps.find(a => a.id === APP_ID);
if (!entry) fail(`${APP_ID} is not in the built manifest`);
if (entry.installKind !== 'epocdir') {
  fail(`${APP_ID} has installKind '${entry.installKind}', expected 'epocdir'`);
}
if (!entry.installFile) fail(`${APP_ID} has no installFile`);
const folder = epocAppFolder(entry.installFile);
const zipBytes = new Uint8Array(fs.readFileSync(path.join(outDir, entry.zip)));
const bundle = await unzipAll(zipBytes);
console.log(`${entry.name}: ${bundle.size} files, ${entry.sizeBytes} bytes ` +
            `→ C:\\System\\Apps\\${folder}\\`);

// ── Harness + socket bridge ─────────────────────────────────────────
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

// The slice of EmulatorControls deliverApp touches on the link path.
// The cable is attached by the harness itself (--serial-attach), so the
// attach calls just report success.
const controls = {
  cardAttached: false,
  getCardBytes: () => null,
  attachCard: async () => false,
  ssdAttached: [false, false, false, false],
  attachSSD: async () => false,
  serialIsAttached: () => true,
  serialAttachHost: () => true,
  serialDetachHost: () => true,
  serialReadBytes: () => read(),
  serialWriteBytes: (_uart: number, data: Uint8Array) => write(data),
  currentDeviceId: '5mx',
} as never;

const harnessArgs = [
  ROM, '--device', '5mx', '--quiet-logs',
  '--serial-bridge-socket', SOCK,
  // Attach the cable at t=8 s — before EPOC's RemoteLinkServer is up, so
  // the client's connect() drives the handshake as the browser does.
  '--serial-attach', '2', '8',
  '--serial-poll-until', String(SECONDS), '2',
];
if (SNAPSHOT) harnessArgs.push('--save-ram-snapshot', SNAPSHOT);
// --screenshot PATH and repeated --tap AT_SEC X Y are passed through to
// the harness, so a caller can drive the device's UI after the install
// (the .sh wrapper uses them to open Extras and start the app).
const SHOT = arg('--screenshot', '');
if (SHOT) harnessArgs.push('--screenshot', SHOT);
for (let i = 0; i < process.argv.length; i++) {
  if (process.argv[i] === '--tap' && i + 3 < process.argv.length) {
    harnessArgs.push('--tap-seq', process.argv[i + 1], process.argv[i + 2], process.argv[i + 3]);
  }
}

// The harness's own log — where to look when a run fails.
const LOG = arg('--log', path.join(os.tmpdir(), 'psion-epocdir-harness.log'));
const logFd = fs.openSync(LOG, 'w');

const t0 = Date.now();
const elapsed = () => ((Date.now() - t0) / 1000).toFixed(1);
let child: ReturnType<typeof spawn>;
const childExited = new Promise<number>(resolve => {
  const server = net.createServer(s => {
    sock = s;
    s.on('data', d => { for (const b of d) rxBuf.push(b); });
    s.on('error', () => { /* harness gone */ });
    void run();
  });
  server.listen(SOCK, () => {
    child = spawn(HARNESS, harnessArgs, {
      // PSION_REALTIME keeps sim time in step with the host clock, so the
      // PLP layer's (wall-clock) timeouts mean what they say.
      env: { ...process.env, PSION_REALTIME: '1' },
      stdio: ['ignore', 'ignore', logFd],
    });
    child.on('exit', code => resolve(code ?? -1));
  });
});

let failure: string | null = null;

async function run(): Promise<void> {
  try {
    // ── Deliver, through the production code path ──────────────────
    let lastPct = -1;
    const result = await deliverApp(entry!, zipBytes, controls, PROFILE,
      phase => console.log(`  [${elapsed()}s] ${phase}`),
      (done, total) => {
        const pct = Math.floor((done / total) * 100);
        if (pct >= lastPct + 20) { lastPct = pct; console.log(`  [${elapsed()}s] ${pct}%`); }
      });
    console.log(`  [${elapsed()}s] ${result.summary}`);

    // ── Read it all back off the device ────────────────────────────
    // A fresh client resumes the session deliverApp parked.
    const client = new PlpClient(read, write, { pollHz: 50, protocol: 'rfsv32', conSeq: 4 });
    client.start();
    try {
      await client.connect(60_000);
      // Every bundle file must be on the device at the path the
      // delivery code targets, with the right length. Directories are
      // listed once each; the DLLs a bundle carries for \System\Libs
      // are checked exactly like the app's own files.
      const byDir = new Map<string, { base: string; data: Uint8Array }[]>();
      for (const [rel, data] of bundle) {
        const target = epocDirPathFor(entry!.installFile!, rel);
        const cut = target.lastIndexOf('\\');
        const dir = `C:${target.slice(0, cut + 1)}`;
        if (!byDir.has(dir)) byDir.set(dir, []);
        byDir.get(dir)!.push({ base: target.slice(cut + 1), data });
      }
      for (const [dir, wanted] of byDir) {
        const listed = await client.listDirectory(dir);
        console.log(`  [${elapsed()}s] ${dir} holds ${listed.length} entries ` +
                    `(${wanted.length} from the bundle)`);
        for (const { base, data } of wanted) {
          const found = listed.find(e => e.longName.toLowerCase() === base.toLowerCase());
          if (!found) throw new Error(`${base} is not in ${dir}`);
          if (found.size !== data.length) {
            throw new Error(`${dir}${base} is ${found.size} bytes on the device, ` +
                            `${data.length} in the bundle`);
          }
        }
      }
      // Byte-compare the files that decide whether the app runs at all:
      // the binary, its registration file, and the loader DLL its
      // compressed stub pulls in.
      const critical = [...bundle.keys()].filter(
        k => k === entry!.installFile || /\.(reg|dll)$/i.test(k));
      for (const rel of critical) {
        const want = bundle.get(rel)!;
        const got = await client.downloadFile(`C:${epocDirPathFor(entry!.installFile!, rel)}`);
        if (got.length !== want.length) {
          throw new Error(`${rel} read back as ${got.length} bytes, expected ${want.length}`);
        }
        for (let i = 0; i < want.length; i++) {
          if (got[i] !== want[i]) throw new Error(`${rel} differs at byte ${i}`);
        }
        console.log(`  [${elapsed()}s] ${rel} round-trips byte-for-byte (${want.length} B)`);
      }
    } finally {
      try { await client.releaseQuiesced(); } catch { /* best effort */ }
    }
  } catch (e) {
    failure = e instanceof Error ? e.message : String(e);
  }
  if (failure) {
    // Nothing more to learn from this run — stop the harness now.
    try { child.kill('SIGKILL'); } catch { /* already gone */ }
  } else if (SNAPSHOT || SHOT) {
    // The snapshot and the screenshot are only written when the poll
    // loop ends, and any scheduled taps still have to fire.
    console.log(`  [${elapsed()}s] transfer done; letting the harness run out to ${SECONDS}s…`);
  } else {
    try { child.kill('SIGKILL'); } catch { /* already gone */ }
  }
}

await childExited;
if (failure) fail(failure);
if (SNAPSHOT && !fs.existsSync(SNAPSHOT)) fail(`the harness did not write ${SNAPSHOT}`);

// --assert-launched: the caller tapped the app open, so the final
// screenshot must no longer be the System screen. Compared against the
// device's committed boot golden — a running app repaints the whole
// LCD, which separates it cleanly from the ways this can go wrong.
// Measured against this ROM: 40% of sampled pixels differ with Wall
// running, 22% with only the Extras bar open (app installed but never
// started), 13% for the "Program closed" panel a broken binary leaves
// on top of the System screen. 30% sits between the good case and both
// bad ones.
if (process.argv.includes('--assert-launched')) {
  if (!SHOT) fail('--assert-launched needs --screenshot');
  const readPgm = (p: string): Uint8Array => {
    const buf = fs.readFileSync(p);
    // P5 <width> <height> <maxval>\n<binary>
    let at = 0;
    const token = () => {
      while (at < buf.length && /\s/.test(String.fromCharCode(buf[at]))) at++;
      let s = '';
      while (at < buf.length && !/\s/.test(String.fromCharCode(buf[at]))) s += String.fromCharCode(buf[at++]);
      return s;
    };
    if (token() !== 'P5') throw new Error(`${p} is not a binary PGM`);
    const w = Number(token()), h = Number(token());
    token();
    at++;
    return new Uint8Array(buf.subarray(at, at + w * h));
  };
  const shot = readPgm(SHOT);
  const golden = readPgm(path.join(REPO, 'tests', 'golden', '5mx.pgm'));
  if (shot.length !== golden.length) fail('screenshot and golden differ in size');
  let differing = 0;
  for (let i = 0; i < shot.length; i += 8) if (shot[i] !== golden[i]) differing++;
  const pct = (differing / Math.ceil(shot.length / 8)) * 100;
  if (pct < 30) {
    fail(`the app does not appear to have opened — only ${pct.toFixed(1)}% of the ` +
         `screen differs from the System screen (see ${SHOT})`);
  }
  console.log(`  [${elapsed()}s] launched: ${pct.toFixed(1)}% of the screen changed ` +
              `from the System screen`);
}
console.log(`\nRESULT: PASS — ${entry.name} installed into C:\\System\\Apps\\${folder}\\ ` +
            `and verified against the bundle (${elapsed()}s)`);
process.exit(0);
