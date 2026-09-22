// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useRef, useState, useEffect, useCallback } from 'react';
import type { PsionModule, DeviceInfo, DeviceProfile } from '../types/emulator';
import {
  loadPsionModule,
  reloadPsionModule,
  serialReadBytes as serialReadBytesWasm,
  serialWriteBytes as serialWriteBytesWasm,
} from '../lib/wasmBridge';
import { browserKeyToEpocChord, charToEpocChord, keyboardLayoutForDevice, isHostTextEntry } from '../lib/keymap';
import { isFat16, createBlankImage, addFile, FAT_ATTR_HIDDEN, FAT_ATTR_SYSTEM } from '../lib/fat16';
import type { PackKind } from '../lib/fefs';
import { createAudioEngine, primeAudioContext, primeMobileAudioSession, type AudioEngine } from '../lib/audioEngine';
import { trackDeviceLoad, trackFeature, startSession, endSession } from '../lib/analytics';
import {
  encodeBundle,
  decodeBundle,
  compressChunk,
  decompressChunk,
  type BundleDeviceInput,
} from '../lib/stateBundle';
import { quiesceActiveSessions } from '../lib/plp/client-spec';

export type EmulatorState = 'idle' | 'loading-wasm' | 'ready' | 'loading-rom' | 'running' | 'error';

// Device-mode pixel LUTs: map luminance [0..255] to dark-grey + alpha so
// unlit pixels read as 2%-opaque dark grey and driven pixels stay opaque black.
const DEVICE_GREY_LUT  = new Uint8Array(256);
const DEVICE_ALPHA_LUT = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
  DEVICE_GREY_LUT[i]  = Math.round(0x33 * i / 255);
  DEVICE_ALPHA_LUT[i] = Math.round(0xFF - (0xFF - 0x05) * i / 255);
}

function applyDeviceModePixels(buf: Uint8ClampedArray) {
  for (let i = 0; i < buf.length; i += 4) {
    const lum = (buf[i] * 77 + buf[i + 1] * 150 + buf[i + 2] * 29) >> 8;
    const grey = DEVICE_GREY_LUT[lum];
    buf[i]     = grey;
    buf[i + 1] = grey;
    buf[i + 2] = grey;
    buf[i + 3] = DEVICE_ALPHA_LUT[lum];
  }
}

const LAST_DEVICE_KEY = 'psion-last-device';

// ── EPOC Unique id (debug feature) ─────────────────────────────────────
// An id the user programmed via the debug panel is remembered per device
// and re-applied on every load and reset, because both paths rebuild the
// emulator from ROM (loadBufferedROM) and would otherwise hand back the
// stock value. Stored as the full 64-bit id — the low 32 bits are the
// identity chip's word, the high 32 the model UID patched into the ROM.
// localStorage rather than IndexedDB: it's eight bytes, and the worker
// hook needs to read it synchronously on the main thread to pass into the
// worker's load. Exported so useEmulatorWorker shares the same keys and
// parsing.
const machineIdKey = (id: string) => `psion-machine-id-${id}`;
// The stored override, as a bigint so all 64 bits survive the round trip
// (a JS number can't hold the full id exactly). Null when unset.
export function loadStoredMachineId(deviceId: string): bigint | null {
  try {
    const raw = localStorage.getItem(machineIdKey(deviceId));
    if (raw == null) return null;
    const v = BigInt('0x' + raw.replace(/[^0-9a-fA-F]/g, ''));
    return v >= 0n ? v & 0xFFFFFFFFFFFFFFFFn : null;
  } catch { return null; }
}
export function storeMachineId(deviceId: string, id: bigint | null): void {
  try {
    if (id == null) localStorage.removeItem(machineIdKey(deviceId));
    else localStorage.setItem(machineIdKey(deviceId), id.toString(16).padStart(16, '0'));
  } catch { /* private-mode / quota — the id still applies to this session */ }
}
// What the UI needs to know: whether this device has a readable identity
// chip, the low half it currently holds, the model UID EPOC prints in
// front of it (null when unknown for this machine), and whether that high
// half can be patched. Same shape as the worker's machineId RPC reply.
export interface MachineIdState {
  supported: boolean;
  id: number | null;
  prefix: number | null;
  prefixSettable: boolean;
}
// Writes `override` (a full 64-bit id) into the machine and reports what
// stuck. The bindings landed together, so requiring the full set doubles as
// the "is this psion.wasm new enough?" check — an older build reports
// unsupported and the UI hides the control.
export function applyMachineId(mod: PsionModule, override: bigint | null): MachineIdState {
  if (typeof mod.hasMachineId !== 'function'
      || typeof mod.getMachineId !== 'function'
      || typeof mod.setMachineId !== 'function'
      || typeof mod.getMachineIdPrefix !== 'function'
      || typeof mod.canSetMachineIdPrefix !== 'function'
      || typeof mod.setMachineIdPrefix !== 'function'
      || !mod.hasMachineId()) {
    return { supported: false, id: null, prefix: null, prefixSettable: false };
  }
  if (override != null) {
    mod.setMachineId(Number(override & 0xFFFFFFFFn) >>> 0);
    // The high half is a ROM constant, so it can only be written where the
    // emulator managed to locate it; elsewhere it stays as the ROM has it
    // and the read-back below shows the user what they actually got.
    const wantPrefix = Number((override >> 32n) & 0xFFFFFFFFn) >>> 0;
    if (wantPrefix !== 0 && mod.canSetMachineIdPrefix()) mod.setMachineIdPrefix(wantPrefix);
  }
  const prefix = mod.getMachineIdPrefix() >>> 0;
  return {
    supported: true,
    id: mod.getMachineId() >>> 0,
    prefix: prefix !== 0 ? prefix : null,
    prefixSettable: mod.canSetMachineIdPrefix(),
  };
}

// ── ROM language variant ──
// Which language a multilingual ROM boots into — an index into the
// device's own list, stored per device the way the Unique id is, and
// applied before the first cycle on every load and reset. localStorage
// rather than IndexedDB for the same reason: the worker hook has to read
// it synchronously on the main thread to pass into the worker's load.
const languageKey = (id: string) => `psion-language-${id}`;
export function loadStoredLanguage(deviceId: string): number | null {
  try {
    const raw = localStorage.getItem(languageKey(deviceId));
    if (raw == null) return null;
    const v = Number.parseInt(raw, 10);
    return Number.isInteger(v) && v >= 0 ? v : null;
  } catch { return null; }
}
export function storeLanguage(deviceId: string, index: number | null): void {
  try {
    if (index == null) localStorage.removeItem(languageKey(deviceId));
    else localStorage.setItem(languageKey(deviceId), String(index));
  } catch { /* private-mode / quota — the choice still applies to this session */ }
}
// What the UI needs: the names this ROM offers (empty on the single-
// language machines, which is what hides the control) and which one the
// machine is currently set to boot into.
export interface LanguageState {
  names: string[];
  index: number;
}
// Selects `override` on the machine and reports what stuck. Called from
// the load path before the device starts stepping, so the guest's
// boot-time read of its settings PROM sees the user's choice. The
// bindings landed together, so requiring the full set doubles as the "is
// this psion.wasm new enough?" check.
export function applyLanguage(mod: PsionModule, override: number | null): LanguageState {
  if (typeof mod.getLanguageCount !== 'function'
      || typeof mod.getLanguageName !== 'function'
      || typeof mod.getLanguage !== 'function'
      || typeof mod.setLanguage !== 'function') {
    return { names: [], index: 0 };
  }
  const count = mod.getLanguageCount() | 0;
  if (count < 2) return { names: [], index: 0 };
  if (override != null) mod.setLanguage(override);
  const names: string[] = [];
  for (let i = 0; i < count; i++) names.push(mod.getLanguageName(i));
  return { names, index: mod.getLanguage() | 0 };
}

const idbStateKey = (id: string) => `state-${id}`;
const idbCardKey  = (id: string) => `cf-${id}`;
// Per-(device, slot) SSD image key. Mirrors the CF card persistence
// pattern but slot-indexed; SIBO devices have up to 2 slots.
const idbSsdKey   = (id: string, slot: number) => `ssd-${id}-${slot}`;
// Pack type the slot was attached with (see PackKind). Stored apart
// from the image bytes because the type models a hardware strap on the
// pack PCB, not a property of the contents: the same FEFS image reads
// as a read/write Flash drive or a read-only "Protected" one depending
// on how it is strapped.
const idbSsdTypeKey = (id: string, slot: number) => `ssd-type-${id}-${slot}`;
// Maps a UI pack kind to the wasm attachSSDImage type code (mirrors
// PsionSSD::Type): 'ram' = 1, 'flash' = 2 (Type 1 Flash, read/write),
// 'protected' = 3 (hardware write-protected, a factory system disk).
// With no stored/explicit kind, sniff the FEFS magic.
function ssdTypeCode(bytes: Uint8Array, kind: PackKind | undefined): number {
  const k = kind ??
    (bytes.length >= 2 && bytes[0] === 0xA5 && bytes[1] === 0xF1 ? 'flash' : 'ram');
  return k === 'protected' ? 3 : k === 'flash' ? 2 : 1;
}
// Factory default SSDs: devices that physically shipped with a pack inserted.
// The MC400 and MC200 both came with a ROM:: System Disk in Pack D (slot 3) —
// the window server, shell, OPL and fonts that populate the lower app bar.
// Returns the bundled image URL + pack kind, or null when the slot has no
// factory default.
function defaultSsdFor(deviceId: string, slot: number): { url: string; kind: PackKind } | null {
  if (deviceId === 'mc200' && slot === 3) {
    // Same Pack D arrangement as the MC400 — the MC200's own
    // factory System Disk, dumped.
    return { url: `${import.meta.env.BASE_URL}roms/MC200_V2.12F_system.ssd`, kind: 'protected' };
  }
  if (deviceId === 'mc400' && slot === 3) {
    // Strapped write-protected, like the real ROM:: System Disk.
    return { url: `${import.meta.env.BASE_URL}roms/MC400_V2.60F_system.ssd`, kind: 'protected' };
  }
  return null;
}
async function fetchDefaultSsd(url: string): Promise<Uint8Array | null> {
  try {
    const resp = await fetch(url);
    if (!resp.ok) return null;
    return new Uint8Array(await resp.arrayBuffer());
  } catch {
    return null;
  }
}
const idbDatapakKey = (id: string, slot: number) => `datapak-${id}-${slot}`;
const IDB_NAME  = 'psion-emu';
const IDB_STORE = 'state';
// Bumped whenever the C++ emulator struct layout changes in a way that
// makes a heap snapshot from an older build incompatible with the new
// build. Restoring a mismatched snapshot silently corrupts fields (e.g.
// touchX/touchY/penDown), so we invalidate on mismatch and cold-boot.
// v2 = the 5MX codec fields were added to Windermere::Emulator (audio
//      emulation branch), growing the struct and shifting maps.
// v3 = 5mx Pro switched from a patched 10.8 MB OS-in-ROM to the 128 KB
//      bootloader at ROM[0] with the OS loaded from CF into DRAM. Old
//      snapshots have the OS bytes in the wrong region for the new build.
// v4 = Series 3a boot-status fix: removed the MainsPresent bit (0x0020)
//      from the ASIC9 reset value so the v3.40f EPOC16 shell shows the
//      System screen instead of triggering a "Media is corrupt" dialog.
//      Old snapshots carry the wrong m_boot_status in the heap and would
//      re-enter the session-restore path on the next cold boot.
// v5 = Stored value shape gained `deviceId` so the restore path no
//      longer relies on a fragile `getDeviceName() == profile.displayName`
//      string match (the C++ side uses inconsistent names for several
//      devices — "Series 5mx" vs profile "Psion Series 5mx", "MC218"
//      vs "Ericsson MC218", default "Psion Series 3" for the Acorn
//      Pocket Book). The C++ heap layout itself is unchanged from v4,
//      so v4 saves restore correctly on a v5 build — see
//      RESTORE_COMPATIBLE_VERSIONS below.
// v6 = ARM710 TLB struct grew: TlbEntry gained permCache + hostPtr
//      fields (16 → 28 bytes on wasm32), flat tlb[64] became
//      set-associative tlb[16][4], nextTlbIndex (4 B) replaced by
//      tlbNextWay[16] (64 B), and legacySmull / enableTlbExtensions
//      bools were added.  Total ARM710 growth ~800+ bytes; every
//      field after the TLB in Windermere / CLPS / SA-1100 shifts.
// v7 = ARM710 `legacySmull` bool removed after the execMultiplyLong
//      signed/unsigned decode was corrected (the flag papered over an
//      inverted selector and is no longer needed).  The struct shrinks
//      by one bool, so every field after it shifts — v6 saves are not
//      layout-compatible and cold-boot instead.
// v8 = SA-1100 interpreter throughput work: the SA1100Bridge fast-TLB
//      entry gained a per-sub-page permission cache (permCachePg[4]), so
//      sizeof(FastTlbEntry) — and every field after the fast TLB in the
//      SA-1100 emulator struct — shifts; and ARM710 gained a virtual
//      (onMmuPermConfigChanged), which reorders the vtable that heap
//      snapshots capture as indirect-function-table indices. Both make a
//      v7 heap incompatible with a v8 build, so old saves cold-boot.
// v11 = SIBO emulation overhaul (this branch): PsionCondor rewritten
//      (word-spaced register file; new m_threPending / m_msrDelta /
//      m_plain16550Irq / interrupt-block fields), PsionAsic9's
//      interrupt status + mask widened uint8 -> uint16 (ASIC9MX UART
//      bits 8/9), Series3c::Emulator gained a second PsionCondor
//      member (m_mxUart0) + host-RX staging deque and lost
//      m_nextMxUartIntAt, and the V30 core gained intInhibit (the
//      MOV SS / POP SS / STI interrupt shadow). Every field after
//      those shifts on all SIBO devices, so v10 heaps restore into
//      garbage object state (observed: a restored 3c wedges hard
//      once the Remote Link touches the relocated Condor) — cold-boot
//      them instead.
// v12 = Geofox One gained its mouse pad: Geofox::Emulator carries the
//       pad's goal queue and the shadow of the pointer the guest's own
//       driver holds (core/geofox.h), so the object is larger than the
//       one a v11 heap was saved with. A restore overwrites the whole
//       heap, allocator state included, so the new build would read and
//       write the pad's fields over whatever the old heap put after that
//       object — cold-boot instead. Only the Geofox's own layout moved
//       (nothing else changed size or vtable shape), but the stored
//       schema is global, so every device re-cold-boots once.
const STATE_SCHEMA_VERSION = 12;

// Schema versions whose IDB-stored heap blob is still bit-compatible
// with the current C++ build's struct layout. Used by the restore /
// import paths so an older save can still load — earlier versions
// either changed C++ struct layout (v1-v3) or pre-date the heap-shape
// stored format. The restore path's LCD-bounds check is the final
// guard if a heap turns out to be incompatible despite being in this
// set: it falls back to a clean cold boot rather than corrupting the
// running emulator.
const RESTORE_COMPATIBLE_VERSIONS = new Set([12]);

// Audio enable preferences are now GLOBAL across devices — moved up to
// the header in b8900a16 — so persist them in localStorage and retain
// them across device switches and page reloads. The actual engine
// activation still happens per-device via the running-device effects
// below; this just keeps the user's preference between sessions.
const SPEAKER_PREF_KEY = 'psion-speaker-enabled';
const MIC_PREF_KEY     = 'psion-mic-enabled';
// Use sessionStorage rather than localStorage: persistence carries the
// user's audio toggle state across DEVICE SWITCHES within a single tab
// session (which is the requested behaviour from earlier in this
// session), but a FRESH page reload starts with audio OFF. Reasons:
//   * Auto-resuming speaker on reload meant the browser emitted audio
//     before the user clicked the speaker button (surprising).
//   * Auto-resuming mic on reload meant getUserMedia fired when the
//     audio engine was instantiated — typically right after the user
//     clicked the SPEAKER toggle — so the mic-permission dialog
//     appeared and the user perceived it as "the speaker button asked
//     for mic access".
function loadAudioPref(key: string): boolean {
  if (typeof window === 'undefined') return false;
  try { return window.sessionStorage.getItem(key) === '1'; }
  catch { return false; }
}
function saveAudioPref(key: string, value: boolean): void {
  if (typeof window === 'undefined') return;
  try { window.sessionStorage.setItem(key, value ? '1' : '0'); }
  catch { /* private-mode storage rejection — best-effort */ }
}

