// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Which ROM a device boots, when it has more than one to choose from.
//
// roms/ follows the layout of https://github.com/explit28/Psion-ROM —
// <Model>/<build>/<image> — and carries every image from there alongside
// the ones this site already shipped. This file is the site's list of the
// ones a device can actually be pointed at: the path under roms/, the
// version and language, and the languages the ROM offers.
//
// The first entry of each device is its default, and it is the image the
// device has always booted: for a plain ROM device it has to equal the
// registry's romFilename (core/device_registry.cpp), for the two machines
// that load their OS off a card (the 5mx Pro and the netBook) it is the OS
// image the frontend writes onto that card. tests/unit/rom-catalog-sync.mts
// checks both, and that every path exists with the size recorded here.
//
// Everything past the first entry was put through the native harness's
// boot gate (tests/boot/test-boot.sh semantics: --assert-boot, the device's
// own profile, no card) and only images that reached a live screen are
// listed. Images that did not — the 1 MB Series 3a v3.22f and Workabout
// v1.00f, the Series 5 v1.00(113) prototype, the two-chip Series 3 v1.77f,
// the German netPad 1.40(174) — are in roms/ but not here; roms/README.md
// has the list.
//
// Languages. A ROM's language options belong to the ROM, not the device:
// the emulator's per-device locale tables (core/windermere.cpp, series5.h,
// geofox.h) were decoded from the default images, and a German Revo or a
// Dutch Series 5 carries a different set behind the same DLL names. So
// each entry says what its image offers:
//   languages undefined — trust the emulator's own list (the defaults,
//                         which are what those tables describe);
//   languages: [...]    — these names, index for index, and only if the
//                         emulator reports the same number of choices;
//   languages: []       — the image boots its own language; no picker.
// Where an entry lists names they were confirmed against the image: the
// Siena's are read straight out of its locale blocks, and the English
// 5mx / Revo / 5mx Pro builds ship exactly the default image's ELocl<n> /
// Ekdata<n> set.
//
// The user's choice is remembered per device in localStorage. Anything the
// machine saves that only makes sense for one ROM is keyed off romStateTag:
// the saved session (a RAM image is only valid on the ROM that wrote it)
// and the language index (an index into that ROM's list).

export interface RomOption {
  /** Stable id: the upstream build folder name. */
  id: string;
  /** Path under roms/. */
  path: string;
  /** Image size in bytes; checked by tests/unit/rom-catalog-sync.mts. */
  size: number;
  version: string;
  /** Language of the ROM's own UI. */
  language: string;
  /** Language picker options this image offers — see the header. */
  languages?: string[];
  /** One line shown in Settings under the choice. */
  note?: string;
}

export interface DeviceRomSet {
  /**
   * 'rom' — the option replaces the device's boot ROM.
   * 'os'  — the device boots a bootloader from its registry ROM and loads
   *         the OS from a card; the option is the OS image on that card.
   */
  kind: 'rom' | 'os';
  /** First entry is the default. */
  options: RomOption[];
}

const EN_UK_SCAN = ['English (UK)', 'English (Scandinavian)'];
const EPOC_5MXPRO = [
  'English (UK)', 'English (Scandinavian)', 'English (USA)',
  'English (Portugal)', 'English (Hungary)',
];
const MC218_ENG = [
  'English (UK)', 'English (Sweden)', 'English (USA)',
  'English (Portugal)', 'English (Hungary)',
];
const REVO_ENG = ['English (UK)', 'English (Scandinavian)', 'English (USA)'];
const SIENA_ENG = ['English (UK)', 'English (USA)', 'Swedish', 'Spanish'];

