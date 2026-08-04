// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// End-to-end test of the netpad's app-library install path against the
// real Netpad.img ROM: the machine shipped with no applications in its
// EPOC R5 ROM (Psion Teklogix put them on a support CD, preserved in
// applib/netpad), and the emulator's "Install standard apps" button
// leads to that set. This drives one of them onto the device and
// requires it to end up in Extras.
//
// --app takes any app in the library catalogued for the netpad, which
// since the machine joined EPOC_DEVICES means the whole ER5 catalogue
// and not just its own CD set: --app epocgames/fred installs a 3-Lib
// game by the same route.
//
// The whole production chain runs for real:
//   scripts/build-app-library.mts  → manifest entry + app zip
//   lib/appLibrary.ts deliverApp() → 'cf' delivery: the .SIS written
//                                    into a FAT16 image, host-side
//   harness/run                    → the actual netpad ROM, which
//                                    mounts the image over its MMC SPI
//                                    port as drive D:
//
// The device is then driven through the install by pen and key, exactly
// as the delivery instructions tell the user to: tap the disk button,
// pick D:, open the .SIS, confirm the installer, then tap the Extras
// silkscreen key. The check is a second, identical run with an empty
// card: with nothing to install the final screens must differ (the
// repo's idiom for asserting on a guest screen without OCR — see
// test-netpad-mmc.sh).
//
// Run (needs harness/run built):
//   node --experimental-strip-types tests/integration/test-netpad-app-install.mts \
//        [--app netpad/word] [--keep]

import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { spawnSync } from 'node:child_process';
import { deliverApp, type AppEntry, type DeviceProfileLike }
  from '../../frontend/src/lib/appLibrary.ts';
import { listDirectory } from '../../frontend/src/lib/fat16.ts';
import { createBlankImage } from '../../frontend/src/lib/fat16.ts';

const REPO = path.resolve(new URL('../..', import.meta.url).pathname);
const HARNESS = path.join(REPO, 'harness', 'run');
const ROM = path.join(REPO, 'roms', 'Netpad.img');

const arg = (flag: string, fallback: string): string => {
  const i = process.argv.indexOf(flag);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : fallback;
};
const APP_ID = arg('--app', 'netpad/word');
const KEEP = process.argv.includes('--keep');

// The netpad's delivery-relevant DeviceProfile fields
// (core/device_registry.cpp): no PC-Card socket, an MMC slot, and a
// Remote Link on UART3 that deliveryKindFor must NOT choose for a .SIS
// — the slot mounts as D: with nothing switched on first, where the
// link wants Remote link enabled from the Tools menu on real hardware,
// which is why the card is the netpad's route.
const PROFILE: DeviceProfileLike = {
  id: 'netpad', hasCFSlot: false, hasMmcSlot: true, ssdSlotCount: 0,
  remoteLinkUart: 3, linkProtocol: 1,
};

function fail(msg: string): never {
  console.log(`\nRESULT: FAIL — ${msg}`);
  process.exit(1);
}

if (!fs.existsSync(HARNESS)) fail('harness/run not built — run bash harness/build.sh');
if (!fs.existsSync(ROM)) {
  console.log('SKIP: Netpad.img not present');
  process.exit(0);
}

// ── The app bundle and the card, built by the real pipeline ─────────
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'psion-netpad-install-'));
const category = APP_ID.split('/')[0];
console.log(`Building the app library (${category}) …`);
const build = spawnSync(process.execPath,
  ['--experimental-strip-types', path.join(REPO, 'scripts', 'build-app-library.mts'),
   '--out', work, '--category', category],
  { encoding: 'utf8' });
if (build.status !== 0) fail(`build-app-library failed: ${build.stderr}`);

const manifest = JSON.parse(fs.readFileSync(path.join(work, 'manifest.json'), 'utf8')) as
  { apps: AppEntry[] };
const entry = manifest.apps.find(a => a.id === APP_ID);
if (!entry) fail(`${APP_ID} is not in the built manifest`);
if (entry.installKind !== 'sis') {
  fail(`${APP_ID} has installKind '${entry.installKind}', expected 'sis'`);
}
if (!entry.devices.includes('netpad')) fail(`${APP_ID} is not catalogued for the netpad`);