// Simulated-time preroll (frames at 64 fps) applied on cold boot before
// the first render tick and before the initial IDB snapshot is saved.
// Lets devices with a long blank initialisation window fast-forward to
// the point where the System screen is visible, so the saved state and
// every subsequent load start with content on screen.
// Series 3a v3.40f: blank LCD for ~7.8 s sim-time before System screen.
// netBook v0.11 bootloader: ~2.5 s sim-time before the splash painter
// runs (LCD ENA flips on at ~2.4 s, splash bitmap paints right after).
// The framebuffer reads as garbage until then — running 3 sim seconds
// of preroll lets the first rendered frame show the finished splash.
// Windermere EPOC5 devices (5mx, mc218, series5, revo, osaris) blank for
// ~5-6 s sim-time before the System screen paints — 7 s of preroll gets
// past it comfortably.  Especially important for the chromeless embed
// route (#/embed/<id>) where there's no header / control bar to signal
// "the device is alive, just booting."
const COLD_BOOT_PREROLL: Readonly<Record<string, number>> = {
  'series3a': 10 * 64,  // 10 simulated seconds → past the ~7.8 s blank window
  'netbook':   3 * 64,  //  3 simulated seconds → past the splash-paint cycle
  'mc218':     7 * 64,  //  7 simulated seconds → past the ~5 s EPOC5 blank window
  '5mx':       7 * 64,
  '5mxpro':    7 * 64,
  'series5':   7 * 64,
  'revo':      7 * 64,
  'conan':     7 * 64,
  'conanv001': 7 * 64,
  'osaris':    7 * 64,
};

const yieldToUI = () => new Promise<void>(r => setTimeout(r, 0));

// Megabytes with one decimal, for the download phase line ("2.1 of 8.4 MB").
const fmtMB = (bytes: number) => (bytes / 1048576).toFixed(1);

// ── IndexedDB helpers ────────────────────────────────────────────────────────

function openIDB(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(IDB_NAME, 1);
    req.onupgradeneeded = e =>
      (e.target as IDBOpenDBRequest).result.createObjectStore(IDB_STORE);
    req.onsuccess = e => resolve((e.target as IDBOpenDBRequest).result);
    req.onerror = () => reject(req.error);
  });
}

async function idbPut(key: string, value: unknown): Promise<void> {
  const db = await openIDB();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE, 'readwrite');
    tx.objectStore(IDB_STORE).put(value, key);
    tx.oncomplete = () => resolve();
    tx.onerror   = () => reject(tx.error);
  });
}

async function idbGet<T>(key: string): Promise<T | null> {
  try {
    const db = await openIDB();
    return new Promise((resolve, reject) => {
      const tx  = db.transaction(IDB_STORE, 'readonly');
      const req = tx.objectStore(IDB_STORE).get(key);
      req.onsuccess = () => resolve((req.result as T | undefined) ?? null);
      req.onerror   = () => reject(req.error);
    });
  } catch {
    return null;
  }
}

async function idbDelete(key: string): Promise<void> {
  try {
    const db = await openIDB();
    return new Promise((resolve, reject) => {
      const tx = db.transaction(IDB_STORE, 'readwrite');
      tx.objectStore(IDB_STORE).delete(key);
      tx.oncomplete = () => resolve();
      tx.onerror   = () => reject(tx.error);
    });
  } catch {}
}

async function idbGetAllKeys(): Promise<string[]> {
  try {
    const db = await openIDB();
    return new Promise((resolve, reject) => {
      const tx  = db.transaction(IDB_STORE, 'readonly');
      const req = tx.objectStore(IDB_STORE).getAllKeys();
      req.onsuccess = () => resolve(req.result as string[]);
      req.onerror   = () => reject(req.error);
    });
  } catch {
    return [];
  }
}

