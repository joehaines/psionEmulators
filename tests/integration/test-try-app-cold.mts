// Browser-equivalent "Try it on a cold-booting device" test.
//
// Runs the REAL WASM module (frontend/public/psion.js — same artefact
// the site ships) under node, and replicates the worker's cold-boot
// try-flow exactly: load ROM → start stepping (no preroll) → after the
// zip-fetch delay, run the REAL lib/appLibrary.ts deliverApp() against
// module-backed controls → keep stepping → probe liveness with a real
// key press and LCD diffs.
//
// Two attach delays are tested per device: ~0.2 s sim (service-worker
// cache hit — the earliest the browser can attach) and ~1.5 s (cold
// network fetch).
//
// Run (after scripts/build-wasm.sh):
//   node --experimental-strip-types scripts/test-try-app-cold.mts [deviceId]

import * as fs from 'node:fs';
import * as path from 'node:path';
import { createRequire } from 'node:module';
import { deliverApp, type AppEntry, type DeviceProfileLike } from '../../frontend/src/lib/appLibrary.ts';

const REPO = path.resolve(new URL('../..', import.meta.url).pathname);
const PUB = path.join(REPO, 'frontend', 'public');
const require_ = createRequire(import.meta.url);

const manifest = JSON.parse(fs.readFileSync(path.join(PUB, 'apps', 'manifest.json'), 'utf8'));
const appById = new Map<string, AppEntry>(manifest.apps.map((a: AppEntry) => [a.id, a]));

// frontend/package.json is "type":"module", which makes node treat the
// CommonJS-shimmed psion.js as ESM and lose its export — stage it as
// .cjs (with the wasm beside it) so require() yields the factory.
const STAGE = fs.mkdtempSync('/tmp/psion-try-');
fs.copyFileSync(path.join(PUB, 'psion.js'), path.join(STAGE, 'psion.cjs'));
fs.copyFileSync(path.join(PUB, 'psion.wasm'), path.join(STAGE, 'psion.wasm'));

// Device → app under test. 3ahexx / castle2 are the apps from the field
// report; 5lostin3d is the canonical small EPOC SIS.
const CASES: { device: string; app: string }[] = [
  { device: 'series3a',  app: 's3games/3ahexx' },
  { device: 'series3c',  app: 's3games/3ahexx' },
  { device: 'series3mx', app: 's3games/3ahexx' },
  { device: 'pocketbk2', app: 's3games/3ahexx' },
  { device: 'workabout', app: 's3games/3ahexx' },
  { device: 'siena',     app: 'siena/castle2' },
  { device: 'series5',   app: 'epocgames/5lostin3d' },
  { device: '5mx',       app: 'epocgames/5lostin3d' },
  { device: 'mc218',     app: 'epocgames/5lostin3d' },
  { device: 'series7',   app: 'epocgames/5lostin3d' },
];

// Per-device boot budget (sim seconds) before the liveness probe — slow
// bootters (Siena ~35 s, SA-1100 ~20 s) need their full window.
const BOOT_SECONDS: Record<string, number> = {
  // The 5mx-class welcome boot takes ~25 s before the System screen
  // accepts keys (screenshot-verified); the ER1 Series 5 is slower
  // still. mc218 ships a faster boot and fits the default.
  siena: 45, series7: 30, series5: 45, '5mx': 35, series3mx: 16, default: 16,
};

interface Mod {
  HEAPU8: Uint8Array;
  _malloc(n: number): number; _free(p: number): void;
  prepareROMUpload(n: number): number;
  loadBufferedROM(n: number, id: string): string;
  getDeviceInfo(): { deviceName: string; lcdWidth: number; lcdHeight: number } | undefined;
  getAllDeviceProfilesJSON(): string;
  stepFrameFull(): void;
  readLCD(p: number): void;
  sendKey(k: number, down: boolean): void;
  prepareCFImageUpload(n: number): number;
  attachCFImage(n: number): boolean;
  isCFImageAttached(): boolean;
  prepareSSDImageUpload(n: number): number;
  attachSSDImage(slot: number, n: number, t: number): boolean;
  isSSDImageAttached(slot: number): boolean;
}

function lcdSnapshot(mod: Mod, w: number, h: number): Uint8Array {
  const p = mod._malloc(w * h * 4);
  try {
    mod.readLCD(p);
    return mod.HEAPU8.slice(p, p + w * h * 4);
  } finally { mod._free(p); }
}

function diffCount(a: Uint8Array, b: Uint8Array): number {
  let n = 0;
  for (let i = 0; i < a.length; i += 16) if (a[i] !== b[i]) n++;
  return n;
}

function variance(a: Uint8Array): number {
  let sum = 0, count = 0;
  for (let i = 0; i < a.length; i += 16) { sum += a[i]; count++; }
  const mean = sum / count;
  let v = 0;
  for (let i = 0; i < a.length; i += 16) v += (a[i] - mean) ** 2;
  return v / count;
}