let cardImage: Uint8Array | null = null;
const controls = {
  cardAttached: false,
  getCardBytes: () => null,
  attachCard: async (bytes: Uint8Array) => { cardImage = bytes; return true; },
  ssdAttached: [] as boolean[],
} as never;
const zipBytes = new Uint8Array(fs.readFileSync(path.join(work, entry.zip)));
const result = await deliverApp(entry, zipBytes, controls, PROFILE);
if (!cardImage) fail('deliverApp did not attach a card — the netpad took another route');
if (!result.summary.includes('MMC card')) {
  fail(`delivery summary does not name the MMC card: ${result.summary}`);
}
const onCard = listDirectory(cardImage, 0).map(e => e.name);
console.log(`${entry.name}: delivered as ${onCard.join(', ')} — ${result.summary}`);
if (onCard.length !== 1) fail(`expected one file on the card, got ${onCard.join(', ') || 'none'}`);

const cardPath = path.join(work, 'card.img');
const emptyPath = path.join(work, 'empty.img');
fs.writeFileSync(cardPath, Buffer.from(cardImage.buffer, cardImage.byteOffset, cardImage.byteLength));
const blank = createBlankImage(16 * 1024 * 1024);
fs.writeFileSync(emptyPath, Buffer.from(blank.buffer, blank.byteOffset, blank.byteLength));

// ── Driving the device ──────────────────────────────────────────────
// Sim-second script, measured against the booted ROM. The netpad reaches
// its desktop at ~14 s and the MMC mount chain (card detect → socket
// power-up → medmmc → F32) finishes a few seconds after the card goes in
// at 15 s. Coordinates are netpad digitiser pixels: the panel is
// 640 x 240 and the five silkscreen keys sit in a column at x 649..674
// beyond its right edge (core/sa1100.h kNpSilkscreen*).
//   10,228   the System screen's disk button, bottom-left
//   60,214   the "D" row of the disk list it opens
//   661,216  the Extras silkscreen key, bottom of the five
// The three Enters are: open the selected .SIS, OK the installer's
// "About to install" dialog, OK "Installation complete".
const SCRIPT = [
  '--tap-seq', '45', '10', '228',
  '--tap-seq', '50', '60', '214',
  '--press-key', '55', '3',
  '--press-key', '62', '3',
  '--press-key', '75', '3',
  '--tap-seq', '82', '661', '216',
];

function run(card: string, shot: string, log: string): void {
  const r = spawnSync(HARNESS, [
    ROM, '--device', 'netpad', '--quiet-logs',
    '--boot-seconds', '15', '--card-path', card, '--post-attach-seconds', '80',
    '--screenshot', shot, ...SCRIPT,
  ], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  fs.writeFileSync(log, `${r.stdout ?? ''}${r.stderr ?? ''}`);
  if (r.status !== 0) fail(`harness exited ${r.status} (see ${log})`);
}

const shotInstalled = path.join(work, 'extras-installed.pgm');
const shotEmpty = path.join(work, 'extras-empty.pgm');
console.log('Running the netpad with the delivered card …');
run(cardPath, shotInstalled, path.join(work, 'installed.log'));
console.log('Running the netpad with an empty card (baseline) …');
run(emptyPath, shotEmpty, path.join(work, 'empty.log'));

const a = fs.readFileSync(shotInstalled);
const b = fs.readFileSync(shotEmpty);
if (a.length === 0) fail('no screenshot was written');
if (a.equals(b)) {
  fail('Extras looks identical with and without the app on the card — nothing installed');
}

// Where the app lands: the Extras bar is drawn down the right-hand side
// of the panel, and an installed app fills its top slot. Requiring the
// difference to be there (rather than anywhere on screen) keeps the
// check honest if the two runs ever diverge for an unrelated reason.
const EXTRAS_X0 = 560, SLOT_Y0 = 0, SLOT_Y1 = 60, W = 640;
const header = /^P5\s+(\d+)\s+(\d+)\s+(\d+)\s/.exec(a.subarray(0, 32).toString('latin1'));
if (!header) fail('screenshot is not a binary PGM');
const pixels = a.length - (header[0].length);
if (pixels < W * 240) fail(`screenshot is smaller than the panel (${pixels} bytes)`);
let slotDiff = 0;
for (let y = SLOT_Y0; y < SLOT_Y1; y++) {
  for (let x = EXTRAS_X0; x < W; x++) {
    const i = header[0].length + y * W + x;
    if (a[i] !== b[i]) slotDiff++;
  }
}
if (slotDiff === 0) fail("the Extras bar's top slot is unchanged — the app is not listed");

console.log(`Extras differs in ${slotDiff} pixels of its top slot.`);
console.log(`RESULT: PASS — ${entry.name} installed onto the netpad from the app library`);
if (KEEP) console.log(`artefacts kept in ${work}`);
else fs.rmSync(work, { recursive: true, force: true });