export function triggerDownload(blob: Blob, filename: string): void {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// gzip compression for the IDB-stored heap blob. Centralised in
// stateBundle.ts so the export/import path and the per-device save
// path all agree on the wire format (gzipped Uint8Array).
const compress = compressChunk;
const decompress = decompressChunk;

// ── Shared state-bundle orchestration (used by useEmulator + useEmulatorWorker) ──
// All IDB-only, so the worker hook (whose module lives off-thread) reuses them on
// the main thread against the same IndexedDB the worker writes to.
export async function listSavedDevices(): Promise<string[]> {
  const keys = await idbGetAllKeys();
  return keys.filter(k => k.startsWith('state-')).map(k => k.slice(6));
}
// Collect every restore-compatible save (+ its CF/SSD/Datapak images) into a
// downloadable bundle. profileMap supplies optional human-readable names; null
// when there are no states.
export async function collectStatesBundle(profileMap: Map<string, string>): Promise<Blob | null> {
  const stateKeys = (await idbGetAllKeys()).filter(k => k.startsWith('state-'));
  const devices: BundleDeviceInput[] = [];
  for (const key of stateKeys) {
    const deviceId = key.slice(6);
    const stored = await idbGet<{ version: number; deviceId?: string; heap: Uint8Array }>(key);
    if (!stored || !RESTORE_COMPATIBLE_VERSIONS.has(stored.version) || !stored.heap) continue;
    devices.push({
      id: deviceId, displayName: profileMap.get(deviceId), schemaVersion: stored.version,
      heap: stored.heap,
      cf:       (await idbGet<Uint8Array>(idbCardKey(deviceId)))    ?? undefined,
      ssd0:     (await idbGet<Uint8Array>(idbSsdKey(deviceId, 0)))  ?? undefined,
      ssd1:     (await idbGet<Uint8Array>(idbSsdKey(deviceId, 1)))  ?? undefined,
      datapak0: (await idbGet<Uint8Array>(idbDatapakKey(deviceId, 0))) ?? undefined,
      datapak1: (await idbGet<Uint8Array>(idbDatapakKey(deviceId, 1))) ?? undefined,
    });
  }
  return devices.length === 0 ? null : encodeBundle(devices);
}
// One device's save, in the same PSIONST1 form collectStatesBundle produces.
//
// The desktop app mirrors each save to a generation file on disk, and does it
// per device rather than for the whole store: collectStatesBundle walks every
// saved device, which on an autosave timer would mean re-encoding twenty
// machines a minute to capture a change in one.
//
// The heap comes out of IDB already gzipped, so there is no recompression
// here — this is a read, an encode of offsets, and a Blob. Returns null when
// the device has no restore-compatible save yet.
export async function collectDeviceBundle(
  deviceId: string,
  displayName?: string,
): Promise<{ blob: Blob; schemaVersion: number } | null> {
  const stored = await idbGet<{ version: number; deviceId?: string; heap: Uint8Array }>(
    idbStateKey(deviceId));
  if (!stored || !RESTORE_COMPATIBLE_VERSIONS.has(stored.version) || !stored.heap) return null;
  const blob = await encodeBundle([{
    id: deviceId, displayName, schemaVersion: stored.version,
    heap: stored.heap,
    cf:       (await idbGet<Uint8Array>(idbCardKey(deviceId)))    ?? undefined,
    ssd0:     (await idbGet<Uint8Array>(idbSsdKey(deviceId, 0)))  ?? undefined,
    ssd1:     (await idbGet<Uint8Array>(idbSsdKey(deviceId, 1)))  ?? undefined,
    datapak0: (await idbGet<Uint8Array>(idbDatapakKey(deviceId, 0))) ?? undefined,
    datapak1: (await idbGet<Uint8Array>(idbDatapakKey(deviceId, 1))) ?? undefined,
  }]);
  return { blob, schemaVersion: stored.version };
}

// True when this device has a restore-compatible save in IDB. The desktop
// app asks before falling back to a generation file on disk — a wiped browser
// profile is exactly when the disk copy earns its keep.
export async function hasStoredState(deviceId: string): Promise<boolean> {
  const stored = await idbGet<{ version: number; heap?: Uint8Array }>(idbStateKey(deviceId));
  return !!stored && RESTORE_COMPATIBLE_VERSIONS.has(stored.version) && !!stored.heap;
}

// Import a bundle previously produced by collectStatesBundle, writing each chunk
// back into IDB. Per-device errors are collected, not thrown.
export async function applyStatesBundle(file: File): Promise<{ imported: number; errors: string[] }> {
  const decoded = await decodeBundle(new Uint8Array(await file.arrayBuffer()));
  let imported = 0; const errors: string[] = [];
  for (const meta of decoded.header.devices) {
    const deviceId = meta.id;
    const chunks = decoded.devices.get(deviceId);
    if (!chunks) continue;
    try {
      if (chunks.heap && chunks.heap.byteLength > 0) {
        if (meta.schemaVersion > STATE_SCHEMA_VERSION)
          throw new Error(`bundle is newer than this build (v${meta.schemaVersion} > v${STATE_SCHEMA_VERSION}) — please update the app`);
        if (!RESTORE_COMPATIBLE_VERSIONS.has(meta.schemaVersion))
          throw new Error(`bundle is from an older build (v${meta.schemaVersion}); only v${[...RESTORE_COMPATIBLE_VERSIONS].join(', v')} are restore-compatible`);
        const heapRaw = await decompress(chunks.heap);
        await idbPut(idbStateKey(deviceId), { version: STATE_SCHEMA_VERSION, deviceId, heap: chunks.heap, byteLength: heapRaw.byteLength });
      }
      if (chunks.cf && chunks.cf.byteLength > 0) await idbPut(idbCardKey(deviceId), await decompress(chunks.cf));
      if (chunks.ssd0 && chunks.ssd0.byteLength > 0) await idbPut(idbSsdKey(deviceId, 0), await decompress(chunks.ssd0));
      if (chunks.ssd1 && chunks.ssd1.byteLength > 0) await idbPut(idbSsdKey(deviceId, 1), await decompress(chunks.ssd1));
      if (chunks.datapak0 && chunks.datapak0.byteLength > 0) await idbPut(idbDatapakKey(deviceId, 0), await decompress(chunks.datapak0));
      if (chunks.datapak1 && chunks.datapak1.byteLength > 0) await idbPut(idbDatapakKey(deviceId, 1), await decompress(chunks.datapak1));
      imported++;
    } catch (err) {
      errors.push(`${deviceId}: ${err instanceof Error ? err.message : String(err)}`);
    }
  }
  return { imported, errors };
}

// Force the WASM linear memory to grow to at least `needed` bytes, so a
// snapshot larger than the current memory can be written in full.
//
// The obvious approach — malloc just the missing delta — is broken: dlmalloc
// can satisfy a delta-sized request out of free space that already exists
// *below* the current memory top, so no growth happens and the memory is
// still too small to hold the snapshot. That left the restore path wrongly
// concluding "growth failed" and cold-booting. The symptom was that the
// first couple of devices loaded after a state import would boot from
// scratch (a freshly-started module has plenty of free space to absorb the
// delta without growing) while later loads worked (by then the module's
// footprint had climbed enough that the delta no longer fit and growth was
// forced).
//
// We're only ever called when `needed > current size`, so a single block of
// `needed` bytes cannot fit in the current (smaller) memory and malloc is
// guaranteed to grow — or return 0 if it genuinely can't (true OOM), which
// we report so the caller can fall back to a clean cold boot. We free the
// block immediately: the caller overwrites the entire heap (allocator state
// included) right after, so its lifetime doesn't matter.
function growHeapToFit(mod: PsionModule, needed: number): boolean {
  if (mod.HEAPU8.byteLength >= needed) return true;
  const p = mod._malloc(needed);
  if (p) mod._free(p);
  return mod.HEAPU8.byteLength >= needed;
}

// ── Bootloader OS-card payloads ───────────────────────────────────────────
//
// The 5mx Pro and netBook ship bootloader-only flash: there's no OS in the
// device's own flash, so the bootloader expects to find the OS image on a
// FAT16 CompactFlash card and load it into DRAM.
//
//   • 5mx Pro: a 128 KB bootloader that loads SYS$ROM.BIN off the card.
//   • netBook: a 2 MB YModem bootloader that loads D:\OS.IMG (a raw EPOCARM
//     ROM image, ~14.6 MB) off the card.
//
// We synthesise the card on the fly: a blank FAT16 image with the bundled OS
// payload added as a single file. On real hardware the OS image is a plain
// visible file on the boot card, so both bootloader devices now write it
// visible (osVisible) — the user sees SYS$ROM.BIN / OS.IMG on the card the
// moment the OS comes up, and File Manager lists it.
//
// Historically the payload was written Hidden+System. That was a workaround
// for two symptoms once the OS took over and remounted the same card as a
// user data drive: a 5mx Pro tight FAT poll (PC 0x5000d41c, polling CF common
// memory at offset 0x504) and the netBook surfacing the payload as a stray
// File Manager entry. The 5mx Pro stall is now handled by the CF-accel hooks
// in core/windermere.cpp (loadROM-time trampoline pre-arm + per-TINT DRAM
// signature rescan), and the netBook lists OS.IMG on E: by design — so a
// visible file is correct on both, no longer a hazard.
//
// The OS payload is the large, slow-to-download part. It's fetched as part of
// device load (see prefetchOsPayload in the hook) so the "Insert CF card
// containing OS" button is instant even on a slow connection — the bytes are
// already in memory, or at least in flight, by the time the user clicks.
//
// The netBook bootloader's *faithful* CompactFlash boot — it reads D:\OS.IMG
// off the FAT16 card through its own medata/ATA driver (painting the real
// "Loading from CF card..." progress bar) and then boots the image it read —
// is the default in the emulator core (sa1100.cpp loadROM enables
// PSION_NB_NATIVE_CF + _NO_HANDOFF for the bootloader; attachCard picks the
// faithful path for a FAT16 card and the synthetic netBookLoadOsFromCard
// shortcut for a raw EPOCARM image). Set PSION_NB_SYNTHETIC_CF=1 to opt back
// into the fast synthetic shortcut.

interface OsCardSpec {
  // URL of the raw OS payload to fetch (the big SYS$ROM.BIN / OS.IMG).
  url: string;
  // Size of the synthesised FAT16 container. 16 MiB comfortably holds the
  // 5mx Pro SYS$ROM.BIN; 32 MiB holds the ~14.6 MB netBook OS.IMG with
  // default cluster size and leaves headroom for the bootloader's working
  // files.
  imageSize: number;
  // Name of the single file written into the FAT16 image.
  fileName: string;
  // Write the OS payload as a NORMAL (visible) file rather than Hidden+System.
  // On real hardware the OS image is a plain visible file on the boot card, so
  // the System screen / File Manager lists it once the OS comes up (drive E:
  // on the netBook, D: on the 5mx Pro). Writing it Hidden+System hides it from
  // the user even though the card mounts fine. Both bootloader devices set
  // this true; default false is retained only as a safe fallback.
  osVisible?: boolean;
}

// Map a bootloader-flash device to its OS-card spec, or null for devices that
// don't boot their OS off a CF card.
// `variant` selects an alternate OS payload for the same device. The default
// (undefined) is the device's normal OS image; the 5mx Pro and the netBook
// both also offer the 'eshell' variant, which boots the ESHELL test ROM
// (roms/ESHELL/SYS$ROM.BIN / roms/ESHELL/OS.IMG) instead of the stock OS.
// The synthesised card is otherwise identical — same size, same on-card file
// name — so the bootloader loads it exactly as it would the stock image.
export function osCardSpec(deviceId: string | null, variant?: string): OsCardSpec | null {
  if (deviceId === '5mxpro') {
    if (variant === 'eshell') {
      return {
        url: `${import.meta.env.BASE_URL}roms/ESHELL/SYS$ROM.BIN`,
        imageSize: 16 * 1024 * 1024,
        fileName: 'SYS$ROM.BIN',
        osVisible: true,
      };
    }
    return {
      url: `${import.meta.env.BASE_URL}roms/5mxPRO_v1.05(319)_patch_eng.bin`,
      imageSize: 16 * 1024 * 1024,
      fileName: 'SYS$ROM.BIN',
      // Write SYS$ROM.BIN as a plain visible file so the user sees it on the
      // CF card (drive D:) the moment the OS comes up — matching real 5mx Pro
      // hardware, where the OS image sits on the boot card as an ordinary
      // file.  The historical Hidden+System flag was a workaround for an OS
      // post-handoff remount stall (a tight FAT poll the OS entered when it
      // re-mounted the boot card as a user drive); that stall is now handled
      // by the CF-accel hooks in core/windermere.cpp (loadROM-time trampoline
      // pre-arm + per-TINT DRAM signature rescan), so the file can be visible
      // without re-triggering the hang.
      osVisible: true,
    };
  }
  if (deviceId === 'netbook') {
    if (variant === 'eshell') {
      // The netBook's own ESHELL build: a 876 KB EPOCARM image that boots to
      // the ESHELL text console instead of the EPOC desktop.  The bootloader
      // reads it off the card exactly as it reads the stock OS.IMG — same
      // file name, same faithful medata/ATA path — so a 16 MiB card is
      // plenty for an image this size.
      return {
        url: `${import.meta.env.BASE_URL}roms/ESHELL/OS.IMG`,
        imageSize: 16 * 1024 * 1024,
        fileName: 'OS.IMG',
        osVisible: true,
      };
    }
    return {
      url: `${import.meta.env.BASE_URL}roms/netBook_v1.05(450)_eng.img`,
      imageSize: 32 * 1024 * 1024,
      fileName: 'OS.IMG',
      // On the real netBook OS.IMG is a plain visible file, so the System
      // screen lists it on drive E:.  Match that (writing it Hidden+System
      // made E: look empty).
      osVisible: true,
    };
  }
  return null;
}

// Wrap an already-fetched OS payload in a fresh FAT16 image. Synchronous and
// cheap; the slow network fetch is handled separately by prefetchOsPayload.
export function buildOsCardImage(spec: OsCardSpec, payload: Uint8Array): Uint8Array | null {
  const img = createBlankImage(spec.imageSize);
  const attr = spec.osVisible ? 0 : (FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM);
  const r = addFile(img, spec.fileName, payload, attr);
  return r.ok ? img : null;
}

// ── Public interface ─────────────────────────────────────────────────────────

export interface EmulatorControls {
  state: EmulatorState;
  loadProgress: number | null;
  // Human-readable phase of the current device load ("Downloading ROM… 2.1 of
  // 8.4 MB", "Restoring saved session…"); null outside loads / between phases.
  loadStatus: string | null;
  error: string | null;
  deviceInfo: DeviceInfo | null;
  canvasRef: React.RefObject<HTMLCanvasElement>;
  deviceModeRef: React.MutableRefObject<boolean>;
  logs: string[];
  paused: boolean;
  cardAttached: boolean;
  // 5mx Pro-only flag: true after the bootloader has consumed the
  // synthesised SYS$ROM.BIN card and we've auto-detached. Used to
  // hide the "Insert CF card containing OS" button so the user
  // doesn't re-trigger the bootloader path while the OS is running.
  osCardConsumed: boolean;
  // Bootloader OS-card download feedback. When the user clicks "Insert CF
  // card containing OS" before the background OS-image fetch has finished,
  // osDownloading is true (the button flips to "Downloading…" and a bar
  // appears below the toolbar) and osDownloadProgress carries the download
  // fraction (0..1, or null when the total size is unknown — render an
  // indeterminate bar).
  osDownloading: boolean;
  osDownloadProgress: number | null;
  speakerEnabled: boolean;
  micEnabled: boolean;
  audioError: string | null;
  currentDeviceId: string | null;
  profiles: DeviceProfile[];
  loadDevice(deviceId: string, romUrl: string): Promise<void>;
  handleKeyDown(e: KeyboardEvent): void;
  handleKeyUp(e: KeyboardEvent): void;
  // Releases every key we still believe is held. Bound to window blur /
  // page-hide, where the browser stops delivering keyup.
  releaseHeldKeys(): void;
  handleInput(e: Event): void;
  handlePasteText(text: string): void;
  pasteFromClipboard(): Promise<void>;
  // Synthesises a press/release of a single EPOC key. Used by on-screen
  // mobile buttons for Esc, Menu, and the arrow keys — soft keyboards on
  // iOS / Android don't expose these, so they need their own UI affordance.
  sendEpocKey(epocKey: number, down: boolean): void;
  handlePointerDown(digitiserX: number, digitiserY: number): void;
  handlePointerMove(digitiserX: number, digitiserY: number): void;
  handlePointerUp(): void;
  // Move the pointer with no button held. Only the Geofox has somewhere
  // for this to go: its mouse pad moves an on-screen pointer that exists
  // whether or not a finger is tapping, so a host mouse hovering over the
  // panel has to reach the guest as motion without a press. On a
  // digitiser machine a hover is not an event at all, which is why the
  // overlay only calls this for the pointer devices that have one.
  handlePointerHover(digitiserX: number, digitiserY: number): void;
  // Enqueue a key-press pulse (down followed by up) for an arbitrary
  // EpocKey value. Used by the mobile on-screen Esc / Menu / arrow
  // buttons, since those keys aren't on the soft keyboard and can't
  // reach the emulator via handleKeyDown from a real keyboard event.
  pressEpocKey(epocKey: number): void;
  // Variant of pressEpocKey that holds a list of modifier keys (Shift,
  // Fn, Psion, …) down across the press, then releases them. Frame-
  // spaced through the same queue. Used by the on-screen Backlight
  // button so the EPOC kernel sees the same Fn+Space / Psion+Space
  // chord a real keyboard would have sent.
  pressEpocChord(modifiers: number[], key: number): void;
  // Live state of the LCD electroluminescent backlight pin. Polled by
  // the on-screen backlight overlay so its visual state stays in sync
  // with the kernel (which is what handles Fn+Space, the auto-off
  // timeout, and the Control Panel brightness slider). Returns false
  // on devices whose backlight pin isn't modelled (Series 3 family
  // including 3mx — MAME hasn't wired it either).
  getBacklight(): boolean;
  // Quarter-turns anticlockwise the emulated panel currently has to be
  // shown at. The netpad's "Switch orientation" (Tools menu) rotates the
  // whole EPOC desktop inside the unchanged 640×240 framebuffer, and the
  // device frame turns with it; every other machine reports 0 forever.
  getScreenOrientation(): number;
  // Emulated cycles executed so far, when the running path can report them.
  //
  // Only the worker path publishes this (it arrives on every status tick);
  // the main-thread path has no equivalent counter, so the field is optional
  // and a caller that cannot get one must assume the machine has advanced.
  // The desktop autosave uses it to avoid freezing the guest to write a
  // snapshot identical to the last — see lib/desktop/autosave.ts.
  getSimCycles?(): number;
  saveState(): Promise<void>;
  clearLogs(): void;
  // Enables or disables WASM-side log emission. The log panel toggle wires
  // this on when open / off when closed so the expensive WASM→JS console.log
  // path only runs when the user is actually inspecting logs.
  setLoggingEnabled(enabled: boolean): void;
  powerOff(): void;
  powerOn(): void;
  resetDevice(): void;
  clearSession(deviceId: string): Promise<void>;
  // CompactFlash card management.
  // attachCard stages the bytes in WASM, asserts card-present, and persists
  // to IndexedDB so the image survives reloads.
  attachCard(bytes: Uint8Array): Promise<boolean>;
  // updateCardInPlace replaces the attached card's bytes WITHOUT an
  // eject/insert: the OS mount stays alive and, on the Series 7, the
  // device's cached directory listing is refreshed so new files appear
  // on a fresh E: navigation. Use this for "push card edits to device"
  // when the card is already attached — it avoids the hot-swap re-mount
  // wall (which shows "Corrupt"). Returns true on success.
  updateCardInPlace(bytes: Uint8Array): Promise<boolean>;
  detachCard(): Promise<void>;
  // Bootloader-flash devices (5mx Pro, netBook): synthesises a FAT16
  // CF image carrying the device's OS payload (SYS$ROM.BIN for 5mx Pro,
  // OS.IMG for netBook) and attaches it. The card stays attached
  // through boot, matching real hardware. Returns true when the attach
  // succeeded.
  //
  // `variant` selects an alternate OS payload (see osCardSpec). Omit it for the
  // device's stock OS; pass 'eshell' on the 5mx Pro or netBook to boot the
  // machine's ESHELL ROM.
  attachOsCard(variant?: string): Promise<boolean>;
  // Returns the current in-device image bytes (including any on-device writes).
  // Null if no card is attached. Sync on the main thread; a Promise in worker
  // mode (the image lives in the worker) — callers must Promise.resolve() it.
  getCardBytes(): Uint8Array | null | Promise<Uint8Array | null>;
  // Sync on the main thread; a Promise in worker mode (RAM lives in the worker).
  // Callers should `await` the result, which is a no-op for the sync value.
  getRamSnapshot(): Uint8Array | null | Promise<Uint8Array | null>;

  // ── EPOC Unique id (behind the "Show debugging" setting) ──────────────
  // The id EPOC shows under System → Information → Machine.
  // machineIdSupported is false on devices whose identity chip the guest
  // can't read (Series 5, Osaris, the Revo, every SIBO machine) and on an
  // older psion.wasm without the bindings — the UI hides the control then.
  // machineId is the identity chip's half; machineIdPrefix is the model
  // UID EPOC prints in front of it (null when unknown for this machine),
  // and machineIdPrefixSettable says whether that half can be patched.
  machineIdSupported: boolean;
  machineId: number | null;
  machineIdPrefix: number | null;
  machineIdPrefixSettable: boolean;
  // Programs the full 64-bit id and remembers it for this device (it is
  // re-applied on every subsequent load and reset). Resolves to the id the
  // device actually holds afterwards — the high half stays put where it
  // can't be patched, and some machines reserve bits of the chip word — so
  // callers should show what came back rather than what they asked for.
  // The running OS cached the old id at boot, so a resetDevice() is needed
  // for EPOC itself to report the new one.
  setMachineId(id: bigint): Promise<bigint | null>;

  // ── ROM language variant ──────────────────────────────────────────
  // languageNames is empty on every single-language machine (and on an
  // older psion.wasm without the bindings), which is what hides the
  // control; the Geofox One offers English (UK) and English (USA).
  // language indexes into it. setLanguage remembers the choice for this
  // device and applies it to the emulator, but the running OS read the
  // index at boot — the machine has to be reset to come up in it.
  languageNames: string[];
  language: number;
  setLanguage(index: number): void;

  // Psion SSD pack management. Slot index matches the user-facing
  // "Pack A" / "Pack B" labels (slot 0 = A, slot 1 = B). ssdAttached
  // is sized to the device profile's ssdSlotCount; entries are true
  // when an image is currently inserted in that slot. The persistence
  // path mirrors CF: bytes go through localStorage keyed by
  // (deviceId, slot), re-attached on the next emulator init.
  ssdAttached: boolean[];
  // `kind` is the pack type presented to the Psion (hardware strap, not
  // content): 'flash' attaches as a Type 1 Flash SSD carrying a FEFS
  // volume (see fefs.ts), 'ram' as a RAM SSD (for dumps of real RAM
  // packs) — both read/write on the device — and 'protected' as a
  // hardware write-protected factory system disk. When omitted, sniffed
  // from the 0xF1A5 magic.
  attachSSD(slot: number, bytes: Uint8Array, kind?: PackKind): Promise<boolean>;
  detachSSD(slot: number): Promise<void>;
  // Sync on the main thread, a Promise in worker mode (like getCardBytes);
  // callers go through Promise.resolve().
  getSSDBytes(slot: number): Uint8Array | null | Promise<Uint8Array | null>;
  // Psion Organiser II Datapak / Rampak management. Distinct surface
  // from the SSD slots above so the dedicated dialog can show the
  // per-slot pack kind (Datapak / Rampak / Empty) discriminant. Slot
  // 0 = Pack A, slot 1 = Pack B; entries in datapakAttached are true
  // when an image is currently inserted.
  datapakAttached: boolean[];
  attachDatapak(slot: number, bytes: Uint8Array): Promise<boolean>;
  detachDatapak(slot: number): Promise<void>;
  getDatapakBytes(slot: number): Uint8Array | null;
  // 0 = empty, 1 = Datapak (read-only EPROM), 2 = Rampak (writable SRAM).
  getDatapakKind(slot: number): number;
  // Audio toggles — only meaningful when deviceInfo.hasAudio is true.
  // toggleSpeaker routes the emulated DAC to the browser output.
  // toggleMic requests microphone permission on first enable and routes
  // the captured stream into the emulated ADC (voice-memo recording).
  toggleSpeaker(): Promise<void>;
  toggleMic(): Promise<void>;

  // ── State file management ─────────────────────────────────────────────────
  // IDs of devices that have a saved state in IndexedDB. Refreshed after
  // save, clear, and import operations so the DevicePanel can show indicators.
  savedDevices: string[];
  // Export all device states (heap + CF card + SSD + Datapak) to a single
  // JSON bundle file that the browser downloads.
  exportAllStates(): Promise<void>;
  // Import a bundle previously exported by exportAllStates. Writes each
  // device's data back to IndexedDB; returns counts for UI feedback.
  importStates(file: File): Promise<{ imported: number; errors: string[] }>;

  // Reload the last-saved IDB state for the current device into the running
  // emulator — equivalent to "undo all changes since last Save State". Returns
  // true on success; false if no saved state exists or restoration fails.
  revertToSaved(): Promise<boolean>;

  // ── Host serial bridge (Windermere devices only) ──────────────────────
  // Bridges the browser to one of the SoC's UARTs so a PLP/PsiWin client
  // can talk to the emulated Revo's Remote Link service. uartIndex is
  // 1 or 2; the Revo's cable port is UART2. Returns false on devices
  // without a bridgeable UART (i.e. non-Windermere).
  serialAttachHost(uartIndex: number): boolean;
  serialDetachHost(uartIndex: number): boolean;
  serialIsAttached(uartIndex: number): boolean;
  serialReadBytes(uartIndex: number): Uint8Array;
  serialWriteBytes(uartIndex: number, data: Uint8Array): number;
}

// ── Hook ─────────────────────────────────────────────────────────────────────

// Options:
// • embedMode — used by the standalone iframe route (#/embed/<id>). Skips
//   the auto-load of LAST_DEVICE_KEY (the embed picks the device itself)
//   and skips writing LAST_DEVICE_KEY on each load so the embed doesn't
//   clobber the user's "last device" choice in the main app (which shares
//   localStorage at the same origin).
export interface UseEmulatorOptions {
  embedMode?: boolean;
  initialDeviceId?: string;
}

// One-shot guard for the "this psion.wasm has no orientation binding"
// warning below — module scope so it fires once per page, not once per
// poll (EmulatorView reads the orientation at 10 Hz).
let staleOrientationWarned = false;

export function useEmulator(options: UseEmulatorOptions = {}): EmulatorControls {
  const { embedMode = false, initialDeviceId } = options;
  const [state, setState]               = useState<EmulatorState>('idle');
  const [loadProgress, setLoadProgress] = useState<number | null>(null);
  const [loadStatus, setLoadStatus] = useState<string | null>(null);
  const [error, setError]               = useState<string | null>(null);
  const [deviceInfo, setDeviceInfo]     = useState<DeviceInfo | null>(null);
  const [profiles, setProfiles]         = useState<DeviceProfile[]>([]);
  const [logs, setLogs]                 = useState<string[]>([]);
  const [paused, setPaused]             = useState(false);
  const [cardAttached, setCardAttached] = useState(false);
  // Bootloader-flash devices (5mx Pro, netBook): tracks whether the
  // user has already attached the OS card this session. Once true,
  // the pulsing "Insert CF card containing OS" affordance is hidden
  // for the rest of the session — the bootloader has its card and
  // the OS inherits it without needing the button.
  const [osCardConsumed, setOsCardConsumed] = useState(false);
  // Download feedback for the bootloader OS card (see EmulatorControls).
  const [osDownloading, setOsDownloading] = useState(false);
  const [osDownloadProgress, setOsDownloadProgress] = useState<number | null>(null);
  // Per-slot SSD attachment state. Length 4 covers every SIBO device
  // (Siena uses [0]; 3a/3c/3mx use [0]-[1]; the MC400 wires four pack
  // slots); ARM-based devices never index into it. The slot count
  // surfaces via the device profile's ssdSlotCount; the dialog only
  // renders that many rows.
  const [ssdAttached, setSsdAttached] = useState<boolean[]>([false, false, false, false]);
  // Per-slot Datapak attachment state for the Psion Organiser II.
  // Length 2 covers Pack A + Pack B; non-Organiser II devices never
  // index into it. Surfaced via the device profile's
  // datapakSlotCount; the Datapak dialog only renders that many rows.
  const [datapakAttached, setDatapakAttached] = useState<boolean[]>([false, false]);
  // Machine ID (debug panel). Re-read from the emulator on every load and
  // reset; machineIdState.defaultId holds the device's factory value so
  // the panel can offer to go back to it.
  const [machineIdState, setMachineIdState] =
    useState<MachineIdState>({ supported: false, id: null, prefix: null, prefixSettable: false });
  // Which language a multilingual ROM boots into. Re-read from the
  // emulator on every load and reset; empty names mean this machine has
  // no choice to offer and the UI hides the control.
  const [languageState, setLanguageState] =
    useState<LanguageState>({ names: [], index: 0 });
  // Speaker preference defaults to ON so the device's boot tune (5mx Pro
  // bootloader chime, OS desk-app start sound, key clicks) plays without
  // the user having to discover the speaker button first. The actual
  // AudioContext can't be resumed before a user gesture, so the engine
  // creation itself is deferred to the first pointerdown / keydown via
  // the effect below; this state just records the preference.
  //
  // Page reloads start with audio OFF regardless of persisted state.
  // The persistence exists so SWITCHING DEVICES inside the same session
  // (e.g. 5mx → Revo) carries the user's last toggle state across the
  // device boundary without forcing them to re-click. A FRESH page
  // reload, however, should not auto-resume audio: it surprised users
  // ("browser emits audio before I pressed the speaker button") and on
  // mic specifically it would call getUserMedia on the auto-restore
  // effect, prompting the permission dialog when the user clicked the
  // SPEAKER button (because the mic-auto-restore effect re-fired on
  // the engine being instantiated, not because the user asked for it).
  //
  // Implementation: track whether this useEmulator instance has done
  // its first device load. If not, auto-restore is skipped — the
  // toggles stay where they are but the engine doesn't get told to
  // honour them. On the SECOND `state === 'running'` transition (i.e.
  // the user switched devices), auto-restore re-enables.
  const audioAutoRestoreArmedRef = useRef(false);
  const [speakerEnabled, setSpeakerEnabled] = useState(() => loadAudioPref(SPEAKER_PREF_KEY));
  const [micEnabled, setMicEnabled]     = useState(() => loadAudioPref(MIC_PREF_KEY));
  // Keep localStorage in sync with the boolean state so revoke / fail
  // paths that flip the state back to false also clear the persisted
  // preference. The explicit saveAudioPref calls in the toggle
  // callbacks are kept for parity, but this effect catches the cases
  // where state changes without going through them (e.g. mic
  // auto-re-enable fails on device load and we reset to false).
  useEffect(() => { saveAudioPref(SPEAKER_PREF_KEY, speakerEnabled); }, [speakerEnabled]);
  useEffect(() => { saveAudioPref(MIC_PREF_KEY, micEnabled); }, [micEnabled]);
  const [audioError, setAudioError]     = useState<string | null>(null);
  const [currentDeviceId, setCurrentDeviceId] = useState<string | null>(null);
  const [savedDevices, setSavedDevices] = useState<string[]>([]);

  const moduleRef            = useRef<PsionModule | null>(null);
  const lcdPtrRef            = useRef<number>(0);
  const rafRef               = useRef<number>(0);
  const canvasRef            = useRef<HTMLCanvasElement>(null!);
  const pausedRef            = useRef(false);
  // Monotonic device-load token. Every doLoadDevice() call claims the next
  // value; after each await it checks the token is still current and bails
  // out if a newer load has started in the meantime. This serialises
  // overlapping loads (e.g. the user picks device B while device A is still
  // mid-load, or while the save-on-switch of A is still compressing) so two
  // doLoadDevice runs never interleave their writes to the shared module /
  // lcdPtr / currentDeviceId refs. Without it, an interleaving could corrupt
  // the module enough that stepFrame() throws on every frame — which, with
  // the render loop's "keep alive on throw" guard, presents as the site
  // wedged in a permanent loading/error loop.
  const loadGenerationRef    = useRef(0);
  // Count of consecutive stepFrame() throws in the render loop. A single
  // throw is logged and the loop kept alive (a transient guest fault should
  // not freeze the UI), but a guest that faults on *every* frame would
  // otherwise spin forever burning CPU and flooding the console. After a
  // threshold we stop the loop and pause so the failure is visible and
  // recoverable (Reset device / switch device) rather than an endless loop.
  const stepErrorCountRef    = useRef(0);
  const deviceModeRef        = useRef(false);
  const logBufferRef         = useRef<string[]>([]);
  const audioEngineRef       = useRef<AudioEngine | null>(null);
  const romBytesRef          = useRef<Uint8Array | null>(null);
  const currentDeviceIdRef   = useRef<string | null>(null);
  // Cache of the raw OS-image payload fetch, keyed by device id. The fetch is
  // kicked off at device-load time (see prefetchOsPayload) so the "Insert CF
  // card containing OS" button doesn't block on the network when clicked. We
  // store the in-flight promise so a click landing mid-download just awaits
  // the existing fetch instead of starting a second one. A failed fetch
  // removes itself from the cache so a later click can retry.
  const osPayloadRef         = useRef<Map<string, Promise<Uint8Array | null>>>(new Map());
  // Latest OS-image download fraction (0..1, or -1 when the total size is
  // unknown), updated by the streaming fetch even while it runs in the
  // background. Lets a click that lands mid-download seed the progress bar
  // with the current position rather than restarting from zero.
  const osProgressRef        = useRef<number>(0);
  // Device ids whose OS payload has finished downloading successfully — lets
  // attachOsCard skip the "Downloading…" UI when the bytes are already cached.
  const osReadyRef           = useRef<Set<string>>(new Set());
  // True only while attachOsCard is actively waiting on the download, so the
  // streaming fetch pushes progress into React state then (and stays quiet
  // during pure background prefetch, avoiding needless re-renders).
  const watchingOsProgressRef = useRef(false);
  const sessionActiveRef     = useRef(false);
  // Promise chain for state saves. Every call to triggerSave() queues
  // onto the tail of this chain — never silently skipped — so the
  // cold-boot fire-and-forget save can't swallow a user-initiated
  // "Save State" click that lands while it's still compressing.
  // (Previous implementation used a `savingRef.current` boolean that
  // returned early if a save was in flight, which made the user's save
  // appear to succeed while actually writing nothing.) The chain
  // catches errors so a failed save doesn't poison subsequent ones.
  const saveChainRef         = useRef<Promise<void> | null>(null);
  // Pack kind each SSD slot is currently strapped as, so a state save
  // can persist it alongside the (guest-modified) image bytes. Without
  // it the reload path would fall back to sniffing the FEFS magic and
  // re-strap a factory system disk as a writable Flash pack.
  const ssdKindsRef          = useRef<(PackKind | undefined)[]>([]);
  const keydownHandledRef    = useRef(false);
  // Tracks whether a physical Shift key is currently held in EPOC's key state.
  // Used to decide whether to synthesise Shift for mobile symbol input.
  const epocShiftRef         = useRef(false);
  // EPOC key codes we have sent a key-down for and not yet a key-up.  The
  // browser only guarantees a keyup while the page has focus, so a key
  // held across an alt-tab (or a Cmd-shortcut that swallows the keyup)
  // would otherwise stay down forever — see releaseHeldKeys.
  const heldKeysRef          = useRef<Set<number>>(new Set());

  // Queue of synthetic key events produced by paste / mobile input / Shift
  // synthesis.  Each entry is sent to WASM at a frame boundary by the render
  // loop.  This is required because the Psion OS scans the keyboard matrix
  // periodically — if we set+clear a matrix bit synchronously between two
  // stepFrame() calls, the OS never observes the press.  By spacing events
  // across frames (and holding key-down for a couple of frames before the
  // matching key-up) we guarantee each press is detectable.
  type QueuedKey = { key: number; down: boolean };
  const keyQueueRef          = useRef<QueuedKey[]>([]);
  // Frames remaining before the next queue entry may be processed.
  const keyQueueWaitRef      = useRef(0);

  // console.log intercept for emulator output. We leave this installed for
  // the lifetime of the app; the WASM side gates the expensive part (see
  // setLoggingEnabled). When the "Show Logs" panel is closed, the WASM
  // logger short-circuits before reaching console.log, so this intercept
  // sees nothing and costs nothing.
  useEffect(() => {
    const orig = console.log;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    console.log = (...args: any[]) => {
      orig(...args);
      const msg = args.map(a => (typeof a === 'string' ? a : JSON.stringify(a))).join(' ');
      logBufferRef.current.push(msg);
    };
    return () => { console.log = orig; };
  }, []);

  // Load WASM on mount
  useEffect(() => {
    setState('loading-wasm');
    loadPsionModule()
      .then(mod => {
        moduleRef.current = mod;
        setProfiles(JSON.parse(mod.getAllDeviceProfilesJSON()) as DeviceProfile[]);
        setState('ready');
      })
      .catch(err => { setError(String(err)); setState('error'); });
  }, []);

  // Populate the list of devices with saved IDB states on mount
  useEffect(() => {
    idbGetAllKeys().then(keys => {
      setSavedDevices(keys.filter(k => k.startsWith('state-')).map(k => k.slice(6)));
    });
  }, []);

  // Auto-load last device when WASM is ready. Skipped in embed mode —
  // the iframe route always loads the device named in its URL, so the
  // last-device key (which it shares with the main app at the same
  // origin) shouldn't drive the device choice here.
  useEffect(() => {
    if (state !== 'ready' || embedMode) return;
    const lastId = initialDeviceId || localStorage.getItem(LAST_DEVICE_KEY);
    if (!lastId || !moduleRef.current) return;
    const profs = JSON.parse(moduleRef.current.getAllDeviceProfilesJSON()) as DeviceProfile[];
    const profile = profs.find(p => p.id === lastId && p.status === 'supported');
    if (profile) {
      const romUrl = `${import.meta.env.BASE_URL}roms/${profile.romFilename}`;
      void doLoadDevice(lastId, romUrl, true);
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [state]);

  // Warn before leaving while a session is active
  useEffect(() => {
    if (state !== 'running') return;
    const onBeforeUnload = (e: BeforeUnloadEvent) => { e.preventDefault(); e.returnValue = ''; };
    window.addEventListener('beforeunload', onBeforeUnload);
    return () => window.removeEventListener('beforeunload', onBeforeUnload);
  }, [state]);

  // Save on tab/window hide
  useEffect(() => {
    if (state !== 'running') return;
    const onHide = () => { if (document.visibilityState === 'hidden') void triggerSave(); };
    document.addEventListener('visibilitychange', onHide);
    return () => document.removeEventListener('visibilitychange', onHide);
  }, [state]); // eslint-disable-line react-hooks/exhaustive-deps

  // Desktop Ctrl+V / Cmd+V paste — textarea onPaste handles the mobile path
  useEffect(() => {
    if (state !== 'running') return;
    const onPaste = (e: ClipboardEvent) => {
      // Ignore if the event originated from our hidden textarea (it has its own handler)
      if ((e.target as HTMLElement)?.dataset?.psionInput) return;
      e.preventDefault();
      injectText(e.clipboardData?.getData('text') ?? '');
    };
    document.addEventListener('paste', onPaste);
    return () => document.removeEventListener('paste', onPaste);
  }, [state]); // eslint-disable-line react-hooks/exhaustive-deps

  // Audio engine survives across device switches. The WASM `mod` is
  // a singleton (set once during 'loading-wasm' and reused for every
  // ROM the user loads), and all audio-capable devices use the same
  // 8 kHz codec sample rate, so a single AudioEngine handles them
  // all. Destroying and recreating the engine on every device load
  // was the previous behaviour but it lost the AudioContext's resume
  // gesture — after the device-switch microtask boundary the
  // browser refused to start audio on the new context, leaving the
  // Speaker/Mic buttons lit yellow with no audio until the user
  // toggled them off and on (which gives the context a fresh gesture).
  //
  // Now we keep the engine alive. Each device load just re-publishes
  // the host audio state (speakerOn / micOn) into the new emulator
  // instance — see the deviceInfo-change effect immediately below.
  // The engine is only torn down on hook unmount (rare) or on a
  // catastrophic pump() failure (defensive).
  useEffect(() => {
    return () => {
      // Unmount = navigation to a non-emulator route (app library, usage
      // page): save the running device first, like a device switch would.
      void triggerSave();
      audioEngineRef.current?.destroy();
      audioEngineRef.current = null;
    };
  }, []); // mount/unmount only

  // After each device switch the WASM `mod`'s underlying emulator is
  // a fresh instance whose `setHostAudioEnabled` starts at (false,
  // false). Re-publish the engine's persisted speaker/mic state so
  // the new emulator routes audio correctly without requiring the
  // user to toggle the buttons.
  useEffect(() => {
    if (!deviceInfo) return;
    audioEngineRef.current?.republishHostState();
  }, [deviceInfo]);

  // Auto-restore the user's audio preferences after a DEVICE SWITCH
  // within the same session. The speaker and microphone are restored
  // strictly independently from their own persisted preferences — the
  // speaker path never touches getUserMedia, so re-enabling the speaker
  // can never pull the mic in (the previous split-effect design shared a
  // fragile arming ref whose ordering let the mic auto-restore fire on
  // the first device load / a speaker-only restore, which surfaced as
  // "enabling the speaker asks for microphone permission").
  //
  // A single effect keyed on BOTH prefs guarantees deterministic
  // ordering and one arming decision for the pair:
  //   * On the FIRST running+audio device of the session we only ARM and
  //     return — no engine calls. A fresh page (re)load must start with
  //     audio OFF: auto-resuming the speaker emits sound before the user
  //     ever clicked it, and auto-calling setMic would fire getUserMedia
  //     (and its permission prompt) without a user gesture.
  //   * On every SUBSEQUENT device switch we reconcile the engine to the
  //     persisted prefs. The AudioContext is already unlocked and any
  //     mic permission was granted earlier in the session, so neither
  //     enable needs a fresh gesture and the mic does not re-prompt.
  // Restore is enable-only; the toggle handlers own the disable path.
  useEffect(() => {
    if (state !== 'running' || !deviceInfo?.hasAudio) return;
    if (!audioAutoRestoreArmedRef.current) {
      audioAutoRestoreArmedRef.current = true;
      return;
    }
    if (!speakerEnabled && !micEnabled) return;
    let cancelled = false;
    (async () => {
      const engine = await ensureAudioEngine();
      if (!engine || cancelled) return;
      // Speaker first, fully independent of the mic.
      if (speakerEnabled) {
        try { await engine.setSpeaker(true); }
        catch (err) { if (!cancelled) setAudioError(String(err)); }
      }
      if (cancelled) return;
      // Mic only when the user's own mic preference is set — guarded so a
      // speaker-only restore never reaches getUserMedia.
      if (micEnabled) {
        try { await engine.setMic(true); }
        catch (err) {
          if (cancelled) return;
          setAudioError(String(err));
          // Permission likely revoked / no device — drop the persisted
          // preference so we stop retrying and the user is back in
          // control. The Mic button stays visible to re-grant.
          setMicEnabled(false);
        }
      }
    })();
    return () => { cancelled = true; };
  }, [state, deviceInfo, speakerEnabled, micEnabled]);

  async function ensureAudioEngine(): Promise<AudioEngine | null> {
    if (audioEngineRef.current) return audioEngineRef.current;
    const mod = moduleRef.current;
    if (!mod || !deviceInfo?.hasAudio) return null;
    try {
      const engine = await createAudioEngine(mod, deviceInfo.audioSampleRate || 8000);
      audioEngineRef.current = engine;
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__psionAudioEngine = engine;  // test hook
      setAudioError(null);
      return engine;
    } catch (err) {
      setAudioError(String(err));
      return null;
    }
  }

  // Render loop
  useEffect(() => {
    if (state !== 'running' || !deviceInfo) return;
    const mod    = moduleRef.current!;
    let canvas = canvasRef.current;
    if (!canvas) return;
    let ctx = canvas.getContext('2d')!;
    const { lcdWidth, lcdHeight } = deviceInfo;
    const ptr = lcdPtrRef.current;

    // Wipe any pixels left over from a previous device. Setting the canvas's
    // width/height attribute auto-clears the buffer, but only when the new
    // value actually differs — switching between same-dimension devices
    // (e.g. Series 5 → 5mx, both 640×240) leaves the prior frame on screen
    // until the first RAF tick repaints it.
    ctx.clearRect(0, 0, lcdWidth, lcdHeight);

    // Pre-allocate once — avoids per-frame GC pressure (otherwise ~1 MB/frame
    // of Uint8ClampedArray + ImageData allocation causes periodic JS pauses).
    const pixelBuf  = new Uint8ClampedArray(lcdWidth * lcdHeight * 4);
    const imageData = new ImageData(pixelBuf, lcdWidth, lcdHeight);

    // Paint the current LCD framebuffer to the canvas synchronously so the
    // first visible frame after mount carries content rather than the
    // transparent post-clearRect state — otherwise the canvas shows
    // whatever's behind it (the device skin's LCD area, plus the page
    // background where the canvas extends past the skin) for the gap
    // between mount and the first requestAnimationFrame callback.  Most
    // visible on a fresh cold boot of bootloader-flash devices like the
    // netBook, where the skin photo's LCD bezel area + JPEG compression
    // can read as a blue tint until the splash paints over it.  The
    // cold-boot preroll above (COLD_BOOT_PREROLL[deviceId]) ensures the
    // splash is already in the framebuffer by the time we get here.
    mod.readLCD(ptr);
    pixelBuf.set(mod.HEAPU8.subarray(ptr, ptr + pixelBuf.length));
    if (deviceModeRef.current) applyDeviceModePixels(pixelBuf);
    ctx.putImageData(imageData, 0, 0);

    const tick = () => {
      // Re-acquire canvas/ctx when the ref points to a new element (e.g.
      // EmulatorView unmounted for SettingsView and remounted — the old
      // canvas is detached from the DOM and a fresh one takes its place).
      if (canvasRef.current !== canvas) {
        canvas = canvasRef.current;
        ctx = canvas ? canvas.getContext('2d')! : null!;
      }
      if (!canvas || !ctx) {
        rafRef.current = requestAnimationFrame(tick);
        return;
      }
      if (!pausedRef.current) {
        // Drain one synthetic key event per frame, holding key-downs for an
        // extra frame so the OS keyboard scanner can reliably observe them.
        if (keyQueueWaitRef.current > 0) {
          keyQueueWaitRef.current--;
        } else if (keyQueueRef.current.length > 0) {
          const ev = keyQueueRef.current.shift()!;
          mod.sendKey(ev.key, ev.down);
          // Hold for 2 frames after a down before allowing the next event;
          // 1 frame after an up is enough for the OS to detect the release.
          keyQueueWaitRef.current = ev.down ? 2 : 1;
        }
        // Wrap the emulator step in try/catch so an exception thrown out of
        // stepFrame (e.g. a guest fault surfaced as a C++/WASM abort) cannot
        // propagate out of the RAF callback and permanently kill the render
        // loop — which would freeze the whole device until a reload. Logging
        // the error (instead of dying silently) also surfaces the cause; the
        // loop keeps re-arming below so the UI can recover.
        try {
        mod.stepFrame();
        // A frame stepped cleanly — clear the consecutive-fault counter so
        // an occasional transient throw never accumulates toward the cap.
        stepErrorCountRef.current = 0;
        // CF poll-gap wall-clock compression: when the emulator is stuck in
        // the 2 s-per-sector polling watchdog (see Windermere::cfGapActive),
        // burst extra sim frames inside the same RAF tick so a file copy
        // completes in seconds rather than minutes. Bounded by a wall-time
        // budget so the UI stays responsive (≥~16 fps during CF activity).
        // The netBook's faithful boot reads the WHOLE ~14 MB OS.IMG off the
        // card (~28000 sectors) before handing off, so the budget/step cap
        // are set generously here — the screen is a near-static "Loading from
        // CF card..." progress bar during this window, so a coarse repaint
        // cadence is fine and the extra throughput keeps the load to a few
        // wall-seconds instead of a minute-plus.
        if (mod.isCFPollGapActive && mod.isCFPollGapActive()) {
          const budgetStart = performance.now();
          const BURST_BUDGET_MS = 60;
          const BURST_MAX_STEPS = 256;
          let burst = 0;
          while (
            burst < BURST_MAX_STEPS &&
            performance.now() - budgetStart < BURST_BUDGET_MS &&
            mod.isCFPollGapActive()
          ) {
            mod.stepFrame();
            burst++;
          }
        }
        } catch (err) {
          // A throw out of stepFrame would otherwise abort this RAF callback
          // and stop the loop from re-arming (line below), freezing the device
          // until reload. Log it and keep the loop alive so a transient fault
          // can recover; the underlying fault still needs fixing, but it should
          // not present as a permanent hang.
          stepErrorCountRef.current++;
          console.error(
            `[emu] stepFrame threw (${stepErrorCountRef.current} in a row) — render loop kept alive:`,
            err,
          );
          setAudioError(`Emulator step error (kept running): ${err instanceof Error ? err.message : String(err)}`);
          // …but a guest that faults on EVERY frame must not spin forever:
          // that burns CPU and floods the console indefinitely (the symptom
          // of a wedged device looking like an "infinite loop"). After a few
          // seconds' worth of back-to-back faults, pause emulation and stop
          // re-arming the loop. The canvas keeps its last frame; the user can
          // Reset device or switch device to recover.
          const STEP_ERROR_CAP = 180; // ~3 s at 60 fps
          if (stepErrorCountRef.current >= STEP_ERROR_CAP) {
            console.error('[emu] stepFrame faulted on every frame — pausing the render loop');
            setAudioError('Emulator stalled after repeated step errors — Reset device or pick another device to recover.');
            pausedRef.current = true;
            setPaused(true);
            return; // do NOT re-arm: breaks the per-frame fault spin
          }
        }
        if (logBufferRef.current.length > 0) {
          const captured = logBufferRef.current.splice(0);
          setLogs(prev => [...prev, ...captured].slice(-500));
        }
        mod.readLCD(ptr);
        pixelBuf.set(mod.HEAPU8.subarray(ptr, ptr + pixelBuf.length));
        if (deviceModeRef.current) applyDeviceModePixels(pixelBuf);
        ctx.putImageData(imageData, 0, 0);
        // Drain the emulated codec's DAC queue into the WebAudio worklet.
        // Wrapped in try/catch so any audio-path exception — e.g. a mobile
        // browser quirk around AudioWorklet or transferable Float32Arrays —
        // can never kill the render loop and freeze the LCD.
        try {
          audioEngineRef.current?.pump();
        } catch (err) {
          console.error('[audio] pump failed, disabling engine:', err);
          audioEngineRef.current?.destroy();
          audioEngineRef.current = null;
          // Reflect the disabled state in the UI so the Speaker / Mic
          // buttons in the header drop their yellow "active" highlight
          // — otherwise they'd stay lit with no engine behind them and
          // the user would think audio was still on. The setX setters
          // also clear the persisted localStorage preference via the
          // sync effect, so a reload doesn't immediately retry the
          // failing path. The user can re-enable manually; if pump
          // works on the next attempt the engine comes back up.
          setSpeakerEnabled(false);
          setMicEnabled(false);
          setAudioError(`Audio engine disabled after pump failure: ${err instanceof Error ? err.message : String(err)}`);
        }
      }
      rafRef.current = requestAnimationFrame(tick);
    };
    rafRef.current = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(rafRef.current);
  }, [state, deviceInfo]);

  // ── Internal: save current device heap to IndexedDB ───────────────────────
  //
  // Saves are chained via saveChainRef so concurrent callers are
  // serialised rather than dropped. Every call returns a promise that
  // resolves once that specific save has actually written to IDB — the
  // user's "Save State" button can safely `await` this and know the
  // user's typed state has persisted (not the cold-boot snapshot left
  // from the fire-and-forget save kicked off by doLoadDevice).
  function triggerSave(): Promise<void> {
    const next = (saveChainRef.current ?? Promise.resolve())
      .catch(() => undefined)    // never propagate prior failures forward
      .then(() => doActualSave());
    saveChainRef.current = next;
    return next;
  }

  async function doActualSave(): Promise<void> {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !sessionActiveRef.current || !deviceId) return;
    try {
      // Snapshot the live CF card bytes BEFORE we copy out the WASM heap,
      // so the secondary `cf-${deviceId}` key (which the load path uses
      // as a safety net and to auto-reattach after a "Reset device")
      // stays in sync with whatever EPOC has written to the card.
      if (mod.isCFImageAttached()) {
        const snap = readCardFromWasm(mod);
        if (snap) await idbPut(idbCardKey(deviceId), snap);
      }
      // Same for the SSD slots: RAM and Flash packs are both writable
      // from the Psion, so the `ssd-${deviceId}-${slot}` key has to
      // follow whatever the guest has written. The kind goes with it —
      // it is a hardware strap, not something re-derivable from bytes.
      for (let slot = 0; slot < 4; slot++) {
        if (!mod.isSSDImageAttached(slot)) continue;
        const snap = readSSDFromWasm(mod, slot);
        if (!snap) continue;
        await idbPut(idbSsdKey(deviceId, slot), snap);
        const kind = ssdKindsRef.current[slot];
        if (kind) await idbPut(idbSsdTypeKey(deviceId, slot), kind);
      }
      // .slice() copies the entire WASM linear memory (currently 128 MB
      // initial + any growth from CF/SSD attachment). Gzip handles the
      // long zero runs efficiently — a typical post-boot snapshot
      // compresses to single-digit MB.
      const raw = mod.HEAPU8.slice();
      const data = await compress(raw);
      await idbPut(idbStateKey(deviceId), {
        version:  STATE_SCHEMA_VERSION,
        deviceId,            // belt-and-braces: the key already encodes this
        heap:     data,
        byteLength: raw.byteLength,  // uncompressed size, for restore-time growth
      });
      // Refresh the per-device "has saved state" indicator so the green
      // dot in the device panel appears after a cold-boot save or a
      // switch-triggered auto-save, not just when the user explicitly
      // clicks "Save State". setSavedDevices is React-state — multiple
      // setState calls per save are cheap (React batches and bails out
      // on identical arrays via reference, but the cost is negligible
      // either way next to a 128 MB compress).
      const keys = await idbGetAllKeys();
      setSavedDevices(keys.filter(k => k.startsWith('state-')).map(k => k.slice(6)));
    } catch (err) {
      console.error('[psion] state save failed:', err);
      throw err;
    }
  }

  // ── Internal: load a device (restore from IDB or cold-boot) ──────────────
  // Start (or reuse) the background fetch of a bootloader device's OS payload.
  // Returns the cached promise so callers either await an already-resolved
  // download or piggyback on one still in flight. Non-bootloader devices
  // resolve to null. The promise is removed from the cache on failure so the
  // next call retries rather than caching the error forever.
  // Cache key for an OS payload: the device id, suffixed with the variant when
  // one is given. Keeps a device's stock and 'eshell' downloads (and their
  // "ready" flags) separate so attaching one never short-circuits the other.
  function osCacheKey(deviceId: string, variant?: string): string {
    return variant ? `${deviceId}:${variant}` : deviceId;
  }

  function prefetchOsPayload(deviceId: string, variant?: string): Promise<Uint8Array | null> {
    const spec = osCardSpec(deviceId, variant);
    if (!spec) return Promise.resolve(null);
    const key = osCacheKey(deviceId, variant);
    const cache = osPayloadRef.current;
    const existing = cache.get(key);
    if (existing) return existing;

    // Fresh download for this device — reset the shared progress position.
    osProgressRef.current = 0;
    // Record the fraction (or -1 = indeterminate) and, only while a click is
    // waiting on us, mirror it into React state so the bar advances live.
    const report = (frac: number) => {
      osProgressRef.current = frac;
      if (watchingOsProgressRef.current) {
        setOsDownloadProgress(frac >= 0 ? frac : null);
      }
    };

    const p = (async (): Promise<Uint8Array | null> => {
      const resp = await fetch(spec.url);
      if (!resp.ok) return null;
      // Stream the body so we can report download progress. If the body
      // stream isn't available, fall back to a plain buffered read.
      if (!resp.body) {
        const buf = new Uint8Array(await resp.arrayBuffer());
        report(1);
        return buf;
      }
      const lenHeader = resp.headers.get('Content-Length');
      const total = lenHeader ? parseInt(lenHeader, 10) : 0;
      const reader = resp.body.getReader();
      const chunks: Uint8Array[] = [];
      let received = 0;
      let lastPct = -1;
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        if (!value) continue;
        chunks.push(value);
        received += value.byteLength;
        if (total > 0) {
          // Throttle to whole-percent steps so a slow trickle of small
          // chunks doesn't flood React with sub-pixel updates.
          const frac = received / total;
          const pct = Math.floor(frac * 100);
          if (pct !== lastPct) { lastPct = pct; report(frac); }
        } else {
          report(-1); // unknown total → indeterminate bar
        }
      }
      const out = new Uint8Array(received);
      let off = 0;
      for (const c of chunks) { out.set(c, off); off += c.byteLength; }
      report(1);
      return out;
    })()
      .catch(() => null)
      .then(bytes => {
        if (bytes) {
          osReadyRef.current.add(key);
        } else {
          // Don't cache a failure — let the next call retry from scratch.
          cache.delete(key);
        }
        return bytes;
      });

    cache.set(key, p);
    return p;
  }

  // Build the bootloader OS card for a device, awaiting the prefetched payload
  // (or the in-flight fetch) so the heavy download is never on the click path
  // when the prefetch has already completed.
  async function buildOsCard(deviceId: string, variant?: string): Promise<Uint8Array | null> {
    const spec = osCardSpec(deviceId, variant);
    if (!spec) return null;
    const payload = await prefetchOsPayload(deviceId, variant);
    if (!payload) return null;
    return buildOsCardImage(spec, payload);
  }

  async function doLoadDevice(deviceId: string, romUrl: string, tryRestore: boolean) {
    let mod = moduleRef.current;
    if (!mod) return;

    // Claim this load. `superseded()` becomes true the moment another
    // doLoadDevice() starts, letting us bail at every await boundary so two
    // overlapping loads never interleave their writes to the shared refs
    // (module / lcdPtr / currentDeviceId) or both commit `running`.
    const myGen = ++loadGenerationRef.current;
    const superseded = () => loadGenerationRef.current !== myGen;

    // A new load means the previous device's render loop must stop competing
    // for the main thread right now — and the per-frame fault counter resets
    // so a fresh device starts with a clean slate.
    stepErrorCountRef.current = 0;

    // Fresh device-load: clear any prior "OS card consumed" marker so
    // the pulsing "Insert CF card containing OS" button re-appears for
    // the new boot of a bootloader-flash device (5mx Pro / netBook).
    setOsCardConsumed(false);
    cancelAnimationFrame(rafRef.current);
    setLoadProgress(0);
    setLoadStatus(null);
    setState('loading-rom');

    try {
      let deviceName: string | null = null;
      let info: DeviceInfo | null = null;
      let restoredFromIDB = false;

      if (tryRestore) {
        // We accept only stored values whose schema-version field matches
        // the current build's STATE_SCHEMA_VERSION. Bare Uint8Arrays (the
        // pre-v4 unversioned shape) and mismatched versions get discarded
        // and fall through to cold boot — necessary because C++ struct-
        // layout changes between builds make an old snapshot's bytes land
        // at the wrong heap offsets in the new emulator and silently
        // corrupt input state.
        //
        // Identity is verified by:
        //   • the IDB key (`state-${deviceId}`) which is unique per device;
        //   • the `deviceId` field inside the stored value (belt-and-
        //     braces against accidental key/value mismatch);
        //   • bounded LCD dimensions after the restored heap is set —
        //     this catches a heap whose vtables have shifted enough that
        //     getDeviceInfo() returns garbage.
        //
        // The previous build also compared `info.deviceName` to the
        // profile's `displayName`. That check rejected legitimate restores
        // on devices where the C++ getDeviceName() string disagrees with
        // the profile displayName (5mx, 5mxpro, mc218, series5, pocketbk),
        // so it has been dropped.
        const stored = await idbGet<unknown>(idbStateKey(deviceId));
        const versioned = (stored && typeof stored === 'object' && !ArrayBuffer.isView(stored)
          && 'version' in (stored as object))
          ? (stored as { version: number; deviceId?: string; heap: Uint8Array; byteLength?: number })
          : null;
        if (versioned && RESTORE_COMPATIBLE_VERSIONS.has(versioned.version)
            && versioned.heap && versioned.heap.byteLength > 0
            // If the stored value carries a deviceId (v5+), require it to
            // match the requested one. v4 saves predate the field and
            // rely on the IDB key alone for identity.
            && (!versioned.deviceId || versioned.deviceId === deviceId)) {
          setLoadProgress(0.1);
          setLoadStatus('Restoring saved session…');
          const heap = await decompress(versioned.heap);
          if (superseded()) return;
          setLoadProgress(0.6);
          await yieldToUI();
          if (superseded()) return;
          if (heap.byteLength > 0) {
            // The WASM module starts at 128 MB (INITIAL_MEMORY) but
            // ALLOW_MEMORY_GROWTH lets it grow during a session (e.g. when a
            // large CF card is attached). If the saved heap is larger than the
            // current WASM memory, we must grow the memory before writing —
            // otherwise HEAPU8.set() would write past the end of the buffer.
            // growHeapToFit() forces the grow (see its comment for why a naive
            // delta-malloc silently fails to grow and cold-boots the device).
            if (!growHeapToFit(mod, heap.byteLength)) {
              // Growth failed (true OOM); restoring would overrun memory.
              // Discard the snapshot and fall through to a clean cold boot.
              await idbDelete(idbStateKey(deviceId));
            } else {
              // Attempt the restore. getDeviceInfo() can WebAssembly-trap on
              // a stale snapshot (virtual-method dispatch through an out-of-
              // range indirect-call index), and even when it returns the
              // values can be garbage from a now-shifted struct layout —
              // both treated the same: discard the snapshot, swap in a fresh
              // WASM module so the cold-boot path below runs on clean
              // memory, and continue.
              let restoreOk = false;
              try {
                mod.HEAPU8.set(heap);
                info = mod.getDeviceInfo();
                if (info && info.deviceName
                    && info.lcdWidth  > 0 && info.lcdWidth  <= 4096
                    && info.lcdHeight > 0 && info.lcdHeight <= 4096) {
                  deviceName = info.deviceName;
                  restoredFromIDB = true;
                  restoreOk = true;
                }
              } catch (err) {
                console.warn('[doLoadDevice] saved state incompatible with current build:', err);
              }
              if (!restoreOk) {
                setLoadProgress(0);
                setLoadStatus(null);
                await idbDelete(idbStateKey(deviceId));
                info = null;
                deviceName = null;
                // The heap we just wrote (or partially wrote) is now garbage
                // from this build's point of view; the safest recovery is a
                // fresh module instance with clean linear memory. Any
                // pointers we were holding into the old module's memory
                // (LCD framebuffer, audio engine ring buffers) must be
                // dropped before the cold-boot path tries to _free them.
                audioEngineRef.current?.destroy();
                audioEngineRef.current = null;
                lcdPtrRef.current = 0;
                const reloaded = await reloadPsionModule();
                // A newer load started while we were reloading — let it own
                // the (already-swapped) module rather than clobbering its
                // moduleRef with this stale instance.
                if (superseded()) return;
                mod = reloaded;
                moduleRef.current = mod;
              } else {
                setLoadProgress(0.8);
                setLoadStatus('Starting device…');
                await yieldToUI();
              }
            }
          }
        } else if (stored) {
          await idbDelete(idbStateKey(deviceId));
        }
      }

      if (!deviceName) {
        // Cold boot — stream the ROM fetch so we can report download progress.
        setLoadStatus('Downloading ROM…');
        const resp = await fetch(romUrl);
        if (!resp.ok) throw new Error(`HTTP ${resp.status} fetching ROM`);
        const contentLength = Number(resp.headers.get('Content-Length') || 0);
        let bytes: Uint8Array;
        if (contentLength > 0 && resp.body) {
          const reader = resp.body.getReader();
          const chunks: Uint8Array[] = [];
          let received = 0;
          for (;;) {
            const { done, value } = await reader.read();
            if (done) break;
            chunks.push(value);
            received += value.byteLength;
            // Content-Length can be the compressed transfer size while the
            // reader yields decompressed bytes — clamp so the bar can't
            // overshoot its 70% download share.
            const frac = Math.min(1, received / contentLength);
            setLoadProgress(0.7 * frac);
            setLoadStatus(`Downloading ROM… ${fmtMB(received)} of ${fmtMB(contentLength)} MB`);
          }
          bytes = new Uint8Array(received);
          let offset = 0;
          for (const c of chunks) { bytes.set(c, offset); offset += c.byteLength; }
        } else {
          // Size unknown — switch the bar to indeterminate rather than
          // sitting at 0% for the whole download.
          setLoadProgress(null);
          bytes = new Uint8Array(await resp.arrayBuffer());
        }
        setLoadProgress(0.7);
        setLoadStatus('Starting device…');
        await yieldToUI();
        // Bail before mutating the module if a newer load superseded us
        // mid-fetch — otherwise we'd upload this ROM over the module the
        // newer load is already setting up.
        if (superseded()) return;
        const ptr = mod.prepareROMUpload(bytes.length);
        mod.HEAPU8.set(bytes, ptr);
        deviceName = mod.loadBufferedROM(bytes.length, deviceId);
        if (!deviceName) throw new Error('ROM not recognised — unsupported or corrupt');
        romBytesRef.current = bytes;
        info = mod.getDeviceInfo();
        setLoadProgress(0.8);
        await yieldToUI();
        // In embed mode (chromeless iframe), fast-forward past blank boot
        // windows so the first visible frame has content — there's no
        // header or control bar to signal "the device is alive, just
        // booting."  In the main app the boot sequence plays in real-time
        // via the render loop, giving the user visual feedback.
        if (embedMode) {
          const prerollFrames = COLD_BOOT_PREROLL[deviceId] ?? 0;
          // stepFrameFull (unbounded): the preroll fast-forwards a fixed
          // amount of SIM time past the blank boot window, so each frame must
          // run in full — stepFrame()'s wall-clock budget could cut a busy
          // bootloader frame short and leave the splash unrendered.
          for (let i = 0; i < prerollFrames; i++) mod.stepFrameFull();
        }
      } else {
        // Fetch ROM bytes in background so resetDevice works
        fetch(romUrl)
          .then(r => r.ok ? r.arrayBuffer() : Promise.reject())
          .then(buf => { romBytesRef.current = new Uint8Array(buf); })
          .catch(() => {});
      }

      // Last chance to abort before we commit this device as the running
      // one. Past this point a newer load would have to undo our state, so
      // stop here if one has started.
      if (superseded()) return;

      // Allocate LCD buffer; skip _free when restoring (old ptr is stale in restored allocator)
      if (!restoredFromIDB && lcdPtrRef.current) mod._free(lcdPtrRef.current);
      lcdPtrRef.current = mod._malloc(info!.lcdWidth * info!.lcdHeight * 4);

      // Re-apply the machine ID the user programmed for this device, before
      // the render loop starts stepping it, so EPOC's boot-time read of the
      // identity chip already sees the new value. Runs on the restore path
      // too: a restored heap carries whatever ID was live when it was saved,
      // and the stored override is the more recent expression of intent.
      setMachineIdState(applyMachineId(mod, loadStoredMachineId(deviceId)));
      // Same deal for the ROM's language variant: the guest reads its
      // settings PROM once, early in boot, so the choice has to be in
      // place before anything steps.
      setLanguageState(applyLanguage(mod, loadStoredLanguage(deviceId)));

      setLoadProgress(0.95);
      await yieldToUI();
      if (superseded()) return;

      currentDeviceIdRef.current = deviceId;
      sessionActiveRef.current = true;
      // Embed mode shares localStorage with the main app at the same
      // origin; persisting here would silently change the main app's
      // last-loaded device every time the iframe was visited.
      if (!embedMode) {
        localStorage.setItem(LAST_DEVICE_KEY, deviceId);
      }

      setCurrentDeviceId(deviceId);
      setDeviceInfo(info!);
      setLogs([]);
      logBufferRef.current = [];
      setPaused(false);
      pausedRef.current = false;
      setLoadProgress(null);
      setLoadStatus(null);
      setState('running');

      trackDeviceLoad(deviceId);
      startSession(deviceId);

      // Auto-attach any previously-saved CF card image for this device.
      // Independent of the heap-state snapshot because the user should get
      // their card back even after a "Reset device" or a fresh cold boot.
      //
      // Special cases:
      // - 5mx Pro ships bootloader-only flash; the bootloader paints a
      //   splash before checking the CF slot.  We leave the slot empty
      //   until the user clicks the "Insert CF card containing OS"
      //   button.  (Documented under attachOsCard() below.)
      // - netBook also ships bootloader-only flash; like the 5mx Pro,
      //   we show a flashing "Insert CF card containing OS" button
      //   and wait for the user to click it.  The bootloader displays
      //   a grey splash while waiting.  On click, the OS card is
      //   synthesised and attached, triggering the native CF detect
      //   chain (MedChgCf → GPIO11 → sleep-wake → synthetic handoff).
      const usesBootloaderOsCard = (deviceId === '5mxpro' || deviceId === 'netbook');
      // Kick off the OS-image download now, in the background, so the
      // "Insert CF card containing OS" button is instant when clicked. On a
      // slow connection the multi-MB OS.IMG / SYS$ROM.BIN would otherwise
      // download only on click, making the button appear to do nothing for
      // many seconds. Fire-and-forget: the bytes land in osPayloadRef and the
      // button's attachOsCard() awaits the same promise.
      if (usesBootloaderOsCard) {
        void prefetchOsPayload(deviceId);
      }
      const autoAttachOsAfterLoad = false;
      const savedCard = await idbGet<Uint8Array>(idbCardKey(deviceId));
      // A newer load may have started during the IDB read above; if so, stop
      // before re-attaching this (now-stale) device's card/SSD/Datapak images
      // — the newer load owns the module now.
      if (superseded()) return;
      if (!usesBootloaderOsCard && !autoAttachOsAfterLoad &&
          savedCard && savedCard.byteLength > 0 && isFat16(savedCard)) {
        const ptr = mod.prepareCFImageUpload(savedCard.byteLength);
        mod.HEAPU8.set(savedCard, ptr);
        const ok = mod.attachCFImage(savedCard.byteLength);
        setCardAttached(ok);
      } else {
        if (!usesBootloaderOsCard && !autoAttachOsAfterLoad &&
            savedCard && savedCard.byteLength > 0) {
          await idbDelete(idbCardKey(deviceId));
        }
        setCardAttached(mod.isCFImageAttached());
      }

      // Same restore path for SSD packs. Up to 2 slots; any persisted
      // image gets re-attached via the WASM bindings. Bad sizes (e.g.
      // not power-of-two) cause attachSSDImage to return false; we
      // delete the bad blob in that case so the user isn't stuck with
      // a permanently-failing slot.
      const ssdState: boolean[] = [false, false, false, false];
      for (let slot = 0; slot < 4; slot++) {
        let saved = await idbGet<Uint8Array>(idbSsdKey(deviceId, slot));
        let savedKind = (await idbGet<PackKind>(idbSsdTypeKey(deviceId, slot))) ?? undefined;
        if (superseded()) return;
        // Devices that shipped with a System Disk SSD inserted (the MC400's
        // ROM:: disk in Pack D) get it pre-loaded on cold boot when the slot
        // is otherwise empty. A user-inserted pack persists to IndexedDB and
        // takes precedence; ejecting clears the slot as usual.
        if (!saved || saved.byteLength === 0) {
          const def = defaultSsdFor(deviceId, slot);
          if (def) {
            saved = (await fetchDefaultSsd(def.url)) ?? null;
            savedKind = def.kind;
            if (superseded()) return;
          }
        }
        if (!saved || saved.byteLength === 0) continue;
        const ptr = mod.prepareSSDImageUpload(saved.byteLength);
        mod.HEAPU8.set(saved, ptr);
        const ok = mod.attachSSDImage(slot, saved.byteLength,
                                      ssdTypeCode(saved, savedKind));
        if (ok) {
          ssdState[slot] = true;
          ssdKindsRef.current[slot] = savedKind;
        } else {
          await idbDelete(idbSsdKey(deviceId, slot));
          await idbDelete(idbSsdTypeKey(deviceId, slot));
        }
      }
      setSsdAttached(ssdState);

      // Same restore path for Datapak / Rampak slots (Organiser II).
      // Up to 2 slots; persisted .opk images come back through the
      // dedicated WASM exports. The pack-kind is re-detected from the
      // image bytes inside the C++ side, so no extra metadata is needed.
      const datapakState: boolean[] = [false, false];
      for (let slot = 0; slot < 2; slot++) {
        const saved = await idbGet<Uint8Array>(idbDatapakKey(deviceId, slot));
        if (superseded()) return;
        if (!saved || saved.byteLength === 0) continue;
        const ptr = mod.prepareDatapakUpload(saved.byteLength);
        mod.HEAPU8.set(saved, ptr);
        const ok = mod.attachDatapakImage(slot, saved.byteLength);
        if (ok) {
          datapakState[slot] = true;
        } else {
          await idbDelete(idbDatapakKey(deviceId, slot));
        }
      }
      setDatapakAttached(datapakState);

      // Immediately save initial state on cold boot so there's always something to restore
      if (!restoredFromIDB) {
        void triggerSave();
      }

      // netBook: auto-attach the OS card after the bootloader splash has
      // started running.  We give the bootloader ~1 second of wall-clock
      // time to paint its "Insert CF card" splash so users briefly see
      // the cold-boot screen, then we synthesise the CF-insert event the
      // same way the manual "Insert CF card containing OS" button does.
      // The C++ side's netBookLoadOsFromCard() detects the EPOCARM ROM
      // header on the attached image and performs the bootloader -> OS
      // handoff (strip header, copy body to RAM bank 1 at PA 0xC8000000,
      // disable MMU, jump to OS entry).  Net effect: the user sees the
      // splash flash briefly, then the Psion desktop boots normally.
      if (autoAttachOsAfterLoad) {
        const modSnapshot = mod;
        setTimeout(() => {
          void (async () => {
            const card = await buildOsCard(deviceId);
            if (!card) {
              console.warn('[doLoadDevice] netBook OS image fetch failed; '
                + 'leaving boot ROM at "Insert CF card" splash');
              return;
            }
            const ptr2 = modSnapshot.prepareCFImageUpload(card.byteLength);
            modSnapshot.HEAPU8.set(card, ptr2);
            const ok = modSnapshot.attachCFImage(card.byteLength);
            if (ok) {
              setCardAttached(true);
            } else {
              console.warn('[doLoadDevice] netBook OS attach returned false');
            }
          })();
        }, 1000);
      }
    } catch (err) {
      console.error('[doLoadDevice] failed:', err);
      setLoadProgress(null);
      setLoadStatus(null);
      setError(String(err));
      setState('error');
    }
  }

  // ── Shared text injection (paste + mobile input) ──────────────────────────
  // Enqueues the key sequence for each character.  The render loop drains the
  // queue at frame boundaries — see the keyQueueRef comment for why.
  function injectText(text: string) {
    const layout = keyboardLayoutForDevice(currentDeviceIdRef.current);
    for (const char of text) {
      const chord = charToEpocChord(char, layout);
      if (!chord) continue;
      enqueueChord(chord.modifiers, chord.key);
    }
  }

  function enqueueChord(modifiers: number[], key: number) {
    const q = keyQueueRef.current;
    for (const m of modifiers) q.push({ key: m, down: true });
    q.push({ key, down: true });
    q.push({ key, down: false });
    for (let i = modifiers.length - 1; i >= 0; i--) q.push({ key: modifiers[i], down: false });
  }

  // Exposed to the UI so on-screen buttons can inject any EpocKey value
  // (Esc, Menu, arrow keys, etc.) without needing a real keyboard event.
  const pressEpocKey = useCallback((epocKey: number) => {
    enqueueChord([], epocKey);
  }, []);

  // Same as pressEpocKey but holds a list of modifier keys (Shift, Fn,
  // Psion, …) down across the press. Used by the on-screen Backlight
  // button to send Fn+Space on Windermere / Psion+Space on SIBO, which
  // is what the real keyboard chord does — letting the EPOC kernel
  // handle the toggle (and its auto-off timeout) the way it would on
  // hardware.
  const pressEpocChord = useCallback((modifiers: number[], key: number) => {
    enqueueChord(modifiers, key);
  }, []);

  // Returns the live state of the LCD backlight pin. Falls back to
  // false on devices whose port pin isn't modelled and on older WASM
  // bundles that pre-date the getBacklight binding.
  const getBacklight = useCallback((): boolean => {
    const mod = moduleRef.current;
    return mod && typeof mod.getBacklight === 'function' ? mod.getBacklight() : false;
  }, []);

  // Orientation the guest is drawing at, in quarter-turns anticlockwise.
  // Falls back to 0 (never rotated) on WASM bundles that pre-date the
  // binding, which is also what every non-netpad device reports — but say
  // so once, because on the netpad that fallback is indistinguishable from
  // "Switch orientation is broken" and the fix is a rebuild.
  const getScreenOrientation = useCallback((): number => {
    const mod = moduleRef.current;
    if (!mod) return 0;
    if (typeof mod.getScreenOrientation !== 'function') {
      if (!staleOrientationWarned) {
        staleOrientationWarned = true;
        console.warn('psion.wasm pre-dates the screen-orientation binding — the '
          + 'netpad will draw rotated without the device turning. Rebuild it: '
          + 'bash scripts/build-wasm.sh');
      }
      return 0;
    }
    return mod.getScreenOrientation();
  }, []);

  // ── Public callbacks ──────────────────────────────────────────────────────

  const loadDevice = useCallback(async (deviceId: string, romUrl: string) => {
    // Prime an AudioContext synchronously while we're still in the
    // user-gesture context that triggered loadDevice. The browser's
    // autoplay policy lets us call AudioContext.resume() only inside
    // that gesture; doing so here means by the time the bootloader
    // actually writes its first PCM sample (sim cycle ~9.3M, ~250 ms
    // wall) the context is already running. Without this the chime
    // would drain as silence, since the device-load button click is
    // typically the only gesture the user makes before the chime
    // plays. createAudioEngine() picks this primed context up on its
    // first call.
    primeAudioContext();
    // We deliberately do NOT call primeMobileAudioSession() here even
    // though the device-load click is a user gesture. That helper
    // .play()s a looping silent <audio> element, and the browser lights
    // up the tab's "audio is playing" indicator for any actively-playing
    // HTMLMediaElement regardless of volume. Doing it on every device
    // load made the indicator appear before the user had ever clicked
    // the Speaker button. The toggleSpeaker handler now owns the iOS
    // unlock — it primes the mobile session synchronously inside its
    // own click handler, before any await, which is the only context
    // iOS Safari accepts.
    // Audio engine is no longer destroyed here — it survives across
    // device switches. The post-deviceInfo effect re-publishes the
    // speaker/mic state into the new emulator instance. Destroying
    // the engine on every device switch used to lose the AudioContext
    // resume gesture, leaving the Speaker/Mic buttons lit yellow but
    // silent until the user toggled them off and on again.
    // Save current device state before switching. Swallow save errors
    // so the user can still switch devices even if (e.g.) IDB is in a
    // bad state — the next save attempt has its own error surface.
    if (currentDeviceIdRef.current && currentDeviceIdRef.current !== deviceId) {
      // Park any live Remote Link session while the device is still
      // stepping (before the pause below), so its last reply gets acked
      // and the saved snapshot holds a clean, adoptable session rather
      // than one that dies on restore. Instant no-op without a link.
      await quiesceActiveSessions({ release: true });
      // Quiesce the outgoing device FIRST. The save copies its 128 MB heap
      // and gzips it; if the guest is doing strenuous work (e.g. stuck in
      // the CF poll-gap burst, which spends up to 60 ms of every animation
      // frame inside stepFrame), the async gzip gets starved behind the RAF
      // loop and the switch can appear to hang indefinitely — the URL has
      // already changed but the new device never appears. Pausing emulation
      // and cancelling the pending frame frees the main thread so the save —
      // and the subsequent load — run promptly. doLoadDevice() clears these
      // again once the new device is ready.
      pausedRef.current = true;
      cancelAnimationFrame(rafRef.current);
      endSession('switch');
      try { await triggerSave(); }
      catch (err) { console.warn('[psion] save-on-switch failed:', err); }
    }
    return doLoadDevice(deviceId, romUrl, true);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const resetDevice = useCallback(() => {
    const mod   = moduleRef.current;
    const bytes = romBytesRef.current;
    if (!mod || !bytes) return;
    const ptr = mod.prepareROMUpload(bytes.length);
    mod.HEAPU8.set(bytes, ptr);
    const deviceId = currentDeviceIdRef.current ?? '';
    const name = mod.loadBufferedROM(bytes.length, deviceId);
    if (!name) return;

    // Bootloader-flash devices (5mx Pro, netBook) reboot straight back into
    // their ROM bootloader, which on real hardware paints its splash and
    // waits for the user to (re-)insert the OS-bearing CF card. Pressing
    // reset must therefore return us to that pre-handoff state: detach the
    // synthesised OS card and clear the "consumed" marker so the pulsing
    // "Insert CF card containing OS" button re-appears (mirroring the
    // fresh-boot path in doLoadDevice, which clears osCardConsumed at load).
    // Without this the button's `!osCardConsumed` gate stays false after the
    // first attach and the affordance never comes back on reset.
    if (deviceId === '5mxpro' || deviceId === 'netbook') {
      mod.detachCFImage();
      setCardAttached(false);
      setOsCardConsumed(false);
    }

    // loadBufferedROM above built a fresh emulator, so the identity chip is
    // back to its factory ID — re-apply the user's, before any stepping, so
    // the reboot the user is watching comes up on the ID they programmed.
    // (Resetting is exactly how a new ID is meant to take effect: a running
    // EPOC has already cached the old one.)
    setMachineIdState(applyMachineId(mod, loadStoredMachineId(deviceId)));
    // The language variant is the same story, and the reset is exactly
    // how a newly picked one is meant to land.
    setLanguageState(applyLanguage(mod, loadStoredLanguage(deviceId)));

    const info = mod.getDeviceInfo();
    const prerollFrames = embedMode ? (COLD_BOOT_PREROLL[deviceId] ?? 0) : 0;
    // Unbounded preroll — see the doLoadDevice call site for why this uses
    // stepFrameFull rather than the wall-clock-budgeted stepFrame.
    for (let i = 0; i < prerollFrames; i++) mod.stepFrameFull();
    if (lcdPtrRef.current) mod._free(lcdPtrRef.current);
    lcdPtrRef.current = mod._malloc(info.lcdWidth * info.lcdHeight * 4);
    setLogs([]);
    logBufferRef.current = [];
    setDeviceInfo(info);
    setPaused(false);
    pausedRef.current = false;
  }, []);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    keydownHandledRef.current = false;

    // The listener is on `window`, so it also sees keystrokes aimed at the
    // host UI's own text fields (the Machine ID hex box, the Modem dialog's
    // compose fields, …). Those must be left to the browser: this handler
    // preventDefaults anything the device keymap covers, which would cancel
    // the character insertion and leave the field looking frozen while the
    // letters went to EPOC instead. See isHostTextEntry — the emulator's own
    // hidden textarea is excluded and keeps forwarding as before.
    if (isHostTextEntry(e.target)) return;

    // Mobile soft keyboards can't be trusted on keydown for character keys:
    // some fire key='Unidentified' (Gboard/iOS IME path — falls through to
    // the input event correctly), but others fire the *unshifted* character
    // with shiftKey=true even when the user typed a capital (so 'A' would
    // arrive as key='a'). Either way the keydown loses the case information
    // and produces lowercase. The input event below always carries the
    // actual typed character with the correct case, so for the mobile
    // textarea we let every single-character key fall through to handleInput
    // and only handle named special keys (Tab, Enter, Esc, arrows, etc.,
    // whose key values are multi-character names like 'Tab', 'ArrowLeft')
    // here.
    const fromMobileTextarea = Boolean((e.target as HTMLElement)?.dataset?.psionInput);
    if (fromMobileTextarea && e.key.length === 1) return;

    const chord = browserKeyToEpocChord(e, keyboardLayoutForDevice(currentDeviceIdRef.current));

    // Key not in any mapping — don't preventDefault so the browser input event
    // fires on the hidden textarea (critical for mobile keyboards that emit
    // key='Unidentified' but still fire the input event with the typed char).
    if (chord === null) return;

    e.preventDefault();
    keydownHandledRef.current = true;

    // The host OS repeats a held key as a stream of keydowns with
    // e.repeat set.  The Psion is already holding the key — on the matrix
    // machines the extra downs are idempotent, but the netpad has no
    // matrix and turns every down into a discrete TRawEvent, so forwarding
    // them stacks the host's auto-repeat on top of EPOC's own and a single
    // press of an arrow key runs away through a menu.  EPOC does its own
    // repeat from the key-down we already sent, so drop these.
    if (e.repeat) return;

    // Track physical Shift state sent to EPOC so we know whether to synthesise
    // it for symbols.  Keys 18/19 are EStdKeyLeftShift / EStdKeyRightShift.
    if (chord.key === 18 || chord.key === 19) epocShiftRef.current = true;

    // Mobile soft keyboards fire keydown+keyup nearly simultaneously with no
    // physical hold, so a plain sendKey(down)/sendKey(up) pair can both happen
    // before the next stepFrame() — EPOC's scanner never sees the press.
    // Detect this path by checking whether the event came from our hidden
    // textarea (data-psion-input="1").  When it did, route every key through
    // the frame-spaced queue so the OS scanner reliably observes each press.

    if (chord.modifiers.length === 0 && !fromMobileTextarea) {
      // Plain physical key from a desktop keyboard — send directly; the user
      // holds it long enough for the OS scanner to observe.  handleKeyUp releases it.
      heldKeysRef.current.add(chord.key);
      moduleRef.current?.sendKey(chord.key, true);
      return;
    }

    // Modifier-wrapped chord (e.g. Shift+'@' or Fn+S for ';') or any key from
    // the mobile soft keyboard.  Skip synthesising Shift if a physical Shift is
    // already held in EPOC.
    const mods = chord.modifiers.filter(m => !((m === 18 || m === 19) && epocShiftRef.current));
    enqueueChord(mods, chord.key);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const handleKeyUp = useCallback((e: KeyboardEvent) => {
    // Same host-UI exclusion as handleKeyDown: no key-down was sent for these,
    // so there is nothing to release, and preventDefault here would interfere
    // with the field's own handling.
    if (isHostTextEntry(e.target)) return;
    const chord = browserKeyToEpocChord(e, keyboardLayoutForDevice(currentDeviceIdRef.current));
    if (chord === null) return;
    e.preventDefault();
    if (chord.key === 18 || chord.key === 19) epocShiftRef.current = false;
    // Only release the main key directly if keydown sent it directly — i.e. a
    // desktop physical key with no modifier wrapping.  Keys routed through the
    // queue (mobile textarea or modifier-wrapped) are self-contained: the queue
    // already includes the matching key-up event.
    const fromMobileTextarea = Boolean((e.target as HTMLElement)?.dataset?.psionInput);
    if (chord.modifiers.length === 0 && !fromMobileTextarea) {
      heldKeysRef.current.delete(chord.key);
      moduleRef.current?.sendKey(chord.key, false);
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // Release everything we think is still held.  Bound to blur / page-hide:
  // the browser stops delivering keyup once focus leaves the page, so a key
  // held across a window switch is never released, and EPOC's auto-repeat
  // then runs until something else presses a key.
  const releaseHeldKeys = useCallback(() => {
    for (const key of heldKeysRef.current) moduleRef.current?.sendKey(key, false);
    heldKeysRef.current.clear();
    epocShiftRef.current = false;
  }, []);

  // Sends an EPOC key code directly. Used by on-screen mobile buttons for
  // keys (Esc, Menu, arrows) that mobile soft keyboards don't produce.
  const sendEpocKey = useCallback((epocKey: number, down: boolean) => {
    moduleRef.current?.sendKey(epocKey, down);
  }, []);

  // Handles input events from the hidden mobile-keyboard textarea.
  // Only reached when keydown didn't fire with a usable key (key='Unidentified'
  // on many mobile browsers), because we avoid preventDefault in that case.
  const handleInput = useCallback((e: Event) => {
    const ie = e as InputEvent;
    const ta  = e.target as HTMLTextAreaElement;
    ta.value  = '';

    // If keydown already sent this key (desktop or mobile with valid keydown),
    // skip to avoid double-sends.
    if (keydownHandledRef.current) {
      keydownHandledRef.current = false;
      return;
    }
    keydownHandledRef.current = false;

    // Handle deletion / enter inputTypes that arrive without data.  Routed
    // through the queue so they're spaced across frames like all other
    // synthetic keypresses — otherwise iOS Safari can fire several input
    // events in one task and the OS scanner misses them.
    if (ie.inputType === 'deleteContentBackward') { enqueueChord([], 1);  return; }
    if (ie.inputType === 'deleteContentForward')  { enqueueChord([], 13); return; }
    if (ie.inputType === 'insertLineBreak' || ie.inputType === 'insertParagraph') {
      enqueueChord([], 3); return;
    }

    if (!ie.data) return;
    injectText(ie.data);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const handlePasteText = useCallback((text: string) => {
    injectText(text);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const pasteFromClipboard = useCallback(async () => {
    // Try the async Clipboard API first.  Requires a user gesture (button
    // click is fine), document focus, and clipboard-read permission.
    if (navigator.clipboard?.readText) {
      try {
        const text = await navigator.clipboard.readText();
        if (text) injectText(text);
        return;
      } catch (err) {
        // Permission denied or document not focused.  Fall through to prompt.
        console.log(`Clipboard read failed: ${String(err)}`);
      }
    }
    // Fallback for browsers that block readText() (Safari without permission,
    // iOS Safari): ask the user to paste into a prompt dialog.  This is the
    // only paste path that works reliably from a button click on iOS.
    const text = window.prompt('Paste text to send to the device:');
    if (text) injectText(text);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const lastTouchRef = useRef<{ x: number; y: number }>({ x: 0, y: 0 });

  const handlePointerDown = useCallback((x: number, y: number) => {
    lastTouchRef.current = { x, y };
    moduleRef.current?.sendTouch(x, y, true);
  }, []);

  const handlePointerMove = useCallback((x: number, y: number) => {
    lastTouchRef.current = { x, y };
    moduleRef.current?.sendTouch(x, y, true);
  }, []);

  const handlePointerHover = useCallback((x: number, y: number) => {
    lastTouchRef.current = { x, y };
    moduleRef.current?.sendTouch(x, y, false);
  }, []);

  const handlePointerUp = useCallback(() => {
    // Send pen-up at the last touch position, NOT (0, 0).  EPOC's
    // window server uses the up-event's coords for hit-testing the
    // release; sending (0, 0) registers a release on the top-left
    // corner of the screen — which causes folders to open-then-close
    // (Down opens, Up at (0,0) clicks the title bar or system menu)
    // and breaks drag entirely.
    const { x, y } = lastTouchRef.current;
    moduleRef.current?.sendTouch(x, y, false);
  }, []);

  const clearLogs = useCallback(() => {
    setLogs([]);
    logBufferRef.current = [];
  }, []);

  const powerOff = useCallback(() => { setPaused(true);  pausedRef.current = true;  }, []);
  const powerOn  = useCallback(() => { setPaused(false); pausedRef.current = false; }, []);

  const saveState = useCallback(async () => {
    // Flush any live Remote Link session (without disconnecting it) so the
    // snapshot holds a cleanly-acked, adoptable session.
    await quiesceActiveSessions();
    await triggerSave();
    const keys = await idbGetAllKeys();
    setSavedDevices(keys.filter(k => k.startsWith('state-')).map(k => k.slice(6)));
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const clearSession = useCallback(async (deviceId: string) => {
    await idbDelete(idbStateKey(deviceId));
    if (deviceId === currentDeviceIdRef.current) {
      sessionActiveRef.current = false;
      localStorage.removeItem(LAST_DEVICE_KEY);
    }
    const keys = await idbGetAllKeys();
    setSavedDevices(keys.filter(k => k.startsWith('state-')).map(k => k.slice(6)));
  }, []);

  // ── State file management ─────────────────────────────────────────────────
  //
  // Bundle format and round-trip live in lib/stateBundle.ts. The hook is
  // responsible for: pulling each device's chunks out of IDB, handing
  // them off to encodeBundle as a list, and triggering the download;
  // and on import, calling decodeBundle and writing each chunk back to
  // IDB under the per-device key.
  //
  // The previous JSON+base64 bundle materialised a multi-GB string when
  // exporting >2 devices' heaps and crashed the tab. The new binary
  // format keeps each gzip blob as a Blob part — no flat string ever
  // exists.

  const exportAllStates = useCallback(async (): Promise<void> => {
    // Persist the running device first (quiesced) so the export includes
    // its latest state with any active link left adoptable.
    await quiesceActiveSessions();
    await triggerSave();
    const blob = await collectStatesBundle(await readProfileMap());
    if (!blob) return;
    const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    // .psionstate is a custom extension so the file picker shows it as a
    // device-state bundle and not a generic .bin.
    triggerDownload(blob, `psion-states-${ts}.psionstate`);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  async function readProfileMap(): Promise<Map<string, string>> {
    const m = new Map<string, string>();
    const mod = moduleRef.current;
    if (!mod) return m;
    try {
      const profs = JSON.parse(mod.getAllDeviceProfilesJSON()) as DeviceProfile[];
      for (const p of profs) m.set(p.id, p.displayName);
    } catch { /* fall through, m stays empty */ }
    return m;
  }

  const importStates = useCallback(async (file: File): Promise<{ imported: number; errors: string[] }> => {
    const result = await applyStatesBundle(file);
    setSavedDevices(await listSavedDevices());
    return result;
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // ── Revert to saved state ─────────────────────────────────────────────────

  const revertToSaved = useCallback(async (): Promise<boolean> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !deviceId || !sessionActiveRef.current) return false;

    const stored = await idbGet<unknown>(idbStateKey(deviceId));
    const versioned = (stored && typeof stored === 'object' && !ArrayBuffer.isView(stored)
      && 'version' in (stored as object))
      ? (stored as { version: number; deviceId?: string; heap: Uint8Array })
      : null;
    if (!versioned || !RESTORE_COMPATIBLE_VERSIONS.has(versioned.version)
        || !versioned.heap || versioned.heap.byteLength === 0) return false;
    if (versioned.deviceId && versioned.deviceId !== deviceId) return false;

    const heap = await decompress(versioned.heap);
    if (heap.byteLength === 0) return false;

    // Grow the WASM memory to fit the snapshot before writing (see
    // growHeapToFit for why a naive delta-malloc fails to grow).
    if (!growHeapToFit(mod, heap.byteLength)) return false;

    mod.HEAPU8.set(heap.subarray(0, Math.min(heap.byteLength, mod.HEAPU8.byteLength)));

    // Reallocate the LCD buffer in the restored allocator so the render loop
    // has a pointer that the restored malloc state knows about.
    const info = mod.getDeviceInfo();
    if (info) {
      lcdPtrRef.current = mod._malloc(info.lcdWidth * info.lcdHeight * 4);
      setDeviceInfo(info);
    }

    // Sync attachment UI state with the restored WASM heap
    setCardAttached(mod.isCFImageAttached());
    setSsdAttached([0, 1, 2, 3].map(s => mod.isSSDImageAttached(s)));
    setDatapakAttached([mod.isDatapakAttached(0), mod.isDatapakAttached(1)]);
    return true;
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // ── CompactFlash card management ──────────────────────────────────────────

  const attachCard = useCallback(async (bytes: Uint8Array): Promise<boolean> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !deviceId || bytes.byteLength === 0) {
      console.log('[CF attach] aborted early: mod=%o device=%o size=%d',
        !!mod, deviceId, bytes.byteLength);
      return false;
    }
    const fat = isFat16(bytes);
    console.log('[CF attach] device=%s size=%d isFat16=%s', deviceId, bytes.byteLength, fat);
    if (!fat) return false;
    const ptr = mod.prepareCFImageUpload(bytes.byteLength);
    mod.HEAPU8.set(bytes, ptr);
    const ok = mod.attachCFImage(bytes.byteLength);
    console.log('[CF attach] core attachCard -> %s', ok);
    if (ok) {
      await idbPut(idbCardKey(deviceId), bytes);
      setCardAttached(true);
      trackFeature(deviceId, 'cf_attach');
    }
    return ok;
  }, []);

  // In-place content update: replace the attached card's bytes but keep the
  // socket inserted and the OS mount alive. On the Series 7 the core also
  // patches the F32 server's cached root-directory sector so a fresh E:
  // listing reflects the new files. Used by "push edits to device" when a
  // card is already attached; avoids the detach/insert hot-swap re-mount wall.
  const updateCardInPlace = useCallback(async (bytes: Uint8Array): Promise<boolean> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !deviceId || bytes.byteLength === 0) return false;
    if (!isFat16(bytes)) return false;
    // Fall back to a plain attach if the export is missing (older core).
    if (typeof mod.updateCFImageInPlace !== 'function') {
      return attachCard(bytes);
    }
    const ptr = mod.prepareCFImageUpload(bytes.byteLength);
    mod.HEAPU8.set(bytes, ptr);
    const ok = mod.updateCFImageInPlace(bytes.byteLength);
    console.log('[CF update-in-place] core -> %s', ok);
    if (ok) {
      await idbPut(idbCardKey(deviceId), bytes);
      setCardAttached(true);
      trackFeature(deviceId, 'cf_update_in_place');
    }
    return ok;
  }, [attachCard]);

  // Bootloader-card flow: build the default FAT16 OS-bearing card for
  // the current device and attach it. Wired to the device-specific
  // "Insert CF card containing OS" button on 5mx Pro and netBook —
  // both ship bootloader-only flash and load their OS image from a CF
  // card.
  //
  // The card stays attached after the bootloader hands off to the OS,
  // matching real hardware (where the user never has to remove the CF
  // card to commence boot). On 5mx Pro this used to require an auto-
  // detach hack to work around the OS re-mounting the bootloader card
  // and draining its FAT one sector every 2 s — that's resolved by the
  // bootloader-OS DRAM rescan in core/windermere.cpp, which re-enables
  // the CF accel hook on the OS image now living in RAM.
  const attachOsCard = useCallback(async (variant?: string): Promise<boolean> => {
    const mod = moduleRef.current;
    if (!mod) return false;
    const deviceId = currentDeviceIdRef.current;
    if (!deviceId) return false;
    const cacheKey = variant ? `${deviceId}:${variant}` : deviceId;

    // If the OS image hasn't finished downloading yet (slow connection, or a
    // click that beats the background prefetch), surface progress feedback so
    // the button doesn't look inert: it flips to "Downloading…" and a bar
    // appears below the toolbar. When the payload is already cached this whole
    // block is skipped and the attach is instant.
    const needsDownload = !osReadyRef.current.has(cacheKey);
    if (needsDownload) {
      watchingOsProgressRef.current = true;
      setOsDownloading(true);
      setOsDownloadProgress(osProgressRef.current >= 0 ? osProgressRef.current : null);
    }

    // buildOsCard awaits the payload prefetched at device-load time (or the
    // in-flight fetch, or a fresh one if a prior attempt failed), so on a
    // normal connection this resolves immediately with no network wait.
    let card: Uint8Array | null = null;
    try {
      card = await buildOsCard(deviceId, variant);
    } finally {
      if (needsDownload) {
        watchingOsProgressRef.current = false;
        setOsDownloading(false);
        setOsDownloadProgress(null);
      }
    }
    if (!card) return false;
    const ptr = mod.prepareCFImageUpload(card.byteLength);
    mod.HEAPU8.set(card, ptr);
    const ok = mod.attachCFImage(card.byteLength);
    if (!ok) return false;
    setCardAttached(true);
    trackFeature(deviceId, 'cf_attach');
    // Don't persist the netBook's synthesised OS card.  The C++
    // attachCard() path runs the bootloader handoff (sa1100.cpp:
    // netBookLoadOsFromCard) which reloads the OS body into ROM and
    // resets the CPU — i.e. the moment a user clicks the button the
    // device is no longer running the bootloader.  A persisted card
    // re-attached on the next boot would race the bootloader's splash
    // and short-circuit it before the user could see the cold-boot
    // UX (matching 5mx Pro's behaviour above).
    // Mark the OS card as consumed so the pulsing "Insert CF card
    // containing OS" affordance disappears for this session.
    // The card stays attached for the duration of the session — real
    // 5mx Pro hardware behaves the same way (the bootloader hands off
    // to the OS and the same card stays inserted; the user never has
    // to remove it). The previous 20 s auto-detach was a workaround
    // for an OS-remount stall that's since been fixed by the CF-accel
    // hooks in core/windermere.cpp (loadROM-time pre-arm + DRAM
    // signature rescan).
    setOsCardConsumed(true);
    return true;
  }, []);

  const detachCard = useCallback(async (): Promise<void> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod) return;
    // Before detaching, snapshot whatever EPOC has written to the card so the
    // persisted image reflects on-device edits.
    if (deviceId && mod.isCFImageAttached()) {
      const snap = readCardFromWasm(mod);
      if (snap) await idbPut(idbCardKey(deviceId), snap);
    }
    mod.detachCFImage();
    setCardAttached(false);
  }, []);

  // ── Psion SSD pack management ─────────────────────────────────────────────

  const attachSSD = useCallback(async (slot: number, bytes: Uint8Array,
                                       kind?: PackKind): Promise<boolean> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !deviceId || bytes.byteLength === 0) return false;
    if (slot < 0 || slot > 3) return false;   // MC400 wires 4 pack slots
    const effectiveKind: PackKind =
      kind ?? (bytes.length >= 2 && bytes[0] === 0xA5 && bytes[1] === 0xF1 ? 'flash' : 'ram');
    const ptr = mod.prepareSSDImageUpload(bytes.byteLength);
    mod.HEAPU8.set(bytes, ptr);
    const ok = mod.attachSSDImage(slot, bytes.byteLength, ssdTypeCode(bytes, effectiveKind));
    if (ok) {
      await idbPut(idbSsdKey(deviceId, slot), bytes);
      await idbPut(idbSsdTypeKey(deviceId, slot), effectiveKind);
      ssdKindsRef.current[slot] = effectiveKind;
      setSsdAttached(prev => {
        const next = [...prev];
        next[slot] = true;
        return next;
      });
    }
    return ok;
  }, []);

  const detachSSD = useCallback(async (slot: number): Promise<void> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || slot < 0 || slot > 3) return;
    // Snapshot any on-device writes before detaching so the persisted
    // image survives — RAM and Flash packs are both writable from the
    // Psion.
    if (deviceId && mod.isSSDImageAttached(slot)) {
      const snap = readSSDFromWasm(mod, slot);
      if (snap) await idbPut(idbSsdKey(deviceId, slot), snap);
    }
    mod.detachSSDImage(slot);
    ssdKindsRef.current[slot] = undefined;
    setSsdAttached(prev => {
      const next = [...prev];
      next[slot] = false;
      return next;
    });
  }, []);

  const getSSDBytes = useCallback((slot: number): Uint8Array | null => {
    const mod = moduleRef.current;
    if (!mod || slot < 0 || slot > 3) return null;
    return readSSDFromWasm(mod, slot);
  }, []);

  // ── Psion Organiser II Datapak management ─────────────────────────────────
  // Mirrors the SSD pair above (attach/detach/read) but routes through
  // the dedicated wasm exports because Datapaks have a kind discriminant
  // the SIBO SSD path doesn't model.

  const attachDatapak = useCallback(async (slot: number, bytes: Uint8Array): Promise<boolean> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !deviceId || bytes.byteLength === 0) return false;
    if (slot < 0 || slot > 1) return false;
    const ptr = mod.prepareDatapakUpload(bytes.byteLength);
    mod.HEAPU8.set(bytes, ptr);
    const ok = mod.attachDatapakImage(slot, bytes.byteLength);
    if (ok) {
      await idbPut(idbDatapakKey(deviceId, slot), bytes);
      setDatapakAttached(prev => {
        const next = [...prev];
        next[slot] = true;
        return next;
      });
    }
    return ok;
  }, []);

  const detachDatapak = useCallback(async (slot: number): Promise<void> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || slot < 0 || slot > 1) return;
    // Snapshot any on-device writes (Rampak only) before detach so the
    // persisted .opk survives the eject. No-op for Datapak / empty slot.
    if (deviceId && mod.isDatapakAttached(slot)) {
      const snap = readDatapakFromWasm(mod, slot);
      if (snap) await idbPut(idbDatapakKey(deviceId, slot), snap);
    }
    mod.detachDatapakImage(slot);
    setDatapakAttached(prev => {
      const next = [...prev];
      next[slot] = false;
      return next;
    });
  }, []);

  const getDatapakBytes = useCallback((slot: number): Uint8Array | null => {
    const mod = moduleRef.current;
    if (!mod || slot < 0 || slot > 1) return null;
    return readDatapakFromWasm(mod, slot);
  }, []);

  const getDatapakKind = useCallback((slot: number): number => {
    const mod = moduleRef.current;
    if (!mod || slot < 0 || slot > 1) return 0;
    return mod.getDatapakKind(slot);
  }, []);

  const getCardBytes = useCallback((): Uint8Array | null => {
    const mod = moduleRef.current;
    return mod ? readCardFromWasm(mod) : null;
  }, []);

  // Snapshot the live emulator RAM into a fresh Uint8Array — used by
  // the "Download RAM Snapshot" button to capture state at the moment
  // a user-reported bug surfaces. Returns null if no emulator is
  // running or the device has no host-visible RAM.
  const getRamSnapshot = useCallback((): Uint8Array | null => {
    const mod = moduleRef.current;
    if (!mod) return null;
    const size = mod.getRamSnapshotSize();
    if (size === 0) return null;
    const ptr = mod._malloc(size);
    try {
      mod.readRamSnapshot(ptr);
      return new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size));
    } finally {
      mod._free(ptr);
    }
  }, []);

  const setLoggingEnabled = useCallback((enabled: boolean) => {
    moduleRef.current?.setLoggingEnabled?.(enabled);
  }, []);

  // Programs the machine's Unique id and remembers it so every later load
  // / reset of this device comes up on it. Async only to match the
  // worker-mode signature; the write itself is a synchronous WASM call.
  const setMachineId = useCallback(async (id: bigint): Promise<bigint | null> => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !deviceId) return null;
    storeMachineId(deviceId, id);
    const next = applyMachineId(mod, id);
    setMachineIdState(next);
    if (next.id == null) return null;
    return (BigInt(next.prefix ?? 0) << 32n) | BigInt(next.id);
  }, []);

  // Picks the ROM's language variant and remembers it, so every later
  // load / reset of this device comes up on it. The running EPOC read the
  // index at boot and cached everything that follows from it, so the
  // caller has to reset for the change to show.
  const setLanguage = useCallback((index: number): void => {
    const mod = moduleRef.current;
    const deviceId = currentDeviceIdRef.current;
    if (!mod || !deviceId) return;
    storeLanguage(deviceId, index);
    setLanguageState(applyLanguage(mod, index));
  }, []);

  // AudioContext creation and getUserMedia both require a user gesture, so
  // these callbacks must be wired directly to button click handlers — don't
  // call them from effects.
  const toggleSpeaker = useCallback(async () => {
    const next = !speakerEnabled;
    if (next) {
      // iOS Safari unlocks AudioContext output only after a successful
      // HTMLMediaElement playback inside the click handler's synchronous
      // prefix — `await ensureAudioEngine()` below yields a microtask
      // boundary which iOS may treat as outside the gesture, so the
      // unlock <audio>.play() must happen before any await. Without
      // this, every speaker toggle on mobile needs to be paired with a
      // mic toggle to keep output audible. See primeMobileAudioSession.
      primeMobileAudioSession();
    }
    const engine = await ensureAudioEngine();
    if (!engine) return;
    try {
      await engine.setSpeaker(next);
      setSpeakerEnabled(next);
      saveAudioPref(SPEAKER_PREF_KEY, next);
      setAudioError(null);
      if (next) trackFeature(currentDeviceIdRef.current, 'speaker_on');
    } catch (err) {
      setAudioError(String(err));
    }
  }, [speakerEnabled, deviceInfo]); // eslint-disable-line react-hooks/exhaustive-deps

  const toggleMic = useCallback(async () => {
    const engine = await ensureAudioEngine();
    if (!engine) return;
    const next = !micEnabled;
    try {
      await engine.setMic(next);
      setMicEnabled(next);
      saveAudioPref(MIC_PREF_KEY, next);
      setAudioError(null);
      if (next) trackFeature(currentDeviceIdRef.current, 'mic_on');
    } catch (err) {
      // Permission denied / no device etc — surface so the UI can hint.
      setAudioError(String(err));
    }
  }, [micEnabled, deviceInfo]); // eslint-disable-line react-hooks/exhaustive-deps

  const serialAttachHost = useCallback((uartIndex: number): boolean => {
    const mod = moduleRef.current;
    return mod ? mod.serialAttachHost(uartIndex) : false;
  }, []);

  const serialDetachHost = useCallback((uartIndex: number): boolean => {
    const mod = moduleRef.current;
    return mod ? mod.serialDetachHost(uartIndex) : false;
  }, []);

  const serialIsAttached = useCallback((uartIndex: number): boolean => {
    const mod = moduleRef.current;
    return mod ? mod.serialIsAttached(uartIndex) : false;
  }, []);

  const serialReadBytes = useCallback((uartIndex: number): Uint8Array => {
    const mod = moduleRef.current;
    return mod ? serialReadBytesWasm(mod, uartIndex) : new Uint8Array(0);
  }, []);

  const serialWriteBytes = useCallback((uartIndex: number, data: Uint8Array): number => {
    const mod = moduleRef.current;
    return mod ? serialWriteBytesWasm(mod, uartIndex, data) : 0;
  }, []);

  return {
    state, loadProgress, loadStatus, error, deviceInfo, canvasRef, deviceModeRef, logs, paused, cardAttached,
    osCardConsumed,
    osDownloading,
    osDownloadProgress,
    ssdAttached,
    speakerEnabled, micEnabled, audioError,
    currentDeviceId, profiles,
    loadDevice, handleKeyDown, handleKeyUp, releaseHeldKeys, handleInput, handlePasteText, pasteFromClipboard,
    sendEpocKey,
    handlePointerDown, handlePointerMove, handlePointerUp, handlePointerHover,
    pressEpocKey,
    pressEpocChord,
    getBacklight,
    getScreenOrientation,
    saveState, clearLogs, powerOff, powerOn, resetDevice, clearSession,
    attachCard, updateCardInPlace, detachCard, attachOsCard, getCardBytes,
    getRamSnapshot,
    machineIdSupported: machineIdState.supported,
    machineId: machineIdState.id,
    machineIdPrefix: machineIdState.prefix,
    machineIdPrefixSettable: machineIdState.prefixSettable,
    setMachineId,
    languageNames: languageState.names,
    language: languageState.index,
    setLanguage,
    attachSSD, detachSSD, getSSDBytes,
    datapakAttached,
    attachDatapak, detachDatapak, getDatapakBytes, getDatapakKind,
    setLoggingEnabled,
    toggleSpeaker, toggleMic,
    savedDevices, exportAllStates, importStates,
    revertToSaved,
    serialAttachHost, serialDetachHost, serialIsAttached,
    serialReadBytes, serialWriteBytes,
  };
}

