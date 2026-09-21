// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The desktop app's main<->renderer contract, and the shape of the
// `window.psionHost` bridge the preload script exposes.
//
// This file is the SINGLE source of truth for both halves and is
// deliberately types-only with no imports. The Electron main process
// compiles it (see ../../tsconfig.json); the renderer reaches it with
// `import type` from frontend/src/lib/desktop/host.ts. The frontend's
// tsconfig is noEmit, so that cross-directory reference costs a typecheck
// edge and nothing at runtime — Vite never sees a value import and never
// bundles anything out of desktop/.
//
// Two rules keep the halves honest:
//   - Every filesystem call names a mount by ID plus a path RELATIVE to
//     it. Absolute paths never cross the bridge; the renderer is not
//     trusted to say where a mount lives, and mount roots are only ever
//     established through a native folder dialog. See main/hostfs.ts.
//   - Channel names live in CHANNELS so a typo fails to compile rather
//     than silently never firing.

// ── Mounts ───────────────────────────────────────────────────────────────
//
// 'internal' is the per-device folder synced to the machine's own internal
// drive over the Remote Link cable (C:\Documents\ on EPOC32, M:\WRD\ on
// EPOC16). 'external' is the one shared card folder that follows the user
// between machines, projected into whatever removable medium the current
// device actually takes.
export type MountId = 'internal' | 'external';

export interface MountInfo {
  id: MountId;
  /**
   * Display-only path for the UI ("~/Psion/Shared Card"). Null when no
   * folder has been chosen. Never use it to build a path — every fs call
   * addresses files relative to the root that main holds.
   */
  path: string | null;
  enabled: boolean;
}

export interface HostDirent {
  /** POSIX-separated path relative to the mount root. */
  rel: string;
  size: number;
  mtimeMs: number;
  isDir: boolean;
}

// ── Save generations ─────────────────────────────────────────────────────

export interface SaveGeneration {
  /** Filename within the device's save directory, e.g. 20260919-140233.psionst1 */
  file: string;
  /** Epoch ms. */
  savedAt: number;
  bytes: number;
  /** STATE_SCHEMA_VERSION at save time. */
  schemaVersion: number;
}

// ── Window ───────────────────────────────────────────────────────────────

/**
 * Chrome that must NOT scale with the emulated panel — the pinned key
 * strip, mainly. Passed alongside the aspect ratio so the window grows by
 * this much instead of squeezing the screen.
 */
export interface AspectExtra { width: number; height: number }

export interface WindowStateEvent {
  maximized: boolean;
  focused: boolean;
  fullScreen: boolean;
}

// ── Commands from the native menu / tray / shortcuts ─────────────────────

export type HostCommand =
  | { kind: 'load-device'; deviceId: string }
  | { kind: 'open-switcher' }
  | { kind: 'open-drives' }
  | { kind: 'save-now' }
  | { kind: 'sync-now' }
  | { kind: 'toggle-keys' }
  | { kind: 'reset-device' }
  | { kind: 'revert-to-saved' };

export interface DeviceMenuEntry {
  id: string;
  name: string;
  supported: boolean;
  hidden: boolean;
}

// ── The bridge ───────────────────────────────────────────────────────────

export interface PsionHostInfo {
  platform: 'darwin' | 'win32' | 'linux';
  appVersion: string;
  /** Display only, never used to build a path. */
  userDataDir: string;
  /**
   * True when the app was started with --dev. Lets the renderer surface
   * developer affordances without guessing from the URL.
   */
  dev: boolean;
}

/**
 * The bridge, grouped so that a capability which has not landed yet is
 * genuinely ABSENT rather than present and throwing. `host.mounts` being
 * undefined is how the renderer knows not to offer a folder picker, which
 * is a better contract than a method that rejects at runtime.
 */
export interface PsionHost {
  readonly info: PsionHostInfo;
  readonly window: PsionHostWindow;
  readonly devices: PsionHostDevices;
  readonly state: PsionHostState;
  readonly lifecycle: PsionHostLifecycle;
  /** Save generations on disk. */
  readonly saves?: PsionHostSaves;
  /** Host folder mounts. */
  readonly mounts?: PsionHostMounts;
}

export interface PsionHostWindow {
  /**
   * Lock the window's CONTENT aspect to `ratio` (emulated panel
   * width/height), excluding `extra` of non-scaling chrome. Pass 0 to
   * unlock. Called on device load, on a netpad orientation change, and on a
   * clamshell lid change.
   */
  setAspectRatio(ratio: number, extra?: AspectExtra): Promise<void>;
  /** Snap content to an exact pixel size, for a pixel-perfect first frame. */
  setContentSize(width: number, height: number): Promise<void>;
  /** Size to an exact integer multiple of the panel, clamped to the display. */
  fitToScale(lcdWidth: number, lcdHeight: number, scale: number): Promise<void>;
  /** Geometry is remembered per device — every machine has its own aspect. */
  rememberGeometry(deviceId: string): Promise<void>;
  restoreGeometry(deviceId: string): Promise<void>;
  minimize(): void;
  toggleMaximize(): void;
  close(): void;
  /**
   * Manual drag fallback for a platform where -webkit-app-region proves
   * unreliable: polls the OS cursor and moves the window by the delta until
   * endDrag(), so the renderer must pair them on pointerdown/pointerup.
   */
  startDrag(): void;
  endDrag(): void;
  onStateChange(cb: (s: WindowStateEvent) => void): () => void;
}

