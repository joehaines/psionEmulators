// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Easter eggs, cheats and hidden extras found in the ROMs this emulator
// runs, and the alternative boots some machines offer. One list feeds
// three places: the one-line hint under a running device's control bar
// (EmulatorView), the Easter eggs page (#/eggs, components/EasterEggs.tsx)
// and the alternative-boot buttons.
//
// Every "how" here was either watched working in the emulator or read out
// of the ROM's own code; `verified` says which. docs/rom-audit.md has the
// addresses and the evidence.

export type EggKind = 'credits' | 'cheat' | 'hidden';

export interface EasterEgg {
  id: string;
  title: string;
  kind: EggKind;
  // Device ids (core/device_registry.cpp) the egg can be triggered on.
  devices: string[];
  // Devices whose ROM carries the egg but where it can't be set off — shown
  // on the page so the list is complete, never hinted on the device.
  dormantOn?: { devices: string[]; why: string };
  // One line for under the control bar. Keep it short: it sits under the
  // buttons while the machine runs.
  hint: string;
  // The full recipe, for the Easter eggs page. `Backticks` mark what to
  // type or press; the page shows those as code.
  steps: string[];
  // What you get, and anything worth knowing about it.
  about: string;
  // Only hint while the 5mx Pro / netBook bootloader is still up (the
  // command goes away once the OS card has been read).
  bootloaderOnly?: boolean;
  // true: seen working in this emulator. false: read out of the ROM's code
  // but not exercised end to end here.
  verified: boolean;
}