export const ROM_CATALOG: Record<string, DeviceRomSet> = {
  series3: { kind: 'rom', options: [
    { id: 's3_v1.91f_eng', path: 'Series3/s3_v1.91f_eng/s3_v1.91f_eng.bin', size: 524288,
      version: '1.91F', language: 'English' },
    { id: 's3_v1.91f_multi', path: 'Series3/s3_v1.91f_multi/s3_v1.91f_multi.bin', size: 524288,
      version: '1.91F', language: 'Multi-lingual', languages: [],
      note: 'Asks on first boot: English, French, German, Spanish or Italian.' },
    { id: 's3_v1.80f_multi', path: 'Series3/s3_v1.80f_multi/s3_v1.80f_multi.bin', size: 524288,
      version: '1.80F', language: 'Multi-lingual', languages: [],
      note: 'Asks on first boot: English, French, German or Italian.' },
  ] },
  series3a: { kind: 'rom', options: [
    { id: 's3a_v3.40f_eng', path: 'Series3a/s3a_v3.40f_eng/s3a_v3.40f_eng.bin', size: 2097152,
      version: '3.40F', language: 'English (UK)' },
    { id: 's3a_v3.40f_usa', path: 'Series3a/s3a_v3.40f_usa/s3a_v3.40f_usa.bin', size: 2097152,
      version: '3.40F', language: 'English (USA)', languages: [] },
    { id: 's3a_v3.40f_ita', path: 'Series3a/s3a_v3.40f_ita/s3a_v3.40f_ita.bin', size: 2097152,
      version: '3.40F', language: 'Italian', languages: [] },
    { id: 's3a_v3.41f_ger', path: 'Series3a/s3a_v3.41f_ger/s3a_v3.41f_ger.bin', size: 2097152,
      version: '3.41F', language: 'German', languages: [] },
    { id: 's3a_v3.43f_rus', path: 'Series3a/s3a_v3.43f_rus/s3a_v3.43f_rus.bin', size: 2097152,
      version: '3.43F', language: 'Russian', languages: [] },
  ] },
  series3c: { kind: 'rom', options: [
    { id: 'oak_v5.20f_eng', path: 'Series3c/oak_v5.20f_eng/oak_v5.20f_eng.bin', size: 2097152,
      version: '5.20F', language: 'English' },
    { id: 'oak_v5.23f_ger', path: 'Series3c/oak_v5.23f_ger/oak_v5.23f_ger.bin', size: 2097152,
      version: '5.23F', language: 'German', languages: [] },
  ] },
  series3mx: { kind: 'rom', options: [
    { id: 'maple_v6.16f_eng', path: 'Series3mx/maple_v6.16f_eng/maple_v6.16f_eng.bin', size: 2097152,
      version: '6.16F', language: 'English' },
    { id: 'maple_v6.17f_nl', path: 'Series3mx/maple_v6.17f_nl/maple_v6.17f_nl.bin', size: 2097152,
      version: '6.17F', language: 'Dutch', languages: [] },
    { id: 'maple_v6.20f_fre', path: 'Series3mx/maple_v6.20f_fre/maple_v6.20f_fre.bin', size: 2097152,
      version: '6.20F', language: 'French', languages: [] },
  ] },
  siena: { kind: 'rom', options: [
    { id: 'vine_v4.20f_eng', path: 'Siena/vine_v4.20f_eng/vine_v4.20f_eng.bin', size: 1048576,
      version: '4.20F', language: 'English', languages: SIENA_ENG },
    { id: 'vine_v4.08f_eng', path: 'Siena/vine_v4.08f_eng/vine_v4.08f_eng.bin', size: 1048576,
      version: '4.08F', language: 'English', languages: SIENA_ENG },
    { id: 'vine_v4.21f_fre', path: 'Siena/vine_v4.21f_fre/vine_v4.21f_fre.bin', size: 1048576,
      version: '4.21F', language: 'French', languages: [] },
  ] },
  workabout: { kind: 'rom', options: [
    { id: 'w1_v2.40f_eng', path: 'Workabout/w1_v2.40f_eng/w1_v2.40f_eng.bin', size: 2097152,
      version: '2.40F', language: 'English' },
    { id: 'w1_v0.24b_eng', path: 'Workabout/w1_v0.24b_eng/w1_v0.24b_eng.bin', size: 2097152,
      version: '0.24B', language: 'English', languages: [],
      note: 'A pre-release (beta) build. Boots to the Insert Startup SSD prompt; Menu opens the System screen.' },
  ] },
  series5: { kind: 'rom', options: [
    { id: 'S5_v1.01(145)_eng', path: 'Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin', size: 6291456,
      version: '1.01(145)', language: 'English (UK)', languages: EN_UK_SCAN },
    { id: 'S5_v1.01(145)_ger', path: 'Series5/S5_v1.01(145)_ger/S5_v1.01(145)_ger.bin', size: 6291456,
      version: '1.01(145)', language: 'German', languages: [],
      note: 'Boots to its desktop with a "Program closed" message from Calc (KERN-EXEC 3) over it.' },
    { id: 'S5_v1.01(145)_ita', path: 'Series5/S5_v1.01(145)_ita/S5_v1.01(145)_ita.bin', size: 6291456,
      version: '1.01(145)', language: 'Italian', languages: [],
      note: 'Boots to its desktop with a "Program closed" message from Calc (KERN-EXEC 3) over it.' },
    { id: 'S5_v1.01(145)_nl', path: 'Series5/S5_v1.01(145)_nl/S5_v1.01(145)_nl.bin', size: 6291456,
      version: '1.01(145)', language: 'Dutch', languages: [],
      note: 'Boots to its desktop with a "Program closed" message from Calc (KERN-EXEC 3) over it.' },
  ] },
  '5mx': { kind: 'rom', options: [
    { id: '5mx_v1.05(260)_eng', path: 'Series5mx/5mx_v1.05(260)_eng/5mx_v1.05(260)_eng.bin', size: 16777216,
      version: '1.05(260)', language: 'English (UK)', languages: EN_UK_SCAN },
    { id: '5mx_v1.05(255)_16_eng', path: 'Series5mx/5mx_v1.05(255)_16_eng/5mx_v1.05(255)_16_eng.bin', size: 16777216,
      version: '1.05(255)', language: 'English (UK)', languages: EN_UK_SCAN,
      note: '16 MB image.' },
    { id: '5mx_v1.05(255)_10_eng', path: 'Series5mx/5mx_v1.05(255)_10_eng/5mx_v1.05(255)_10_eng.bin', size: 10485760,
      version: '1.05(255)', language: 'English (UK)', languages: EN_UK_SCAN,
      note: '10 MB image.' },
    { id: '5mx_v1.05(254)_eng', path: 'Series5mx/5mx_v1.05(254)_eng/5mx_v1.05(254)_eng.bin', size: 10018816,
      version: '1.05(254)', language: 'English', languages: [] },
    { id: '5mx_v1.05(250)_eng', path: 'Series5mx/5mx_v1.05(250)_eng/5mx_v1.05(250)_eng.bin', size: 10485760,
      version: '1.05(250)', language: 'English (UK)', languages: EN_UK_SCAN },
    { id: '5mx_v1.05(315)_fre', path: 'Series5mx/5mx_v1.05(315)_fre/5mx_v1.05(315)_fre.bin', size: 16777216,
      version: '1.05(315)', language: 'French', languages: [] },
    { id: '5mx_v1.05(292)_fre', path: 'Series5mx/5mx_v1.05(292)_fre/5mx_v1.05(292)_fre.bin', size: 10485760,
      version: '1.05(292)', language: 'French', languages: [] },
  ] },
  '5mxpro': { kind: 'os', options: [
    { id: '5mxPRO_v1.05(319)_patch_site_eng',
      path: 'Series5mxPRO/5mxPRO_v1.05(319)_patch_site_eng/5mxPRO_v1.05(319)_patch_eng.bin', size: 10799996,
      version: '1.05(319) patched', language: 'English (UK)', languages: EPOC_5MXPRO,
      note: 'The build this site has always shipped.' },
    { id: '5mxPRO_v1.05(319)_patch_eng', path: 'Series5mxPRO/5mxPRO_v1.05(319)_patch_eng/sys$rom.bin', size: 10799996,
      version: '1.05(319) patched', language: 'English (UK)', languages: EPOC_5MXPRO,
      note: "The Psion-ROM archive's copy of the patched English OS." },
    { id: '5mxPRO_v1.05(319)_patch_ger', path: 'Series5mxPRO/5mxPRO_v1.05(319)_patch_ger/sys$rom.bin', size: 11413572,
      version: '1.05(319) patched', language: 'German', languages: [] },
    { id: '5mxPRO_v1.05(319)_ger', path: 'Series5mxPRO/5mxPRO_v1.05(319)_ger/sys$rom.bin', size: 10473472,
      version: '1.05(319)', language: 'German', languages: [] },
    { id: '5mxPRO_v1.05(273)_ger', path: 'Series5mxPRO/5mxPRO_v1.05(273)_ger/sys$rom.bin', size: 10256384,
      version: '1.05(273)', language: 'German', languages: [] },
    { id: '5mxPRO_v1.05(265)_ger', path: 'Series5mxPRO/5mxPRO_v1.05(265)_ger/sys$rom.bin', size: 10248192,
      version: '1.05(265)', language: 'German', languages: [] },
  ] },
  mc218: { kind: 'rom', options: [
    { id: 'MC218_v1.05(259)_eng', path: 'MC218/MC218_v1.05(259)_eng/MC218_v1.05(259)_eng.bin', size: 12582912,
      version: '1.05(259)', language: 'English (UK)', languages: MC218_ENG },
    { id: 'MC218_v1.05(256)_eng', path: 'MC218/MC218_v1.05(256)_eng/MC218_v1.05(256)_eng.bin', size: 12582912,
      version: '1.05(256)', language: 'English', languages: [] },
    { id: 'MC218_v1.05(260)_ger', path: 'MC218/MC218_v1.05(260)_ger/MC218_v1.05(260)_ger.bin', size: 12582912,
      version: '1.05(260)', language: 'German', languages: [] },
    { id: 'MC218_v1.05(262)_fre', path: 'MC218/MC218_v1.05(262)_fre/MC218_v1.05(262)_fre.bin', size: 12582912,
      version: '1.05(262)', language: 'French', languages: [] },
  ] },
  osaris: { kind: 'rom', options: [
    { id: 'Osaris_v1.02(209)_eng', path: 'Osaris/Osaris_v1.02(209)_eng/Osaris_v1.02(209)_eng.bin', size: 8388608,
      version: '1.02(209)', language: 'English' },
    { id: 'Osaris_v1.02(209)_fre', path: 'Osaris/Osaris_v1.02(209)_fre/Osaris_v1.02(209)_fre.bin', size: 8388608,
      version: '1.02(209)', language: 'French', languages: [] },
  ] },
  series7: { kind: 'rom', options: [
    { id: 'S7_v1.05(254)_b756_eng', path: 'Series7/S7_v1.05(254)_b756_eng/S7_v1.05(254)_b756_eng.bin', size: 16777216,
      version: '1.05(254) build 756', language: 'English' },
    { id: 'S7_v1.05(254)_b754_eng', path: 'Series7/S7_v1.05(254)_b754_eng/S7_v1.05(254)_b754_eng.bin', size: 16777216,
      version: '1.05(254) build 754', language: 'English', languages: [] },
  ] },
  revo: { kind: 'rom', options: [
    { id: 'Revo_v1.06(390)_eng', path: 'Revo/Revo_v1.06(390)_eng/Revo_v1.06(390)_eng.bin', size: 8388608,
      version: '1.06(390)', language: 'English (UK)', languages: REVO_ENG },
    { id: 'Revo_v1.06(361)_eng', path: 'Revo/Revo_v1.06(361)_eng/Revo_v1.06(361)_eng.bin', size: 8388608,
      version: '1.06(361)', language: 'English (UK)', languages: REVO_ENG },
    { id: 'Revo_v1.06(352)_ger', path: 'Revo/Revo_v1.06(352)_ger/Revo_v1.06(352)_ger.bin', size: 8388608,
      version: '1.06(352)', language: 'German', languages: [] },
    { id: 'Revo_v1.06(369)_ger', path: 'Revo/Revo_v1.06(369)_ger/Revo_v1.06(369)_ger.bin', size: 8388608,
      version: '1.06(369)', language: 'German', languages: [] },
    { id: 'Revo_v1.06(391)_ger', path: 'Revo/Revo_v1.06(391)_ger/Revo_v1.06(391)_ger.bin', size: 8388608,
      version: '1.06(391)', language: 'German', languages: [] },
    { id: 'Revo_v1.06(392)_fre', path: 'Revo/Revo_v1.06(392)_fre/Revo_v1.06(392)_fre.bin', size: 8388608,
      version: '1.06(392)', language: 'French', languages: [] },
    { id: 'Revo_v1.06(401)_nl', path: 'Revo/Revo_v1.06(401)_nl/Revo_v1.06(401)_nl.bin', size: 8388608,
      version: '1.06(401)', language: 'Dutch', languages: [] },
    { id: 'Revo_v1.08(14)_chitr', path: 'Revo/Revo_v1.08(14)_chitr/Revo_v1.08(14)_chitr.bin', size: 16777216,
      version: '1.08(14)', language: 'Chinese (Traditional)', languages: [] },
  ] },
  netbook: { kind: 'os', options: [
    { id: 'netBook_v1.05(450)_patch_site_eng',
      path: 'netBook/Patched/netBook_v1.05(450)_patch_site_eng/netBook_v1.05(450)_eng.img', size: 14648032,
      version: '1.05(450) patched', language: 'English (UK)',
      note: 'The build this site has always shipped.' },
    { id: 'netBook_v1.05(450)_eng', path: 'netBook/netBook_v1.05(450)_eng/OS.IMG', size: 14651648,
      version: '1.05(450)', language: 'English (UK)', languages: [] },
    { id: 'netBook_v1.05(453)_usa', path: 'netBook/netBook_v1.05(453)_usa/OS.IMG', size: 14627072,
      version: '1.05(453)', language: 'English (USA)', languages: [] },
    { id: 'netBook_v1.05(456)_fre', path: 'netBook/netBook_v1.05(456)_fre/OS.IMG', size: 14565632,
      version: '1.05(456)', language: 'French', languages: [] },
    { id: 'netBook_v1.05(457)_ger', path: 'netBook/netBook_v1.05(457)_ger/OS.IMG', size: 14672128,
      version: '1.05(457)', language: 'German', languages: [] },
    { id: 'netBook_v1.05(462)_spa', path: 'netBook/netBook_v1.05(462)_spa/OS.IMG', size: 14586112,
      version: '1.05(462)', language: 'Spanish', languages: [] },
    { id: 'MalayBook_v1.05(281)_eng', path: 'netBook/MalayBook_v1.05(281)_eng/OS.IMG', size: 13947136,
      version: '1.05(281)', language: 'English', languages: [],
      note: 'The Malaysian schools edition.' },
    { id: 'netBook_v1.05(254)_eng', path: 'netBook/netBook_v1.05(254)_eng/OS.IMG', size: 12869888,
      version: '1.05(254)', language: 'English (UK)', languages: [] },
    { id: 'netBook_v1.05(254)_usa', path: 'netBook/netBook_v1.05(254)_usa/OS.IMG', size: 12845312,
      version: '1.05(254)', language: 'English (USA)', languages: [] },
    { id: 'netBook_v1.05(450)_patch_eng', path: 'netBook/Patched/netBook_v1.05(450)_patch_eng/OS.IMG', size: 14651648,
      version: '1.05(450) patched', language: 'English (UK)', languages: [] },
    { id: 'netBook_v1.05(450)_patch2_eng', path: 'netBook/Patched/netBook_v1.05(450)_patch2_eng/OS.IMG', size: 14648032,
      version: '1.05(450) patch 2', language: 'English (UK)', languages: [] },
    { id: 'netBook_v1.05(453)_patch_usa', path: 'netBook/Patched/netBook_v1.05(453)_patch_usa/OS.IMG', size: 14627072,
      version: '1.05(453) patched', language: 'English (USA)', languages: [] },
    { id: 'netBook_v1.05(456)_patch_fre', path: 'netBook/Patched/netBook_v1.05(456)_patch_fre/OS.IMG', size: 14565632,
      version: '1.05(456) patched', language: 'French', languages: [] },
    { id: 'netBook_v1.05(457)_patch_ger', path: 'netBook/Patched/netBook_v1.05(457)_patch_ger/OS.IMG', size: 14672128,
      version: '1.05(457) patched', language: 'German', languages: [] },
    { id: 'netBook_v1.05(462)_patch_spa', path: 'netBook/Patched/netBook_v1.05(462)_patch_spa/OS.IMG', size: 14586112,
      version: '1.05(462) patched', language: 'Spanish', languages: [] },
    { id: 'MalayBook_v1.05(281)_patch_eng', path: 'netBook/Patched/MalayBook_v1.05(281)_patch_eng/OS.IMG', size: 13947136,
      version: '1.05(281) patched', language: 'English', languages: [] },
  ] },
};

