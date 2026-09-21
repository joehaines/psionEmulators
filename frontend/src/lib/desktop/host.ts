// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The renderer's single point of contact with the desktop shell.
//
// Everything desktop-only goes through `host`, which is null in a browser.
// That is the whole capability layer: the web build's behaviour is
// unchanged because every new code path is behind a null check that is
// always false there.
//
// The PsionHost types come from desktop/src/ipc/contract.ts — one source of
// truth for both halves. It is a TYPE-ONLY import: the frontend's tsconfig
// is noEmit so this costs a typecheck edge and nothing else, and because
// `import type` is fully erased under isolatedModules, Vite never resolves
// it and nothing from desktop/ is ever bundled into either build.

import type { PsionHost } from '../../../../desktop/src/ipc/contract.ts';

export type {
  PsionHost, PsionHostInfo, MountId, MountInfo, HostDirent,
  SaveGeneration, HostCommand, DeviceMenuEntry, AspectExtra, WindowStateEvent,
} from '../../../../desktop/src/ipc/contract.ts';

/**
 * The host bridge, or null in a browser.
 *
 * Read once at module load: the preload runs before any renderer script,
 * so if it is going to be there at all it is there now. Reading it once
 * also means a page that somehow acquires a `psionHost` later cannot flip
 * the app into desktop mode mid-session.
 */
export const host: PsionHost | null =
  (typeof window !== 'undefined' && window.psionHost) || null;

/** True only inside the Electron shell. */
export function isDesktop(): boolean {
  return host !== null;
}

/**
 * The build-time flag, distinct from `isDesktop()`.
 *
 * `isDesktop()` asks "am I running in the shell right now"; this asks "was
 * this bundle built for the shell". They differ in exactly one useful way:
 * the web bundle can tree-shake desktop-only code on this constant, while
 * still shipping the null check for safety.
 */
export const DESKTOP_BUILD: boolean =
  import.meta.env.VITE_PSION_DESKTOP === '1';

/**
 * Narrowing helper for the many call sites that only make sense with a
 * host present. Throws rather than returning null so a caller that got
 * past its own guard fails loudly instead of silently doing nothing.
 */
export function requireHost(): PsionHost {
  if (!host) throw new Error('psionHost is unavailable — not running in the desktop shell');
  return host;
}
