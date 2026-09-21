// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The case and keyboard keys that have no natural host-keyboard equivalent,
// as data.
//
// The desktop shell's screen-only window draws no case, so every key that
// EmulatorView renders as an on-screen button has to be reachable some other
// way. This table is what its key strip is built from.
//
// The codes mirror EmulatorView's own EPOC_* constants, which in turn mirror
// lib/keymap.ts, so an on-screen button emits exactly the event the physical
// key would. EmulatorView still carries its own copy inline; folding it onto
// this module is deliberately deferred so the web build's rendering is
// untouched by the desktop work. That duplication is a known debt, not an
// oversight — if you change a code here, change it there too.

/** EPOC scan codes. EStdKey* values from the ROM's keyboard driver. */
export const EPOC_KEY = {
  BACKSPACE: 1,
  TAB: 2,
  ENTER: 3,
  ESCAPE: 4,
  LEFT: 14,
  RIGHT: 15,
  UP: 16,
  DOWN: 17,
  SHIFT: 18,
  /** EStdKeyRightAlt — the SIBO ⊔ (Psion logo) key, left of Menu. */
  PSION: 21,
  /** EStdKeyLeftFunc — the Windermere clamshells' blue Fn modifier. */
  FN: 24,
  /** EStdKeyCapsLock — the SIBO ◆ (diamond) key, right of Menu. */
  DIAMOND: 26,
  /** EStdKeyXXX, repurposed as the MC400 trackpad click bit. */
  TRACKPAD: 120,
  MENU: 148,
  DICT_PLAY: 156,
  DICT_STOP: 157,
  DICT_RECORD: 158,
} as const;

export interface DeviceKey {
  label: string;
  /** Tooltip, and the accessible name. */
  title: string;
  code: number;
  /** A host key that already does this, mentioned in the tooltip so the
   *  strip teaches its own shortcuts instead of being the only route. */
  hostHint?: string;
}

export interface KeyGroup {
  id: string;
  /** Shown as a small caption above the group when several are present. */
  label: string;
  keys: DeviceKey[];
}

/**
 * Which machine this is, in the terms the key tables care about.
 *
 * Derived from the same signals EmulatorView uses: the family from the
 * profile's display name (the ROM is the source of truth for it) and the
 * per-model lists by device id.
 */
export interface KeyContext {
  deviceId: string | null;
  deviceName: string | null;
}

function isSibo(name: string | null): boolean {
  if (!name) return false;
  return name.includes('Series 3') || name.includes('Pocket Book') || name.includes('Siena');
}

function isOrganiser2(name: string | null): boolean {
  return !!name?.includes('Organiser');
}

function isMc400(name: string | null): boolean {
  return !!name?.includes('MC400');
}

// Windermere-family clamshells with a physical Fn key. Series 7 / netBook is
// excluded on purpose: it has a full F1-F10 row and no Fn modifier.
const FN_DEVICES = new Set(['5mx', '5mxpro', 'series5', 'mc218', 'osaris', 'revo', 'conan']);

// EPOC clamshells with Record / Play / Stop on the case. Revo and Osaris have
// no recorder hardware, even though Revo's matrix would accept the codes.
const DICTAPHONE_DEVICES = new Set(['series5', '5mx', '5mxpro', 'mc218']);

const NAV_KEYS: DeviceKey[] = [
  { label: '←', title: 'Arrow left', code: EPOC_KEY.LEFT, hostHint: '←' },
  { label: '↑', title: 'Arrow up', code: EPOC_KEY.UP, hostHint: '↑' },
  { label: '↓', title: 'Arrow down', code: EPOC_KEY.DOWN, hostHint: '↓' },
  { label: '→', title: 'Arrow right', code: EPOC_KEY.RIGHT, hostHint: '→' },
];

/**
 * The key groups worth offering for a machine, most useful first.
 *
 * Arrow keys and Esc/Tab/Shift are omitted for every machine that has a
 * keyboard: a host keyboard already has them, and keymap.ts routes them.
 * What survives is the set a PC keyboard genuinely cannot express.
 */
export function keyGroupsFor(ctx: KeyContext): KeyGroup[] {
  const { deviceId, deviceName } = ctx;
  const groups: KeyGroup[] = [];

  if (isOrganiser2(deviceName)) {
    // The Organiser II is mostly keypad, and its six-by-six matrix has no
    // host analogue for the command keys. The letters and digits come from
    // the host keyboard; these five do not.
    groups.push({
      id: 'organiser',
      label: 'Organiser',
      keys: [
        { label: 'ON', title: 'On / Clear', code: EPOC_KEY.ESCAPE, hostHint: 'Esc' },
        { label: 'MODE', title: 'Mode', code: EPOC_KEY.MENU, hostHint: 'Tab' },
        { label: 'EXE', title: 'Execute', code: EPOC_KEY.ENTER, hostHint: 'Enter' },
        { label: 'DEL', title: 'Delete', code: EPOC_KEY.BACKSPACE, hostHint: 'Backspace' },
        { label: 'SHIFT', title: 'Shift', code: EPOC_KEY.SHIFT, hostHint: 'Shift' },
      ],
    });
    groups.push({ id: 'nav', label: 'Arrows', keys: NAV_KEYS });
    return groups;
  }

  if (isSibo(deviceName)) {
    // The app-button bar below the screen already renders in chromeless
    // mode, so only the three keyboard specials are missing.
    groups.push({
      id: 'sibo',
      label: 'Keys',
      keys: [
        { label: 'Menu', title: 'Menu key', code: EPOC_KEY.MENU, hostHint: 'Esc' },
        { label: '⊔', title: 'Psion key', code: EPOC_KEY.PSION, hostHint: 'Alt' },
        { label: '◆', title: 'Diamond key', code: EPOC_KEY.DIAMOND, hostHint: 'Caps Lock' },
      ],
    });
    return groups;
  }

  const main: DeviceKey[] = [
    { label: 'Menu', title: 'Menu key', code: EPOC_KEY.MENU, hostHint: 'Esc' },
  ];
  if (deviceId && FN_DEVICES.has(deviceId)) {
    main.push({ label: 'Fn', title: 'Fn key', code: EPOC_KEY.FN, hostHint: 'Left Meta' });
  }
  groups.push({ id: 'main', label: 'Keys', keys: main });

  if (isMc400(deviceName)) {
    groups.push({
      id: 'trackpad',
      label: 'Trackpad',
      keys: [{ label: 'Click', title: 'Trackpad click', code: EPOC_KEY.TRACKPAD }],
    });
  }

  if (deviceId && DICTAPHONE_DEVICES.has(deviceId)) {
    groups.push({
      id: 'dictaphone',
      label: 'Recorder',
      keys: [
        { label: '● Rec', title: 'Record', code: EPOC_KEY.DICT_RECORD },
        { label: '▶ Play', title: 'Play', code: EPOC_KEY.DICT_PLAY },
        { label: '■ Stop', title: 'Stop', code: EPOC_KEY.DICT_STOP },
      ],
    });
  }

  return groups;
}

/**
 * Machines whose screen alone is not enough to use them, so the desktop
 * shell defaults to the skinned view instead of screen-only.
 *
 * Just the Organiser II: a two-line sixteen-character LCD above a
 * thirty-six-key calculator pad, where the pad IS the machine. Everything
 * else is a clamshell whose keyboard the host keyboard stands in for.
 */
export function prefersSkinnedView(ctx: KeyContext): boolean {
  return isOrganiser2(ctx.deviceName);
}