function makeControls(mod: Mod) {
  // The slice of EmulatorControls deliverApp touches, backed directly by
  // the module — equivalent to the worker RPCs.
  return {
    get cardAttached() { return mod.isCFImageAttached(); },
    getCardBytes: () => null,
    attachCard: async (bytes: Uint8Array) => {
      const p = mod.prepareCFImageUpload(bytes.length);
      mod.HEAPU8.set(bytes, p);
      return mod.attachCFImage(bytes.length);
    },
    get ssdAttached() { return [0, 1, 2, 3].map(s => mod.isSSDImageAttached(s)); },
    attachSSD: async (slot: number, bytes: Uint8Array,
                      kind?: 'ram' | 'flash' | 'protected') => {
      const p = mod.prepareSSDImageUpload(bytes.length);
      mod.HEAPU8.set(bytes, p);
      // Mirrors PsionSSD::Type (1 RAM / 2 Flash / 3 write-protected).
      return mod.attachSSDImage(slot, bytes.length,
        kind === 'protected' ? 3 : kind === 'flash' ? 2 : 1);
    },
    serialIsAttached: () => false,
    serialAttachHost: () => false,
    serialDetachHost: () => false,
    serialReadBytes: () => new Uint8Array(0),
    serialWriteBytes: () => 0,
    currentDeviceId: null,
  } as never;
}

async function runCase(device: string, appId: string, attachFrames: number): Promise<string> {
  const entry = appById.get(appId);
  if (!entry) return `SKIP (app ${appId} not in manifest)`;
  const factory = require_(path.join(STAGE, 'psion.cjs'));
  const mod: Mod = await factory({
    // The artefact is built for the web (fetch-based wasm loading);
    // hand it the bytes directly so it runs under node unchanged.
    wasmBinary: new Uint8Array(fs.readFileSync(path.join(STAGE, 'psion.wasm'))),
    print: () => {}, printErr: () => {},
  });
  const profiles = JSON.parse(mod.getAllDeviceProfilesJSON()) as
    ({ id: string; romFilename: string } & DeviceProfileLike)[];
  const profile = profiles.find(p => p.id === device);
  if (!profile) return 'SKIP (no profile)';
  const romPath = path.join(REPO, 'roms', profile.romFilename);
  if (!fs.existsSync(romPath)) return `SKIP (ROM ${profile.romFilename} missing)`;

  const rom = new Uint8Array(fs.readFileSync(romPath));
  const rp = mod.prepareROMUpload(rom.length);
  mod.HEAPU8.set(rom, rp);
  mod.loadBufferedROM(rom.length, device);
  const info = mod.getDeviceInfo();
  if (!info || !info.lcdWidth) return 'FAIL (no device info)';
  const { lcdWidth: w, lcdHeight: h } = info;

  // Cold boot starts stepping immediately (worker preroll = 0)…
  for (let i = 0; i < attachFrames; i++) mod.stepFrameFull();

  // …then the app lands, via the production delivery code. The link
  // fields are stripped so SIS apps take the CF route here: this
  // matrix exists to catch storage-attach timing wedges (the link path
  // is a serial protocol exercised by the PLP e2e suites and needs a
  // live pump loop this synchronous rig doesn't have).
  const zip = new Uint8Array(fs.readFileSync(path.join(PUB, 'apps', entry.zip)));
  const result = await deliverApp(entry, zip, makeControls(mod),
    { ...profile, remoteLinkUart: -1 });
  void result;

  // Run out the boot window.
  const bootFrames = (BOOT_SECONDS[device] ?? BOOT_SECONDS.default) * 64;
  for (let i = attachFrames; i < bootFrames; i++) mod.stepFrameFull();

  const booted = lcdSnapshot(mod, w, h);
  if (variance(booted) < 50) return `FAIL (blank/flat screen after boot, variance ${variance(booted).toFixed(0)})`;

  // Liveness: Menu key must visibly change the screen.
  mod.sendKey(148, true);
  for (let i = 0; i < 4; i++) mod.stepFrameFull();
  mod.sendKey(148, false);
  for (let i = 0; i < 96; i++) mod.stepFrameFull();
  const afterMenu = lcdSnapshot(mod, w, h);
  const changed = diffCount(booted, afterMenu);
  if (changed < 20) return `FAIL (no response to Menu key, ${changed} px changed)`;

  return `PASS (boot var ${variance(booted).toFixed(0)}, menu Δ${changed}px)`;
}

const only = process.argv[2];
let failures = 0;
for (const c of CASES) {
  if (only && c.device !== only) continue;
  for (const attachFrames of [12, 96]) {
    const label = `${c.device} + ${c.app} @${(attachFrames / 64).toFixed(2)}s`;
    try {
      const r = await runCase(c.device, c.app, attachFrames);
      if (r.startsWith('FAIL')) failures++;
      console.log(`${r.startsWith('PASS') ? '✓' : r.startsWith('SKIP') ? '–' : '✗'} ${label}: ${r}`);
    } catch (e) {
      failures++;
      console.log(`✗ ${label}: THREW ${e instanceof Error ? e.message : e}`);
    }
  }
}
process.exit(failures > 0 ? 1 : 0);
