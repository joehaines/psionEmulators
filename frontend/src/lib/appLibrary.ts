// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// App-library runtime: manifest loading and "Try it on a device"
// delivery. The manifest + per-app zips are generated at deploy time by
// scripts/build-app-library.mts from applib/ (the 3-Lib collection).
//
// Delivery picks the mechanism the device actually had:
//   - EPOC32 with a card slot → the app's .SIS on a fresh (or the
//     currently-inserted) FAT16 card; the user opens it from drive D:.
//     CompactFlash everywhere bar the netpad, whose slot takes an MMC —
//     the same raw image either way, and the netpad's default because
//     its cable only listens once Remote link is switched on.
//   - SIBO (Series 3 family)  → a FEFS flash SSD pack with the app's
//     files under \APP\; EPOC16 sees it as pack A/B.
//   - EPOC32 without CF (Revo) → Remote Link upload of the .SIS to C:\.
//   - EPOC32 bundles that are an installed app folder rather than an
//     installer ('epocdir') → a Remote Link copy straight into
//     \System\Apps\<App>\ on C:, no on-device installer involved.
//
// All builders work against EmulatorControls so they run identically in
// main-thread and worker mode.

import type { EmulatorControls } from '../hooks/useEmulator.ts';
import { createBlankImage, addFile } from './fat16.ts';
import { createFlashPack, addFileToPack, FLASH_PACK_SIZES } from './fefs.ts';
import { unzipAll } from './zip.ts';
import { PlpClient } from './plp/client-spec.ts';

// ── Manifest types (mirrors scripts/build-app-library.mts) ──────────

export interface AppCategory {
  id: string;
  label: string;
  platform: 'sibo' | 'epoc32' | 'other';
  vault: boolean;
}

export interface AppEntry {
  id: string;
  name: string;
  category: string;
  section: string | null;
  date: string | null;
  description: string;
  sizeBytes: number;
  fileCount: number;
  zip: string;
  icon: string | null;
  devices: string[];
  tryDevice: string | null;
  // 'sis'     — an EPOC32 installer to hand to the device.
  // 'sibo'     — EPOC16 program + data, delivered on an SSD pack.
  // 'epocdir'  — an already-installed EPOC32 app folder, copied into
  //              \System\Apps\<App>\ (installFile names the .app).
  installKind: 'sis' | 'sibo' | 'epocdir' | 'none';
  installFile: string | null;
  readmes: string[];
  vault: boolean;
}

export interface AppManifest {
  version: number;
  generatedAt: string;
  categories: AppCategory[];
  apps: AppEntry[];
}

// Guarded so the module also loads under the node test runner, where
// import.meta.env doesn't exist.
const APPS_BASE = `${(import.meta as { env?: { BASE_URL?: string } }).env?.BASE_URL ?? '/'}apps/`;

export function appAssetUrl(rel: string): string {
  return APPS_BASE + rel;
}

// Fetch the manifest. Throws a readable error when offline / the
// library isn't deployed — AppLibrary surfaces it as a friendly notice
// rather than a broken page.
export async function fetchAppManifest(): Promise<AppManifest> {
  let resp: Response;
  try {
    resp = await fetch(appAssetUrl('manifest.json'));
  } catch {
    throw new Error('offline');
  }
  if (!resp.ok) throw new Error(resp.status === 404 ? 'missing' : `http ${resp.status}`);
  return await resp.json() as AppManifest;
}

export async function fetchAppZip(entry: AppEntry): Promise<Uint8Array> {
  const resp = await fetch(appAssetUrl(entry.zip));
  if (!resp.ok) throw new Error(`download failed (HTTP ${resp.status})`);
  return new Uint8Array(await resp.arrayBuffer());
}

// ── The library route ───────────────────────────────────────────────
// `#/apps`, with two optional parameters:
//   device=<deviceId>       open filtered to that machine's apps (the
//                           netpad's "Install standard apps" button)
//   app=<category>/<slug>   open that app's details popup — the whole
//                           point being that the address bar always
//                           holds a link to whatever is on screen, so
//                           sharing one app is a copy and paste.
// The route lives here rather than in either component because App.tsx
// reads it and AppLibrary writes it; they have to agree on the shape.

export const APPS_ROUTE = '#/apps';

export interface AppsRoute {
  device: string | null;
  app: string | null;
}

