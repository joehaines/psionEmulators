// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// App-library runtime: manifest loading and "Try it on a device"
// delivery. The manifest + per-app zips are generated at deploy time by
// scripts/build-app-library.mts from applib/ (the 3-Lib collection).
//
// Delivery picks the mechanism the device actually had:
//   - EPOC32 with a CF slot   → the app's .SIS on a fresh (or the
//     currently-inserted) FAT16 card; the user opens it from drive D:.
//   - SIBO (Series 3 family)  → a FEFS flash SSD pack with the app's
//     files under \APP\; EPOC16 sees it as pack A/B.
//   - EPOC32 without CF (Revo) → Remote Link upload of the .SIS to C:\.
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
  installKind: 'sis' | 'sibo' | 'none';
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
export const TRY_CAPABLE: Record<string, 'cf' | 'ssd' | 'link'> = {
  series5:  'link', '5mx': 'link', '5mxpro': 'link', mc218: 'link',
  revo:     'link', osaris: 'link', series7: 'link', netbook: 'link',
  series3:  'ssd', series3a: 'ssd', series3c: 'ssd', series3mx: 'ssd',
  siena:    'ssd', workabout: 'ssd', workaboutmx: 'ssd',
  pocketbk: 'ssd', pocketbk2: 'ssd',
};

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
  ssdSlotCount?: number;
  remoteLinkUart?: number;
  linkProtocol?: number;
}

export function deliveryKindFor(profile: DeviceProfileLike, entry: AppEntry):
    'cf' | 'ssd' | 'link' | null {
  const linkOk = (profile.remoteLinkUart ?? -1) >= 0 && (profile.linkProtocol ?? 0) === 1;
  if (entry.installKind === 'sis') {
    // The user's delivery preference governs, then capability fallback
    // (a CF-less Revo still links under 'cf'; a link-less profile
    // still gets the card under 'link').
    if (getEpocDeliveryPref() === 'link' && linkOk) return 'link';
    if (profile.hasCFSlot) return 'cf';
    if (linkOk) return 'link';
  }
  if (entry.installKind === 'sibo' && (profile.ssdSlotCount ?? 0) > 0) return 'ssd';
  return null;
}

// Deliver a SIS by writing it into the CompactFlash card image (drive D:),
// host-side — no Remote Link transfer involved, so it has none of the cable's
// size/timing fragility. Used both as the user's chosen 'cf' delivery and as
// the automatic fallback when a large-app Remote Link upload stalls (see
// deliverApp's link path): the EPOC ROM's RemoteLinkServer can wedge partway
// through a sustained multi-hundred-KB upload, and the card path always works.
async function deliverViaCard(
  entry: AppEntry,
  files: Map<string, Uint8Array>,
  controls: EmulatorControls,
  onPhase?: (phase: string) => void,
): Promise<DeliveryResult> {
  if (!entry.installFile) throw new Error('No installer in this app bundle.');
  const sis = files.get(entry.installFile);
  if (!sis) throw new Error(`Installer ${entry.installFile} missing from bundle.`);
  const name = to83(entry.installFile);

  onPhase?.('Building CF card…');
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
    if (!retry.ok) throw new Error(`Could not add ${name} to the CF image: ${retry.reason}`);
  }
  onPhase?.('Inserting CF card…');
  const ok = await controls.attachCard(img);
  if (!ok) throw new Error('Could not attach the CF card.');
  return {
    summary: `${name} is on the CF card${reused ? '' : ' (a fresh card was inserted)'}.`,
    steps: [
      'On the device, open the System screen and switch to the D: drive (the CF card).',
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

  if (kind === 'cf') return deliverViaCard(entry, files, controls, onPhase);

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

  // kind === 'link' — EPOC32 machines: upload the .SIS over the Remote
  // Link cable straight into C:\Documents, the folder the System
  // screen already shows, so the installer is one tap away.
  if (!entry.installFile) throw new Error('No installer in this app bundle.');
  const sis = files.get(entry.installFile);
  if (!sis) throw new Error(`Installer ${entry.installFile} missing from bundle.`);
  const name = to83(entry.installFile);
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
    onPhase?.(`Uploading ${name}…`);
    onProgress?.(0, sis.length);
    lastAdvance = Date.now();
    stallTimer = setInterval(() => {
      if (Date.now() - lastAdvance > STALL_MS) stallCtl.abort();
    }, 2_000);
    await raceStall(client.uploadFile(`C:\\Documents\\${name}`, sis,
      bytes => { checkAborted(); lastAdvance = Date.now(); onProgress?.(bytes, sis.length); }));
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
  // The cable upload stalled/failed. If the device has a CF slot, deliver via
  // the card instead — that path never touches the wedge-prone link server.
  if (profile.hasCFSlot) {
    onPhase?.('Remote Link stalled — delivering via the CF card instead…');
    return await deliverViaCard(entry, files, controls, onPhase);
  }
  throw linkErr;
}
