// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// App-library tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/applib.test.mts
// or `npm run test:applib` from frontend/.
//
// Covers: the browser-side ZIP reader (stored + deflate entries,
// matching the writer in scripts/build-app-library.mts), the 8.3
// filename squeezer, delivery-kind routing, and full deliverApp runs
// against mock controls — verifying the produced FAT16 CF image and
// FEFS SSD pack actually contain the app.

import { deflateRawSync } from 'node:zlib';
import { listZipEntries, readZipEntry, unzipAll } from '../zip.ts';
import {
  to83, deliveryKindFor, cardNameFor, tryTargetsFor, deliverApp,
  isDriveRooted, epocAppFolder, epocDirPathFor,
  isAppsRoute, parseAppsRoute, appsRouteHash,
  type AppEntry, type DeviceProfileLike,
} from '../appLibrary.ts';
import { listDirectory } from '../fat16.ts';
import { listFiles } from '../fefs.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq<T>(actual: T, expected: T, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg}: got ${String(actual)}, want ${String(expected)}`);
    failures++;
  }
}

// ── Minimal in-test zip writer (mirrors the pipeline's) ─────────────

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32(data: Uint8Array): number {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < data.length; i++) c = CRC_TABLE[(c ^ data[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

function makeZip(entries: { name: string; data: Uint8Array; store?: boolean }[]): Uint8Array {
  const parts: Buffer[] = [];
  const central: Buffer[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, 'latin1');
    const deflated = deflateRawSync(e.data, { level: 9 });
    const useDeflate = !e.store && deflated.length < e.data.length;
    const payload = useDeflate ? deflated : Buffer.from(e.data);
    const method = useDeflate ? 8 : 0;
    const crc = crc32(e.data);
    const loc = Buffer.alloc(30);
    loc.writeUInt32LE(0x04034b50, 0); loc.writeUInt16LE(20, 4);
    loc.writeUInt16LE(method, 8);
    loc.writeUInt32LE(crc, 14); loc.writeUInt32LE(payload.length, 18);
    loc.writeUInt32LE(e.data.length, 22); loc.writeUInt16LE(name.length, 26);
    const cen = Buffer.alloc(46);
    cen.writeUInt32LE(0x02014b50, 0); cen.writeUInt16LE(20, 4); cen.writeUInt16LE(20, 6);
    cen.writeUInt16LE(method, 10);
    cen.writeUInt32LE(crc, 16); cen.writeUInt32LE(payload.length, 20);
    cen.writeUInt32LE(e.data.length, 24); cen.writeUInt16LE(name.length, 28);
    cen.writeUInt32LE(offset, 42);
    parts.push(loc, name, payload);
    central.push(cen, name);
    offset += loc.length + name.length + payload.length;
  }
  let centralSize = 0;
  for (const c of central) centralSize += c.length;
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(entries.length, 8); eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(centralSize, 12); eocd.writeUInt32LE(offset, 16);
  return new Uint8Array(Buffer.concat([...parts, ...central, eocd]));
}

const text = (s: string) => new TextEncoder().encode(s);

// ── zip.ts ──────────────────────────────────────────────────────────

await (async () => {
  // Compressible payload exercises the deflate path; tiny one stays stored.
  const big = text('the quick brown fox jumps over the lazy dog '.repeat(50));
  const zip = makeZip([
    { name: 'readme.txt', data: text('hi') },
    { name: 'sub/dir/app.sis', data: big },
    { name: 'stored.bin', data: text('x'), store: true },
  ]);
  const entries = listZipEntries(zip);
  eq(entries.length, 3, 'three entries listed');
  eq(entries[1].name, 'sub/dir/app.sis', 'nested path preserved');
  check(entries[1].method === 8, 'large entry deflated');
  check(entries[2].method === 0, 'tiny entry stored');

  const files = await unzipAll(zip);
  eq(new TextDecoder().decode(files.get('readme.txt')), 'hi', 'stored-ish entry round-trips');
  eq(files.get('sub/dir/app.sis')!.length, big.length, 'deflated entry inflates to full size');
  eq(new TextDecoder().decode(files.get('sub/dir/app.sis')!.subarray(0, 9)), 'the quick',
     'deflated content correct');

  const single = await readZipEntry(zip, entries[0]);
  eq(new TextDecoder().decode(single), 'hi', 'readZipEntry works standalone');
})();

// ── to83 ────────────────────────────────────────────────────────────

eq(to83('lostin3d.sis'), 'LOSTIN3D.SIS', 'plain 8.3 passes through uppercased');
eq(to83('sub/dir/My App v1.2!.sis'), 'MYAPPV12.SIS', 'path stripped, squeezed to 8.3');
eq(to83('verylongprogramname.opa'), 'VERYLONG.OPA', 'stem truncated to 8');
eq(to83('.hidden'), 'HIDDEN', 'dotfile becomes bare name');

// ── the library route ───────────────────────────────────────────────
// App.tsx parses it, AppLibrary writes it back as the user opens and
// closes app details, and the result is what people paste to each other.

eq(isAppsRoute('#/apps'), true, 'bare library route');
eq(isAppsRoute('#/apps?app=epocgames/monopoly'), true, 'library route with parameters');
eq(isAppsRoute('#/appsomething'), false, 'a longer hash is a different route');
eq(isAppsRoute('#/5mx'), false, 'a device route is not the library');

eq(appsRouteHash({ device: null, app: null }), '#/apps', 'no parameters → bare route');
eq(appsRouteHash({ device: 'netpad', app: null }), '#/apps?device=netpad', 'device only');
eq(appsRouteHash({ device: null, app: 'epocgames/monopoly' }),
   '#/apps?app=epocgames/monopoly', "the app id's slash stays readable");
eq(appsRouteHash({ device: 'netpad', app: 'netpad/word' }),
   '#/apps?device=netpad&app=netpad/word', 'both, device first');

{
  const empty = parseAppsRoute('#/apps');
  eq(empty.device, null, 'bare route names no device');
  eq(empty.app, null, 'bare route names no app');
  const both = parseAppsRoute('#/apps?device=netpad&app=netpad/word');
  eq(both.device, 'netpad', 'device parsed');
  eq(both.app, 'netpad/word', 'app parsed with its slash intact');
  // Both spellings of the slash have to parse — an older link, or one a
  // chat client has helpfully percent-encoded on the way through.
  eq(parseAppsRoute('#/apps?app=netpad%2Fword').app, 'netpad/word',
     'percent-encoded slash decodes');
  const round = '#/apps?device=netpad&app=netpad/word';
  eq(appsRouteHash(parseAppsRoute(round)), round, 'parse ∘ format is a round trip');
}

// ── delivery routing ────────────────────────────────────────────────

const sisApp: AppEntry = {
  id: 'epocgames/demo', name: 'Demo', category: 'epocgames', section: null,
  date: null, description: '', sizeBytes: 3, fileCount: 1,
  zip: 'files/epocgames/demo.zip', icon: null,
  devices: ['series5', '5mx', 'revo'], tryDevice: '5mx',
  installKind: 'sis', installFile: 'demo.sis', readmes: [], vault: false,
};
const siboApp: AppEntry = {
  ...sisApp, id: 's3games/demo', category: 's3games',
  devices: ['series3a', 'series3c'], tryDevice: 'series3c',
  installKind: 'sibo', installFile: 'DEMO.OPA',
};

eq(deliveryKindFor({ id: '5mx', hasCFSlot: true }, sisApp), 'cf',
   'SIS + CF slot, no link wired → cf fallback');
eq(deliveryKindFor({ id: '5mx', hasCFSlot: true, remoteLinkUart: 2, linkProtocol: 1 }, sisApp),
   'link', '5mx prefers the Remote Link (installer lands in Documents)');
eq(deliveryKindFor({ id: 'series7', hasCFSlot: true, remoteLinkUart: 3, linkProtocol: 1 }, sisApp),
   'link', 'Series 7 links by default too');
// The Settings preference flips link-capable devices to CF (a shimmed
// localStorage stands in for the browser's).
{
  const store = new Map<string, string>();
  (globalThis as Record<string, unknown>).localStorage = {
    getItem: (k: string) => store.get(k) ?? null,
    setItem: (k: string, v: string) => { store.set(k, v); },
  };
  store.set('psion-app-delivery', 'cf');
  eq(deliveryKindFor({ id: 'series7', hasCFSlot: true, remoteLinkUart: 3, linkProtocol: 1 }, sisApp),
     'cf', 'pref=cf: Series 7 takes the card');
  eq(deliveryKindFor({ id: 'revo', hasCFSlot: false, remoteLinkUart: 2, linkProtocol: 1 }, sisApp),
     'link', 'pref=cf: CF-less Revo still links');
  store.set('psion-app-delivery', 'link');
  eq(deliveryKindFor({ id: 'series7', hasCFSlot: true, remoteLinkUart: 3, linkProtocol: 1 }, sisApp),
     'link', 'pref=link: Series 7 links again');
  delete (globalThis as Record<string, unknown>).localStorage;
}
eq(deliveryKindFor({ id: 'revo', hasCFSlot: false, remoteLinkUart: 2, linkProtocol: 1 }, sisApp),
   'link', 'SIS + no CF + RFSV32 link → link');
eq(deliveryKindFor({ id: 'series3c', ssdSlotCount: 2 }, siboApp), 'ssd', 'SIBO + SSD slots → ssd');
eq(deliveryKindFor({ id: 'series3c', ssdSlotCount: 2 }, { ...siboApp, installKind: 'none' }),
   null, 'no installer → no delivery');
check(tryTargetsFor(sisApp).includes('5mx') && tryTargetsFor(sisApp).includes('revo'),
      'try targets include cf and link devices');
eq(tryTargetsFor({ ...sisApp, installKind: 'none' }).length, 0, 'no installer → no targets');

// ── netpad: MMC slot, card first ────────────────────────────────────
// The netpad's slot takes an MMC rather than a PC-Card (same raw FAT16
// image), and its EPOC R5 build only opens the Remote Link port once
// Remote link is switched on from the Tools menu — so the card wins
// even though the machine can link and the preference says otherwise.
const netpadApp: AppEntry = {
  ...sisApp, id: 'netpad/word', category: 'netpad',
  devices: ['netpad'], tryDevice: 'netpad', installFile: 'word.sis',
};
const netpadProfile: DeviceProfileLike = {
  id: 'netpad', hasCFSlot: false, hasMmcSlot: true,
  remoteLinkUart: 3, linkProtocol: 1,
};
eq(deliveryKindFor(netpadProfile, netpadApp), 'cf', 'netpad SIS takes the MMC card');
eq(cardNameFor(netpadProfile), 'MMC card', 'netpad names its slot an MMC');
eq(cardNameFor({ id: '5mx', hasCFSlot: true }), 'CF card', 'everyone else says CF');
{
  const store = new Map<string, string>([['psion-app-delivery', 'link']]);
  (globalThis as Record<string, unknown>).localStorage = {
    getItem: (k: string) => store.get(k) ?? null,
    setItem: (k: string, v: string) => { store.set(k, v); },
  };
  eq(deliveryKindFor(netpadProfile, netpadApp), 'cf',
     'pref=link does not divert the netpad onto its cable');
  delete (globalThis as Record<string, unknown>).localStorage;
}
check(tryTargetsFor(netpadApp).includes('netpad'), 'netpad is a try target for its own apps');

await (async () => {
  const sis = text('fake netpad sis');
  const zip = makeZip([{ name: 'word.sis', data: sis }]);
  let attached: Uint8Array | null = null;
  const phases: string[] = [];
  const controls = {
    cardAttached: false,
    getCardBytes: () => null,
    attachCard: async (img: Uint8Array) => { attached = img; return true; },
    ssdAttached: [] as boolean[],
  } as never;
  const result = await deliverApp(netpadApp, zip, controls, netpadProfile,
                                  p => phases.push(p));
  check(attached !== null, 'netpad MMC image attached');
  check(listDirectory(attached!, 0).some(e => e.name === 'WORD.SIS'),
        'WORD.SIS present in the MMC root directory');
  check(result.summary.includes('MMC card') && result.steps[0].includes('MMC card'),
        `netpad instructions say MMC (got "${result.summary}")`);
  check(phases.some(p => p.includes('MMC card')), 'progress phases say MMC too');
})();

// ── installed-folder ('epocdir') bundles ────────────────────────────
// Two shapes: a bare app folder, and a drive-rooted tree for apps that
// also need files outside \System\Apps (a shared library, say).
const folderApp: AppEntry = {
  ...sisApp, id: 'epocgames/demofolder',
  installKind: 'epocdir', installFile: 'DEMO.APP',
};
const rootedApp: AppEntry = {
  ...folderApp, id: 'epocgames/demorooted',
  installFile: 'System/Apps/Demo/DEMO.APP',
};

eq(isDriveRooted(folderApp.installFile!), false, 'flat bundle is not drive-rooted');
eq(isDriveRooted(rootedApp.installFile!), true, 'System/… bundle is drive-rooted');
eq(epocAppFolder(folderApp.installFile!), 'DEMO', 'flat bundle: folder named after the binary');
eq(epocAppFolder(rootedApp.installFile!), 'Demo', 'drive-rooted: folder taken from the path');
eq(epocDirPathFor(folderApp.installFile!, 'DEMO.APP'), '\\System\\Apps\\DEMO\\DEMO.APP',
   'flat bundle lands under \\System\\Apps\\<App>\\');
eq(epocDirPathFor(folderApp.installFile!, 'data/LEVELS.DAT'),
   '\\System\\Apps\\DEMO\\data\\LEVELS.DAT', 'flat bundle keeps its sub-directories');
eq(epocDirPathFor(rootedApp.installFile!, 'System/Libs/helper.dll'),
   '\\System\\Libs\\helper.dll', 'drive-rooted paths are used as given');

// The card path can only write 8.3 short names, which would rename a
// library the EPOC loader looks up by name — so a folder bundle is
// cable-only, and a device that can't link has no automated install.
eq(deliveryKindFor({ id: '5mx', hasCFSlot: true, remoteLinkUart: 2, linkProtocol: 1 }, folderApp),
   'link', 'installed folder goes over the link');
eq(deliveryKindFor({ id: '5mx', hasCFSlot: true }, folderApp), null,
   'installed folder never takes the card');
{
  const store = new Map<string, string>([['psion-app-delivery', 'cf']]);
  (globalThis as Record<string, unknown>).localStorage = {
    getItem: (k: string) => store.get(k) ?? null,
    setItem: (k: string, v: string) => { store.set(k, v); },
  };
  eq(deliveryKindFor({ id: '5mx', hasCFSlot: true, remoteLinkUart: 2, linkProtocol: 1 }, folderApp),
     'link', 'pref=cf does not divert an installed folder onto the card');
  delete (globalThis as Record<string, unknown>).localStorage;
}
check(tryTargetsFor(folderApp).includes('5mx'), 'installed folder still has try targets');

// ── deliverApp: CF path ─────────────────────────────────────────────

await (async () => {
  const sis = text('fake sis payload');
  const zip = makeZip([{ name: 'demo.sis', data: sis }, { name: 'readme.txt', data: text('r') }]);
  let attached: Uint8Array | null = null;
  const controls = {
    cardAttached: false,
    getCardBytes: () => null,
    attachCard: async (img: Uint8Array) => { attached = img; return true; },
    ssdAttached: [] as boolean[],
  } as never;
  const result = await deliverApp(sisApp, zip, controls, { id: '5mx', hasCFSlot: true });
  check(attached !== null, 'CF image attached');
  const entries = listDirectory(attached!, 0);
  const f = entries.find(e => e.name === 'DEMO.SIS');
  check(!!f, 'DEMO.SIS present in the CF root directory');
  eq(f?.size, sis.length, 'CF file has the SIS payload size');
  check(result.steps.length >= 2 && result.summary.includes('DEMO.SIS'),
        'CF instructions mention the file');
})();

// ── deliverApp: SSD path ────────────────────────────────────────────

await (async () => {
  const opa = text('OPLObjectFile!');
  const zip = makeZip([
    { name: 'DEMO.OPA', data: opa },
    { name: 'demo.txt', data: text('docs') },
    { name: 'Demo/LEVEL.DAT', data: text('lvl') },   // Jumpy-style subfolder
  ]);
  let slotUsed = -1;
  let pack: Uint8Array | null = null;
  const controls = {
    cardAttached: false,
    ssdAttached: [true, false],   // slot A busy → expect slot B
    attachSSD: async (slot: number, bytes: Uint8Array) => {
      slotUsed = slot; pack = bytes; return true;
    },
  } as never;
  const result = await deliverApp(siboApp, zip, controls, { id: 'series3c', ssdSlotCount: 2 });
  eq(slotUsed, 1, 'free slot B chosen over busy slot A');
  check(pack !== null, 'SSD pack attached');
  const names = listFiles(pack!).map(f => f.name);
  check(names.includes('APP\\DEMO.OPA'),
        `program in \\APP\\ where Psion+I looks (got ${names.join(', ')})`);
  check(names.includes('APP\\DEMO.TXT'),
        'flat data beside the program in \\APP\\');
  check(names.includes('APP\\DEMO\\DEMO.TXT'),
        'flat data under \\APP\\DEMO\\ (Blades-style load-dir base)');
  check(names.includes('DEMO\\DEMO.TXT'),
        'flat data mirrored at \\DEMO\\ (Ants-style drive-root base)');
  check(names.includes('APP\\DEMO\\LEVEL.DAT'),
        'subfolder data: zip root ≡ \\APP\\ (Jumpy-style)');
  check(names.includes('DEMO\\LEVEL.DAT'),
        'subfolder data mirrored at the drive root');
  check(result.summary.includes('B:') && result.summary.includes('\\APP\\'),
        'instructions name the right pack and folder');
})();

if (failures > 0) {
  console.error(`\n${failures} failure(s)`);
  process.exit(1);
}
console.log('applib.test.mts: all tests passed');