// True for the library route with or without parameters.
export function isAppsRoute(hash: string): boolean {
  return hash === APPS_ROUTE || hash.startsWith(`${APPS_ROUTE}?`);
}

export function parseAppsRoute(hash: string): AppsRoute {
  const params = new URLSearchParams(
    hash.startsWith(`${APPS_ROUTE}?`) ? hash.slice(APPS_ROUTE.length + 1) : '');
  return { device: params.get('device'), app: params.get('app') };
}

export function appsRouteHash(route: AppsRoute): string {
  const params = new URLSearchParams();
  if (route.device) params.set('device', route.device);
  if (route.app) params.set('app', route.app);
  // App ids are "<category>/<slug>". A literal slash is legal in a query
  // string and URLSearchParams parses it back happily, so un-escape the
  // one URLSearchParams insists on writing — this URL is meant to be
  // read and pasted by people.
  const query = params.toString().replace(/%2F/g, '/');
  return query ? `${APPS_ROUTE}?${query}` : APPS_ROUTE;
}

// ── Pending "try" handoff ───────────────────────────────────────────
// AppLibrary writes the request, navigates to the device route, and
// App.tsx completes the delivery once the emulator reports 'running'.
// sessionStorage survives that route remount but not a new tab — which
// is the scoping we want.

const PENDING_KEY = 'psion.pendingTryApp';

export interface PendingTry { entry: AppEntry; deviceId: string; at: number }

export function setPendingTry(entry: AppEntry, deviceId: string): void {
  sessionStorage.setItem(PENDING_KEY, JSON.stringify({ entry, deviceId, at: Date.now() }));
}

export function takePendingTry(deviceId: string): AppEntry | null {
  const raw = sessionStorage.getItem(PENDING_KEY);
  if (!raw) return null;
  try {
    const p = JSON.parse(raw) as PendingTry;
    // Stale or for another device → drop it.
    if (p.deviceId !== deviceId || Date.now() - p.at > 5 * 60_000) {
      sessionStorage.removeItem(PENDING_KEY);
      return null;
    }
    sessionStorage.removeItem(PENDING_KEY);
    return p.entry;
  } catch {
    sessionStorage.removeItem(PENDING_KEY);
    return null;
  }
}

// ── Try-target capability ───────────────────────────────────────────
// Which delivery mechanism each emulated device supports, used by the
// library UI (which runs without the WASM module, so it can't consult
// live DeviceProfiles). The runtime delivery in App.tsx re-validates
// against the real profile. 5mx Pro and netBook are deliberately
// absent: their CF slot is consumed by the OS boot card.

// EPOC32 machines deliver over the Remote Link by default: the
// installer lands in C:\Documents — the folder the System screen
// already shows. A Settings option ("App delivery") switches the
// EPOC devices to CF-card delivery instead; see getEpocDeliveryPref.
// The netpad is the exception, and takes its card by default — see
// CARD_FIRST_DEVICES.
export const TRY_CAPABLE: Record<string, 'cf' | 'ssd' | 'link'> = {
  series5:  'link', '5mx': 'link', '5mxpro': 'link', mc218: 'link',
  revo:     'link', osaris: 'link', series7: 'link', netbook: 'link',
  netpad:   'cf',
  series3:  'ssd', series3a: 'ssd', series3c: 'ssd', series3mx: 'ssd',
  siena:    'ssd', workabout: 'ssd', workaboutmx: 'ssd',
  pocketbk: 'ssd', pocketbk2: 'ssd',
};

// Devices whose card beats their cable regardless of the delivery
// preference. Only the netpad: its EPOC R5 build opens the Remote Link
// port only once Remote link is switched on from the System screen's
// Tools menu, so a freshly booted machine has nothing listening — while
// its MMC slot mounts as D: on its own. (The card is an MMC rather than
// a PC-Card, but it is the same raw FAT16 image either way, so it takes
// the 'cf' path; only the wording differs.)
const CARD_FIRST_DEVICES = new Set(['netpad']);

// User preference for SIS delivery on EPOC32 machines (Settings →
// "App delivery"). Remote Link is the default; 'cf' switches devices
// with a CF slot to card delivery (CF-less ones like the Revo keep
// using the link regardless).
const DELIVERY_PREF_KEY = 'psion-app-delivery';