export interface PsionHostDevices {
  /** Hand main the device list so the native menu and tray can show it. */
  publish(list: DeviceMenuEntry[]): void;
  /** Keep the menu's radio check and the window title in step. */
  setCurrent(id: string | null): void;
  /** Menu items, tray items and accelerators all arrive here. */
  onCommand(cb: (c: HostCommand) => void): () => void;
}

export interface PsionHostState {
  get<T>(key: string): Promise<T | null>;
  set(key: string, value: unknown): Promise<void>;
}

export interface PsionHostLifecycle {
  /**
   * Main sends this INSTEAD of quitting and waits for the callback to
   * resolve, or 25 s, whichever comes first. A 128 MB gzip cannot finish
   * inside a default quit.
   */
  onPrepareQuit(cb: () => Promise<void>): () => void;
  /** Non-null shows a blocking overlay and keeps the app off the fast-quit path. */
  setBusy(reason: string | null): void;
  notify(title: string, body: string): void;
}

export interface PsionHostSaves {
  write(deviceId: string, bundle: Uint8Array,
        meta: { savedAt: number; schemaVersion: number }): Promise<void>;
  list(deviceId: string): Promise<SaveGeneration[]>;
  read(deviceId: string, file: string): Promise<Uint8Array>;
  prune(deviceId: string, files: string[]): Promise<void>;
}

export interface PsionHostMounts {
  get(): Promise<Record<MountId, MountInfo>>;
  /** Opens the native folder picker. Resolves unchanged if cancelled. */
  pick(id: MountId): Promise<MountInfo>;
  clear(id: MountId): Promise<void>;
  setEnabled(id: MountId, on: boolean): Promise<void>;
  reveal(id: MountId, rel?: string): Promise<void>;
  list(id: MountId): Promise<HostDirent[]>;
  read(id: MountId, rel: string): Promise<Uint8Array>;
  write(id: MountId, rel: string, data: Uint8Array, mtimeMs?: number): Promise<void>;
  remove(id: MountId, rel: string): Promise<void>;
  mkdirp(id: MountId, rel: string): Promise<void>;
  /**
   * Debounced change notification. Carries no path detail — the mirror
   * re-lists. A per-path stream would be a lie on macOS atomic saves, where
   * a document save arrives as a rename dance over a temp file.
   */
  onChange(cb: (id: MountId) => void): () => void;
}

// ── Channels ─────────────────────────────────────────────────────────────

export const CHANNELS = {
  // renderer → main, invoke
  windowSetAspect:   'psion:window:set-aspect',
  windowSetContent:  'psion:window:set-content-size',
  windowFitToScale:  'psion:window:fit-to-scale',
  windowRemember:    'psion:window:remember-geometry',
  windowRestore:     'psion:window:restore-geometry',
  mountsGet:         'psion:mounts:get',
  mountsPick:        'psion:mounts:pick',
  mountsClear:       'psion:mounts:clear',
  mountsSetEnabled:  'psion:mounts:set-enabled',
  mountsReveal:      'psion:mounts:reveal',
  fsList:            'psion:fs:list',
  fsRead:            'psion:fs:read',
  fsWrite:           'psion:fs:write',
  fsDelete:          'psion:fs:delete',
  fsMkdirp:          'psion:fs:mkdirp',
  saveWrite:         'psion:save:write',
  saveList:          'psion:save:list',
  saveRead:          'psion:save:read',
  savePrune:         'psion:save:prune',
  stateGet:          'psion:state:get',
  stateSet:          'psion:state:set',

  // renderer → main, fire-and-forget
  windowMinimize:    'psion:window:minimize',
  windowMaximize:    'psion:window:toggle-maximize',
  windowClose:       'psion:window:close',
  windowStartDrag:   'psion:window:start-drag',
  windowEndDrag:     'psion:window:end-drag',
  devicesPublish:    'psion:devices:publish',
  deviceCurrent:     'psion:devices:current',
  setBusy:           'psion:app:set-busy',
  notify:            'psion:app:notify',
  quitReady:         'psion:quit:ready',

  // main → renderer
  command:           'psion:command',
  fsChanged:         'psion:fs:changed',
  windowState:       'psion:window:state',
  prepareQuit:       'psion:quit:prepare',
} as const;

declare global {
  // eslint-disable-next-line no-var
  interface Window { psionHost?: PsionHost }
}
