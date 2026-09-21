// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect } from 'react';
import { acquireUart, type LeaseOwner } from '../lib/plp/uartLease';

// Claim the emulated serial port for as long as a dialog is open.
//
// The desktop app runs a background drive sync that wants the same UART
// continuously. Without arbitration it and a dialog would both drive the port,
// and because PlpClient's keepalive is a global single slot the second client
// silently replaces the first's — starving a live session rather than merely
// competing for bandwidth.
//
// Tied to component lifetime rather than to each serialAttachHost call, because
// these dialogs attach from several places and tear down along several paths;
// mount and unmount are the two moments that unambiguously mean "this dialog
// has the port" and "it no longer does".
//
// In the browser there is nothing to arbitrate with: the background sync only
// exists in the desktop shell, so the lease is always granted immediately and
// this costs a promise.
export function useUartLease(uartIndex: number, owner: LeaseOwner): void {
  useEffect(() => {
    if (uartIndex < 0) return;
    let released = false;
    let release: (() => void) | null = null;

    // A dialog outranks background work, so this always succeeds — but it is
    // awaited rather than assumed, so the background holder has finished
    // quiescing before the dialog starts writing to the same port.
    void acquireUart(uartIndex, owner).then((lease) => {
      if (!lease) return;
      if (released) { lease.release(); return; }
      release = () => lease.release();
    });

    return () => {
      released = true;
      release?.();
    };
  }, [uartIndex, owner]);
}