export function getEpocDeliveryPref(): 'link' | 'cf' {
  try { return localStorage.getItem(DELIVERY_PREF_KEY) === 'cf' ? 'cf' : 'link'; }
  catch { return 'link'; }
}

export function setEpocDeliveryPref(pref: 'link' | 'cf'): void {
  try { localStorage.setItem(DELIVERY_PREF_KEY, pref); } catch { /* ignore */ }
}

// The bootloader devices boot their OS from the CF slot, so a FRESH
// load can't double as an app target — but once the user has booted
// one (a saved session exists), the OS runs from RAM and the slot is
// plain D: storage. The library UI keeps these selectable but disabled
// until a saved session for them exists.
export const BOOTLOADER_TRY_DEVICES = new Set(['5mxpro', 'netbook']);

// Devices an app can actually be tried on: the manifest's device list
// intersected with the machines that support automated delivery.
export function tryTargetsFor(entry: AppEntry): string[] {
  if (entry.installKind === 'none') return [];
  return entry.devices.filter(d => TRY_CAPABLE[d] !== undefined);
}

// ── Filename helpers ────────────────────────────────────────────────

// Squeeze a filename into FAT/FEFS 8.3: uppercase, alphanumeric (plus
// underscore), name ≤ 8 / ext ≤ 3.
export function to83(name: string): string {
  const base = name.split('/').pop() ?? name;
  const dot = base.lastIndexOf('.');
  let stem = (dot > 0 ? base.slice(0, dot) : base).replace(/[^A-Za-z0-9_]/g, '').toUpperCase();
  let ext  = (dot > 0 ? base.slice(dot + 1) : '').replace(/[^A-Za-z0-9_]/g, '').toUpperCase();
  if (stem.length === 0) stem = 'APP';
  return ext ? `${stem.slice(0, 8)}.${ext.slice(0, 3)}` : stem.slice(0, 8);
}

// Squeeze one path component into a FEFS directory name (8 chars).
function toDirComponent(part: string): string {
  const d = part.replace(/[^A-Za-z0-9_]/g, '').toUpperCase().slice(0, 8);
  return d || 'DIR';
}

// SIBO pack placements. Programs go in \\APP\\ — the only place the
// System screen's Psion+I Install dialog lists files. Data placement
// follows the bundles' own structure plus mirrors for the conventions
// the apps' readmes document:
//   - zips shipping a top-level App/ tree map onto the drive as-is
//     (App/X → \\APP\\X), with a drive-root mirror of the data;
//   - zips with subfolders treat the zip root as \\APP\\ (Jumpy ships
//     JUMPY.OPA + Jumpy/CLASSIC.PIC, documented as \\APP\\JUMPY\\), with
//     a drive-root mirror (apps that resolve from the root);
//   - flat-zip data sits beside the program in \\APP\\ AND in an
//     app-named folder at both bases (\\APP\\BLADES\\ per Blades'
//     readme, \\ANTS\\ per Ants').
// The pack is read-only and roomy, so the duplication just means every
// app finds its tree wherever it was written to look.
function siboDirsFor(appDir: string, rel: string): string[] {
  const parts = rel.split('/');
  const subs = parts.slice(0, -1).map(toDirComponent);
  const base = parts[parts.length - 1];
  if (subs[0] === 'APP') {
    const inApp = subs.join('\\');
    const atRoot = subs.slice(1).join('\\');
    return atRoot ? [inApp, atRoot] : [inApp];
  }
  if (subs.length === 0) {
    // Root-level programs are installable → straight into \\APP\\.
    if (/\.(opa|app)$/i.test(base)) return ['APP'];
    return ['APP', `APP\\${appDir}`, appDir];
  }
  // Subfolder files: zip root ≡ \\APP\\, plus the drive-root mirror.
  return [['APP', ...subs].join('\\'), subs.join('\\')];
}

// ── Installed-folder ('epocdir') placement ──────────────────────────
//
// These bundles are an app as it sits on a device rather than an
// installer, and come in two shapes:
//
//   drive-rooted — the paths start at the drive root ("System/Apps/Wall/
//     WALL.APP", "System/Libs/zExeLoader.dll"). Used when the app needs
//     files outside its own folder, e.g. a shared library in
//     \System\Libs. Every file lands at C:\<path>, as named.
//   app-folder   — just the contents of \System\Apps\<App>\, with the
//     folder itself implied by the .app binary's name.
//
// EPOC finds applications by scanning \System\Apps\<folder>\ on every
// drive, which is why either shape ends up installed simply by copying.

