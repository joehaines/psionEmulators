// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Real sockets for the emulated Psion LX's Wi-Fi.
//
// The emulator core runs the guest's network itself (core/virtnet.h): it
// answers ARP and DHCP, and it ends every TCP connection the guest opens in
// a TCP endpoint of its own, so what reaches this file is the plain request
// "open a byte stream to ip:port" or "send this datagram". That is the NAT
// half of a home router, done with Node's net and dgram. Addresses arrive as
// unsigned 32-bit IPv4 numbers. 10.0.2.2 is the router, which for TCP and
// UDP means this machine (127.0.0.1); UDP to 10.0.2.3:53 goes to the system
// resolver.
//
// The renderer is the only caller and its input is validated like any other
// IPC: bad ids, ports and sizes are dropped, and the number of open flows is
// capped so a runaway guest cannot exhaust the host's sockets.

import { BrowserWindow, ipcMain } from 'electron';
import { CHANNELS } from '../ipc/contract';
import { NetHost, parse } from './netHost';

let host: NetHost | null = null;

export function registerNet(target: () => BrowserWindow | null): void {
  host = new NetHost((batch) => {
    const w = target();
    if (w && !w.isDestroyed()) w.webContents.send(CHANNELS.netEvents, batch);
  });
  ipcMain.on(CHANNELS.netSend, (_e, commands: unknown) => {
    if (!Array.isArray(commands) || commands.length > 4096) return;
    for (const raw of commands) {
      const c = parse(raw);
      if (c) host?.handle(c);
    }
  });
}

/** The window went away (reload, quit): nothing on the other end any more. */
export function resetNet(): void {
  host?.reset();
}