export const EASTER_EGGS: EasterEgg[] = [
  {
    id: 'series3c-bogan',
    title: '“Jerusalem” and the rogues’ gallery',
    kind: 'credits',
    devices: ['series3c', 'series3mx'],
    hint: 'Easter egg: open Info › About (Psion+V) on the System screen and type !Mrs T Bogan!',
    steps: [
      'On the System screen press `Psion+V` (Info menu › “About Series 3c”, or “About Series 3mx”).',
      'With the About box showing, type `!Mrs T Bogan!` — both exclamation marks, capital M, T and B.',
      'Wait: the machine plays “Jerusalem” on its buzzer, which takes about twelve seconds.',
      'A credits table follows, with a scroll bar for the rest of the team.',
    ],
    about:
      'The credits list the development team as a charge sheet, with the columns ' +
      '“Accused”, “Main Charges” and “Other Offences” (“ROM builds, Duty pedant”, ' +
      '“Thought policing”, …). The trigger phrase and the tune are stored in the ROM ' +
      'with every byte shifted by three, so a plain search of the image never finds them.',
    verified: true,
  },
  {
    id: 'bombs-cheat',
    title: 'Bombs cheat mode',
    kind: 'cheat',
    devices: ['series5', '5mx', '5mxpro', 'mc218'],
    dormantOn: {
      devices: ['geofox', 'osaris', 'series7', 'netbook'],
      why:
        'the same Bombs is in these ROMs, but their silkscreens have no Infrared ' +
        'icon (or no sidebar at all), so the five-key sequence can’t be entered.',
    },
    hint: 'Easter egg: in Bombs, tap the sidebar icons bottom to top (Zoom out, Zoom in, Infrared, Clipboard, Menu)',
    steps: [
      'Tap Extras on the silkscreen strip under the screen and start Bombs.',
      'Tap the icons on the sidebar left of the screen from the bottom up: Zoom out, Zoom in, Infrared, Clipboard, Menu.',
      '“Cheat mode on” appears in the corner.',
      'Now when you step on a bomb, tap the clock (within the first hour of the game): the bombs are covered up again and you carry on playing.',
    ],
    about:
      'Bombs is written in OPL. Its event loop receives the sidebar icons as key ' +
      'codes 10000–10004 and packs the last five into a hex number; 0x43210, the ' +
      'icons in reverse order, turns the cheat on. It resets at every new game.',
    verified: true,
  },
  {
    id: '5mxpro-about',
    title: 'Bootloader credits',
    kind: 'credits',
    devices: ['5mxpro'],
    hint: 'There is a bootloader easter egg, type "about" to see it',
    steps: [
      'Load the Series 5mx Pro and wait for the bootloader screen, before inserting the OS card.',
      'Type `about`.',
    ],
    about:
      'The 5mx Pro’s flash bootloader watches the keyboard for the word “about” ' +
      'and shows who built it. Once the OS card has been read the bootloader has ' +
      'handed over and the command is gone.',
    bootloaderOnly: true,
    verified: true,
  },
  {
    id: 'workabout-tests',
    title: 'Factory test programs in ROM',
    kind: 'hidden',
    devices: [],
    dormantOn: {
      devices: ['workabout', 'workaboutmx', 'hc120'],
      why:
        'these are in the ROM drive but have no icon on the System screen, and we ' +
        'haven’t yet confirmed a way to start them from the running machine.',
    },
    hint: '',
    steps: [
      'Workabout: LCDTEST.IMG, BEEPTEST.IMG and SYS$SOAK.IMG (a soak test).',
      'Workabout MX: LCDTEST.IMG, SYS$SOAK.IMG, IRSNIFF.APP (an infrared sniffer), IRDEM, FTPDEM, DEMMAN and a TCP/IP sockets log dumper.',
      'HC120: TTEST.IMG, a serial terminal tester, and BATCHK.IMG.',
    ],
    about: 'Programs Psion shipped in the ROM for manufacturing and support rather than for users.',
    verified: false,
  },
  {
    id: 'bist',
    title: 'Built-in self test',
    kind: 'hidden',
    devices: [],
    dormantOn: {
      devices: ['revo', 'conan', 'conanv001'],
      why:
        'the ROM starts the self test only when the factory has flagged the ' +
        'machine for it, and that flag isn’t something the emulator sets yet.',
    },
    hint: '',
    steps: [
      'Revo: bist.exe with manual and automatic test modes, E2PROM and serial tests.',
      'Conan: BIST.EXE, whose menu can rewrite the machine’s language and keyboard index, and a Bluetooth loopback test that sends “Symbian EPOC Bluetooth to Bluetooth test 20/07/99 Hello World!!”.',
    ],
    about: 'A factory test harness left in the shipping ROMs.',
    verified: false,
  },
  {
    id: 'netbook-bootloader-tests',
    title: 'The netBook bootloader’s test suite',
    kind: 'hidden',
    devices: [],
    dormantOn: {
      devices: ['netbook'],
      why:
        'the bootloader is itself a small EPOC ROM with these in Z:\\Test, but its ' +
        'boot shell never starts them.',
    },
    hint: '',
    steps: [
      'T_CALIB (touch calibration), T_SERIAL (serial loopback and handshaking), T_HAL (“Turn the red one on … Flashy flashy”), T_INF (prints the language and keyboard index), T_HW (an ASIC register poker) and T_pccdsr (a CompactFlash stress test).',
    ],
    about: 'Leftovers from bringing up the netBook hardware.',
    verified: false,
  },
];

// Eggs to hint on a device, in list order.
export function eggsForDevice(deviceId: string | null): EasterEgg[] {
  if (!deviceId) return [];
  return EASTER_EGGS.filter(e => e.hint && e.devices.includes(deviceId));
}

// ── Alternative boots ────────────────────────────────────────────────
// Machines whose bootloader reads the OS off a card can be handed a
// different OS image on it. `variant` is the osCardSpec() variant in
// hooks/useEmulator.ts.
export interface AltBoot {
  variant: string;
  label: string;
  title: string;
}

export const ALT_BOOTS: Record<string, AltBoot[]> = {
  '5mxpro': [
    { variant: 'eshell', label: 'Boot ESHELL',
      title: 'Boot the ESHELL ROM (EPOC’s text console) instead of the stock 5mx Pro OS' },
  ],
  netbook: [
    { variant: 'eshell', label: 'Boot ESHELL',
      title: 'Boot the ESHELL ROM (EPOC’s text console) instead of the stock netBook OS' },
    { variant: 'quartz', label: 'Boot Quartz',
      title: 'Boot the netBook build of Quartz, EPOC’s pen interface and the ancestor of UIQ' },
  ],
};

export const EASTER_EGGS_HASH = '#/eggs';