// True when the bundle's paths are drive-rooted (see above).
export function isDriveRooted(installFile: string): boolean {
  return /^system\//i.test(installFile);
}

// The app's folder under \System\Apps\ — the directory the .app sits in
// for a drive-rooted bundle, otherwise the .app's own name (that is the
// pairing EPOC's app architecture expects).
export function epocAppFolder(installFile: string): string {
  const parts = installFile.split('/');
  if (isDriveRooted(installFile) && parts.length >= 2) return parts[parts.length - 2];
  const base = parts[parts.length - 1];
  const dot = base.lastIndexOf('.');
  return dot > 0 ? base.slice(0, dot) : base;
}

// Where one bundle file lands on the device, as an EPOC path without
// the drive letter.
export function epocDirPathFor(installFile: string, rel: string): string {
  const backslashed = rel.split('/').join('\\');
  return isDriveRooted(installFile)
    ? `\\${backslashed}`
    : `\\System\\Apps\\${epocAppFolder(installFile)}\\${backslashed}`;
}

// ── Delivery ────────────────────────────────────────────────────────

export interface DeliveryResult {
  // Step-by-step instructions shown in the overlay once delivery is done.
  steps: string[];
  // Short summary line, e.g. "FOO.SIS is on the CF card (drive D:)".
  summary: string;
}

export interface DeviceProfileLike {
  id: string;
  hasCFSlot?: boolean;
  // The netpad's slot takes an MMC instead of a PC-Card. Same raw FAT16
  // image, same drive D: — so card delivery treats the two alike.
  hasMmcSlot?: boolean;
  ssdSlotCount?: number;
  remoteLinkUart?: number;
  linkProtocol?: number;
}

// The card noun for a device, for the delivery instructions.
export function cardNameFor(profile: DeviceProfileLike): string {
  return profile.hasMmcSlot ? 'MMC card' : 'CF card';
}

export function deliveryKindFor(profile: DeviceProfileLike, entry: AppEntry):
    'cf' | 'ssd' | 'link' | null {
  const linkOk = (profile.remoteLinkUart ?? -1) >= 0 && (profile.linkProtocol ?? 0) === 1;
  const hasCard = (profile.hasCFSlot ?? false) || (profile.hasMmcSlot ?? false);
  // An installed folder goes over the cable only. The card path would
  // have to write it through fat16.ts, which only emits 8.3 short names
  // — fine for the app's own WALL.APP, fatal for a shared library whose
  // exact name the EPOC loader looks up (zExeLoader.dll). Every EPOC32
  // machine the library offers as a try target can link, so nothing is
  // lost by not offering a card route here.
  if (entry.installKind === 'epocdir') return linkOk ? 'link' : null;
  if (entry.installKind === 'sis') {
    // A card-first device ignores the preference: its cable needs the
    // user to switch Remote link on first, the card doesn't.
    if (hasCard && CARD_FIRST_DEVICES.has(profile.id)) return 'cf';
    // Otherwise the user's delivery preference governs, then capability
    // fallback (a CF-less Revo still links under 'cf'; a link-less
    // profile still gets the card under 'link').
    if (getEpocDeliveryPref() === 'link' && linkOk) return 'link';
    if (hasCard) return 'cf';
    if (linkOk) return 'link';
  }
  if (entry.installKind === 'sibo' && (profile.ssdSlotCount ?? 0) > 0) return 'ssd';
  return null;
}

