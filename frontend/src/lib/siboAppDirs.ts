// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Where a file has to live on a SIBO volume for the machine to find it.
//
// EPOC16's System screen does not show a flat directory — it scans a fixed set
// of app folders on every drive and lists what it finds under the matching
// application. A .WRD sitting in the root is reachable through Disk →
// Directory and nowhere else, which to a user looks like the file did not
// arrive. Putting it in \WRD\ is what makes it appear in Word's own file list.
//
// Lifted out of SSDDialog so the shared-card projection routes files the same
// way the SSD dialog already does; that component keeps using it, so there is
// one table rather than two that drift.

export const APP_DIR_BY_EXT: Record<string, string> = {
  WRD: 'WRD',   // Word
  AGN: 'AGN',   // Agenda
  SPR: 'SPR',   // Sheet
  DBF: 'DAT',   // Data
  WLD: 'WLD',   // World
  OPL: 'OPL',   // OPL source
  OPO: 'OPO',   // compiled OPL
  OPA: 'APP',   // OPL applications
  APP: 'APP',
  IMG: 'APP',   // RunImg apps ship alongside .APP on factory packs
};

/** The app folder a filename belongs in, or undefined for the root. */
export function appDirForFile(name: string): string | undefined {
  const dot = name.lastIndexOf('.');
  if (dot < 0) return undefined;
  return APP_DIR_BY_EXT[name.slice(dot + 1).toUpperCase()];
}

/** The reserved app folder names, for reversing the routing on read-back. */
export const APP_DIRS: ReadonlySet<string> = new Set(Object.values(APP_DIR_BY_EXT));

/**
 * True when `dir` is one of EPOC16's app folders.
 *
 * Read-back needs this: a file found in \WRD\ came from the host folder's
 * ROOT and must go back there, or the folder slowly accretes EPOC's directory
 * layout. A folder the user made themselves, which is not one of these, passes
 * through in both directions.
 */
export function isAppDir(dir: string): boolean {
  return APP_DIRS.has(dir.toUpperCase());
}