const storageKey = (deviceId: string) => `psion-rom-${deviceId}`;

/** The choices for a device, default first; empty when it has only one ROM. */
export function romOptions(deviceId: string): RomOption[] {
  const set = ROM_CATALOG[deviceId];
  return set && set.options.length > 1 ? set.options : [];
}

/** The ROM the device will boot: the stored choice, or the default. */
export function selectedRom(deviceId: string): RomOption | null {
  const set = ROM_CATALOG[deviceId];
  if (!set) return null;
  let id: string | null = null;
  try { id = localStorage.getItem(storageKey(deviceId)); } catch { /* storage blocked */ }
  return set.options.find(o => o.id === id) ?? set.options[0];
}

/** Remember a choice; null (or the default's id) goes back to the default. */
export function setSelectedRom(deviceId: string, romId: string | null): void {
  const set = ROM_CATALOG[deviceId];
  try {
    if (!set || romId == null || romId === set.options[0].id) localStorage.removeItem(storageKey(deviceId));
    else localStorage.setItem(storageKey(deviceId), romId);
  } catch { /* private mode — the choice lasts this session only */ }
}

/**
 * '' on the default ROM, otherwise the chosen ROM's id. Tags the things
 * that only make sense for one ROM (the saved session, the language
 * index), so the default keeps the keys it always had.
 */