// Deliver a SIS by writing it into the card image (drive D:) host-side —
// no Remote Link transfer involved, so it has none of the cable's
// size/timing fragility. Used as the user's chosen 'cf' delivery, as the
// netpad's default (its cable needs Remote link switched on first), and as
// the automatic fallback when a large-app Remote Link upload stalls (see
// deliverApp's link path): the EPOC ROM's RemoteLinkServer can wedge partway
// through a sustained multi-hundred-KB upload, and the card path always works.
// `card` is the machine's noun for its slot — a CompactFlash on every device
// that has one bar the netpad, whose slot takes an MMC.
async function deliverViaCard(
  entry: AppEntry,
  files: Map<string, Uint8Array>,
  controls: EmulatorControls,
  card: string,
  onPhase?: (phase: string) => void,
): Promise<DeliveryResult> {
  if (!entry.installFile) throw new Error('No installer in this app bundle.');
  const sis = files.get(entry.installFile);
  if (!sis) throw new Error(`Installer ${entry.installFile} missing from bundle.`);
  const name = to83(entry.installFile);

  onPhase?.(`Building ${card}…`);
  // Reuse the inserted card when there is one (keeps the user's
  // files); otherwise create a fresh 16 MB image.
  let img: Uint8Array | null = null;
  if (controls.cardAttached) {
    const existing = await Promise.resolve(controls.getCardBytes());
    if (existing && existing.length > 0) img = existing.slice();
  }
  let reused = true;
  if (!img) { img = createBlankImage(16 * 1024 * 1024); reused = false; }
  const added = addFile(img, name, sis);
  if (!added.ok) {
    // Name collision or full card — fall back to a fresh image.
    img = createBlankImage(16 * 1024 * 1024);
    reused = false;
    const retry = addFile(img, name, sis);
    if (!retry.ok) throw new Error(`Could not add ${name} to the card image: ${retry.reason}`);
  }
  onPhase?.(`Inserting ${card}…`);
  const ok = await controls.attachCard(img);
  if (!ok) throw new Error(`Could not attach the ${card}.`);
  return {
    summary: `${name} is on the ${card}${reused ? '' : ' (a fresh card was inserted)'}.`,
    steps: [
      `On the device, open the System screen and switch to the D: drive (the ${card}).`,
      `Open ${name} — the installer runs on the device.`,
      'Confirm the install prompts; the app then appears in Extras.',
    ],
  };
}

