// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The application menu.
//
// Not optional on macOS: a frameless window with no menu bar there has no
// Cmd+Q, no Cmd+C, and no way to reach anything — the app reads as broken.
// On Windows a frameless window shows no menu bar at all, but the menu is
// still set because its accelerators keep working, which is what gives both
// platforms the same shortcuts from one definition.
//
// The device list comes from the renderer (the WASM module is the single
// source of truth for it), so the menu is rebuilt whenever it publishes.

import { Menu, app, shell, type BrowserWindow, type MenuItemConstructorOptions } from 'electron';
import { CHANNELS, type DeviceMenuEntry, type HostCommand } from '../ipc/contract';

let devices: DeviceMenuEntry[] = [];
let currentDeviceId: string | null = null;
let target: BrowserWindow | null = null;

function send(cmd: HostCommand): void {
  if (target && !target.isDestroyed()) target.webContents.send(CHANNELS.command, cmd);
}

function deviceItems(): MenuItemConstructorOptions[] {
  const usable = devices.filter((d) => d.supported && !d.hidden);
  if (!usable.length) {
    return [{ label: 'Loading devices…', enabled: false }];
  }
  return usable.map((d) => ({
    label: d.name,
    type: 'radio',
    checked: d.id === currentDeviceId,
    click: () => send({ kind: 'load-device', deviceId: d.id }),
  }));
}

function template(): MenuItemConstructorOptions[] {
  const isMac = process.platform === 'darwin';

  const appMenu: MenuItemConstructorOptions[] = isMac
    ? [{
        label: app.name,
        submenu: [
          { role: 'about' },
          { type: 'separator' },
          { role: 'hide' }, { role: 'hideOthers' }, { role: 'unhide' },
          { type: 'separator' },
          // Quit goes through before-quit, which defers for the final save.
          { role: 'quit' },
        ],
      }]
    : [];

  return [
    ...appMenu,
    {
      label: 'Device',
      submenu: [
        {
          label: 'Switch Device…',
          accelerator: 'CmdOrCtrl+K',
          click: () => send({ kind: 'open-switcher' }),
        },
        { type: 'separator' },
        ...deviceItems(),
        { type: 'separator' },
        {
          label: 'Reset Device',
          accelerator: 'CmdOrCtrl+R',
          click: () => send({ kind: 'reset-device' }),
        },
      ],
    },
    {
      label: 'Drives',
      submenu: [
        {
          label: 'Shared Card Folder…',
          accelerator: 'CmdOrCtrl+D',
          click: () => send({ kind: 'open-drives' }),
        },
      ],
    },
    {
      label: 'View',
      submenu: [
        {
          label: 'Show Device Keys',
          accelerator: 'F1',
          click: () => send({ kind: 'toggle-keys' }),
        },
        { type: 'separator' },
        { role: 'togglefullscreen' },
        { role: 'toggleDevTools' },
      ],
    },
    // Edit is needed for the clipboard roles to work at all on macOS —
    // pasteFromClipboard() in useEmulator reads navigator.clipboard, and
    // Cmd+V has to be bound to something for the OS to route it.
    {
      label: 'Edit',
      submenu: [
        { role: 'cut' }, { role: 'copy' }, { role: 'paste' }, { role: 'selectAll' },
      ],
    },
    {
      label: 'Window',
      submenu: isMac
        ? [{ role: 'minimize' }, { role: 'zoom' }, { type: 'separator' }, { role: 'front' }]
        : [{ role: 'minimize' }, { role: 'close' }],
    },
    {
      role: 'help',
      submenu: [{
        label: 'Psion Emulator on the Web',
        click: () => void shell.openExternal('https://joehaines.com/psion'),
      }],
    },
  ];
}

export function attach(w: BrowserWindow): void {
  target = w;
  rebuild();
  // The menu bar would otherwise reappear over the device on Windows/Linux.
  // Accelerators keep firing regardless, which is the point of setting it.
  if (process.platform !== 'darwin') {
    w.setMenuBarVisibility(false);
    w.setAutoHideMenuBar(true);
  }
}

export function rebuild(): void {
  Menu.setApplicationMenu(Menu.buildFromTemplate(template()));
}

export function setDevices(list: DeviceMenuEntry[]): void {
  devices = list;
  rebuild();
}

export function setCurrentDevice(id: string | null): void {
  currentDeviceId = id;
  const name = devices.find((d) => d.id === id)?.name;
  if (target && !target.isDestroyed()) {
    target.setTitle(name ? `${name} — Psion Emulator` : 'Psion Emulator');
  }
  rebuild();
}