// Copies the currently-attached CF image bytes out of the WASM heap into a
// fresh Uint8Array. Returns null if no card is attached.
function readCardFromWasm(mod: PsionModule): Uint8Array | null {
  if (!mod.isCFImageAttached()) return null;
  const size = mod.getCFImageSize();
  if (size === 0) return null;
  const ptr = mod._malloc(size);
  try {
    mod.readCFImage(ptr);
    return new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size));
  } finally {
    mod._free(ptr);
  }
}

function readSSDFromWasm(mod: PsionModule, slot: number): Uint8Array | null {
  if (!mod.isSSDImageAttached(slot)) return null;
  const size = mod.getSSDImageSize(slot);
  if (size === 0) return null;
  const ptr = mod._malloc(size);
  try {
    mod.readSSDImage(slot, ptr);
    return new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size));
  } finally {
    mod._free(ptr);
  }
}

function readDatapakFromWasm(mod: PsionModule, slot: number): Uint8Array | null {
  if (!mod.isDatapakAttached(slot)) return null;
  const size = mod.getDatapakImageSize(slot);
  if (size === 0) return null;
  const ptr = mod._malloc(size);
  try {
    mod.readDatapakImage(slot, ptr);
    return new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size));
  } finally {
    mod._free(ptr);
  }
}