export async function deliverApp(
  entry: AppEntry,
  zipBytes: Uint8Array,
  controls: EmulatorControls,
  profile: DeviceProfileLike,
  onPhase?: (phase: string) => void,
  // Byte-level progress for the Remote Link upload (the CF/SSD paths
  // are near-instant local builds, so only the link path reports it).
  onProgress?: (bytes: number, total: number) => void,
  // Aborting mid-delivery (the notice's Close button) rejects promptly
  // AND tears the Remote Link down cleanly — without the Disc_Pdu the
  // device keeps the half-open session alive and refuses the next
  // connection until its Remote Link is toggled off and on.
  signal?: AbortSignal,
): Promise<DeliveryResult> {
  const kind = deliveryKindFor(profile, entry);
  if (!kind) throw new Error('This app has no automated install path for this device.');
  const checkAborted = () => {
    if (signal?.aborted) throw new Error('cancelled');
  };
  checkAborted();
  onPhase?.('Unpacking app…');
  const files = await unzipAll(zipBytes);

  if (kind === 'cf') return deliverViaCard(entry, files, controls, cardNameFor(profile), onPhase);

  if (kind === 'ssd') {
    onPhase?.('Building SSD pack…');
    // Size the pack: smallest power-of-two that fits the payload with
    // headroom for FEFS record overhead AND the multi-base data
    // placement (up to three copies of a data file — see siboDirsFor).
    const total = entry.sizeBytes;
    const sizes = FLASH_PACK_SIZES.map(s => s.bytes);
    const want = Math.max(128 * 1024, total * 4.6 + 64 * 1024);
    const packSize = sizes.find(s => s >= want) ?? sizes[sizes.length - 1];
    let pack = createFlashPack(packSize, 'APPS');
    const appDir = toDirComponent(entry.name);
    const placed: string[] = [];
    for (const [rel, data] of files) {
      const name = to83(rel);
      for (const dir of siboDirsFor(appDir, rel)) {
        try {
          pack = addFileToPack(pack, name, data, dir);
          placed.push(`${dir}\\${name}`);
        } catch {
          // Pack full or bad name — keep going; the main program matters most.
        }
      }
    }
    const mainPath = entry.installFile
      ? `${siboDirsFor(appDir, entry.installFile)[0]}\\${to83(entry.installFile)}`
      : null;
    if (mainPath && !placed.includes(mainPath)) {
      throw new Error('The app did not fit on an SSD pack.');
    }

    // Prefer a free slot so we don't eject something the user attached.
    const slots = profile.ssdSlotCount ?? 1;
    let slot = 0;
    for (let i = 0; i < slots; i++) {
      if (!controls.ssdAttached[i]) { slot = i; break; }
      if (i === slots - 1) slot = 0;
    }
    onPhase?.('Inserting SSD pack…');
    const ok = await controls.attachSSD(slot, pack, 'flash');
    if (!ok) throw new Error('Could not attach the SSD pack.');
    const packLetter = String.fromCharCode(65 + slot); // A:, B:
    const mainName = entry.installFile ? to83(entry.installFile) : null;
    return {
      summary: `The app is on SSD pack ${packLetter}: in \\APP\\.`,
      steps: [
        `On the System screen press Psion+I (Install), set Disk to ${packLetter} and pick ${mainName ?? 'the program'}.`,
        'The app appears on the System screen — move the cursor to it and press Enter.',
        'Its data files are laid out where the app expects them.',
        'If the device looks frozen, press Reset in the control bar — it reboots fresh with the pack still inserted.',
      ],
    };
  }

  // kind === 'link' — EPOC32 machines: upload over the Remote Link
  // cable. A .SIS goes straight into C:\Documents, the folder the
  // System screen already shows, so the installer is one tap away; an
  // installed folder ('epocdir') is copied onto C: where it belongs —
  // \System\Apps\<App>\, plus anything else the bundle carries.
  if (!entry.installFile) throw new Error('No installer in this app bundle.');
  const sis = files.get(entry.installFile);
  if (!sis) throw new Error(`Installer ${entry.installFile} missing from bundle.`);
  const asFolder = entry.installKind === 'epocdir';
  const folder = asFolder ? epocAppFolder(entry.installFile) : '';
  const name = to83(entry.installFile);
  // Bytes to move: the one installer, or every file in the folder.
  const totalBytes = asFolder
    ? [...files.values()].reduce((s, f) => s + f.length, 0)
    : sis.length;
  const uart = profile.remoteLinkUart ?? -1;

  onPhase?.('Connecting Remote Link…');
  checkAborted();
  // ER3/ER4 CL-PS711x ROMs (Series 5, Osaris) need the 0x22 Req_Con
  // flavour and a passive handshake — same rule as RemoteLinkDialog.
  const conSeq = (profile.id === 'series5' || profile.id === 'osaris') ? 2 : 4;
  // R5 active-handshake devices (Windermere / SA-1100) get the
  // parked-session flow: the cable stays attached between deliveries
  // and the device-side session is resumed via the adoption probe —
  // the R5 link server never survives a Disc or cable unplug (it goes
  // dormant until Remote Link is toggled on the device). The passive
  // CL-PS711x devices keep the legacy teardown: their ROMs recover via
  // the per-connect re-plug and wedge on unsolicited host frames.
  const parkable = conSeq !== 2;
  if (parkable) {
    // Never detach (a detach = cable unplug = the link server goes
    // dormant). Just ensure the bridge is attached — a restored device
    // may already show attached with a parked session, which connect()'s
    // adoption probe resumes.
    if (!controls.serialIsAttached(uart) && !controls.serialAttachHost(uart)) {
      throw new Error('Could not attach the serial cable.');
    }
  } else {
    // Passive CL-PS711x: an already-attached port is held by another
    // dialog (Modem / Printer); they re-plug per connect, so we don't
    // stomp it.
    if (controls.serialIsAttached(uart)) {
      throw new Error('The serial port is busy — close the Remote Link / Modem / Printer dialog first.');
    }
    if (!controls.serialAttachHost(uart)) {
      throw new Error('Could not attach the serial cable.');
    }
  }
  const client = new PlpClient(
    () => controls.serialReadBytes(uart),
    data => controls.serialWriteBytes(uart, data),
    // SA-1100 (Series 7 / netBook, UART3) has no host RX FIFO cap, so it
    // takes a 2 KB chunk where Windermere is held to 1 KB by its 4 KB
    // FIFO — halving the round-trips roughly halves SA-1100 upload time.
    { conSeq, chunkSize: uart === 3 ? 2048 : undefined },
  );
  client.start();
  // Rejects the moment the signal aborts, so a Close mid-connect (or a
  // mid-upload stall) falls straight through to the finally below.
  const aborted = signal
    ? new Promise<never>((_, reject) => {
        const fire = () => reject(new Error('cancelled'));
        if (signal.aborted) fire();
        else signal.addEventListener('abort', fire, { once: true });
      })
    : null;
  const race = <T,>(p: Promise<T>): Promise<T> => {
    if (!aborted) return p;
    p.catch(() => { /* settles after an abort won the race */ });
    return Promise.race([p, aborted]);
  };
  // No-progress watchdog. A large-app upload can stall mid-stream when the
  // device's RemoteLinkServer wedges (a ROM-side fragility on sustained
  // transfers — not disk space, the seq wrap, or dropped frames, all ruled
  // out). Rather than wait out the full RFSV timeout chain, abort the cable
  // attempt ~20 s after progress stops so we can fall back to the CF card.
  let linkErr: unknown = null;
  let lastAdvance = Date.now();
  let stallTimer: ReturnType<typeof setInterval> | null = null;
  const stallCtl = new AbortController();
  const STALL_MS = 20_000;
  const stalled = new Promise<never>((_, reject) => {
    stallCtl.signal.addEventListener('abort',
      () => reject(new Error('Remote Link upload stalled')), { once: true });
  });
  const raceStall = <T,>(p: Promise<T>): Promise<T> => {
    p.catch(() => { /* settles after the stall/abort won the race */ });
    return Promise.race([race(p), stalled]);
  };
  try {
    await race(client.connect(60_000));
    onPhase?.(asFolder ? `Copying ${entry.name} to \\System\\Apps\\${folder}…` : `Uploading ${name}…`);
    onProgress?.(0, totalBytes);
    lastAdvance = Date.now();
    stallTimer = setInterval(() => {
      if (Date.now() - lastAdvance > STALL_MS) stallCtl.abort();
    }, 2_000);
    const advance = (bytes: number) => {
      checkAborted(); lastAdvance = Date.now(); onProgress?.(bytes, totalBytes);
    };
    if (asFolder) {
      // One MkDirAll per directory the bundle touches — it creates every
      // missing parent, and is a no-op where the directory already
      // exists (a re-delivery just overwrites the files in place).
      const made = new Set<string>();
      let done = 0;
      for (const [rel, data] of files) {
        const target = epocDirPathFor(entry.installFile, rel);
        const dir = target.slice(0, target.lastIndexOf('\\') + 1);
        if (!made.has(dir)) {
          await raceStall(client.makeDirAll(`C:${dir}`));
          made.add(dir);
          advance(done);
        }
        const base = done;
        await raceStall(client.uploadFile(
          `C:${target}`, data, bytes => advance(base + bytes)));
        done += data.length;
        advance(done);
      }
    } else {
      await raceStall(client.uploadFile(`C:\\Documents\\${name}`, sis, advance));
    }
  } catch (e) {
    linkErr = e;
  } finally {
    if (stallTimer) clearInterval(stallTimer);
    if (parkable) {
      // Park the session: stop the host side WITHOUT Disc or detach,
      // after letting the device's last in-flight reply get acked.
      // The next delivery (or the Remote Link dialog) resumes it via
      // the adoption probe.
      try { await client.releaseQuiesced(); } catch { /* best effort */ }
    } else {
      try { client.disconnect(); } catch { /* already down */ }
      if (controls.serialIsAttached(uart)) controls.serialDetachHost(uart);
    }
  }
  if (!linkErr) {
    if (asFolder) {
      return {
        summary: `${entry.name} is installed in C:\\System\\Apps\\${folder}.`,
        steps: [
          `${entry.name} appears in Extras — tap it to run it.`,
          'If it is not listed yet, press Reset in the control bar: EPOC rebuilds its app list at boot.',
          'The files sit on the internal disk, so they survive a reset.',
        ],
      };
    }
    return {
      summary: `${name} is in the Documents folder.`,
      steps: [
        `On the device, open ${name} from the Documents folder on the System screen — the installer runs.`,
        'Confirm the install prompts; the app then appears in Extras.',
      ],
    };
  }
  // A user cancel surfaces as-is — don't fall back.
  if (signal?.aborted) throw linkErr;
  // The cable upload stalled/failed. If the device has a card slot, deliver
  // via the card instead — that path never touches the wedge-prone link
  // server. Not open to installed folders: the card can only carry 8.3 names
  // (see deliveryKindFor), so a "rescue" there would install a broken copy.
  if ((profile.hasCFSlot || profile.hasMmcSlot) && !asFolder) {
    const card = cardNameFor(profile);
    onPhase?.(`Remote Link stalled — delivering via the ${card} instead…`);
    return await deliverViaCard(entry, files, controls, card, onPhase);
  }
  throw linkErr;
}