export function romStateTag(deviceId: string): string {
  const set = ROM_CATALOG[deviceId];
  const rom = selectedRom(deviceId);
  return !set || !rom || rom.id === set.options[0].id ? '' : rom.id;
}

/** URL of the image the device boots from (for 'os' devices, its bootloader). */
export function romUrlFor(profile: { id: string; romFilename: string }, baseUrl: string): string {
  const set = ROM_CATALOG[profile.id];
  const path = set?.kind === 'rom' ? selectedRom(profile.id)!.path : profile.romFilename;
  return `${baseUrl}roms/${path}`;
}

/** Path under roms/ of the OS image an 'os' device loads off its card. */
export function osImagePath(deviceId: string): string | null {
  return ROM_CATALOG[deviceId]?.kind === 'os' ? selectedRom(deviceId)!.path : null;
}

/** The option a romStateTag names ('' is the default). */
export function romForTag(deviceId: string, tag: string): RomOption | null {
  const set = ROM_CATALOG[deviceId];
  if (!set) return null;
  return (tag && set.options.find(o => o.id === tag)) || set.options[0];
}

/**
 * The language names to show for a device's ROM, given what the emulator
 * reports. `tag` is the romStateTag the machine was loaded with (default:
 * the current choice). See the header for the three cases.
 */
export function romLanguageNames(deviceId: string, reported: string[],
                                 tag: string = romStateTag(deviceId)): string[] {
  const listed = romForTag(deviceId, tag)?.languages;
  if (listed === undefined) return reported;
  return listed.length > 1 && listed.length === reported.length ? listed : [];
}

/** False when the ROM offers no language choice at all. */
export function romHasLanguageChoice(deviceId: string,
                                     tag: string = romStateTag(deviceId)): boolean {
  const listed = romForTag(deviceId, tag)?.languages;
  return listed === undefined || listed.length > 1;
}
