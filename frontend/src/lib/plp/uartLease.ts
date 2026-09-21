// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Who currently owns the emulated serial port.
//
// Five things in this app want the same UART: the Remote Link dialog, the
// printer capture, the infrared bridge, the simulated modem, and app delivery
// from the library. Until now that was fine, because all five are modal — a
// user opens one at a time.
//
// The desktop drive sync breaks that assumption. It wants the port
// continuously, in the background, for as long as the app is open. So the port
// needs an owner, and the background holder needs to get out of the way the
// moment a user opens a dialog that wants it.
//
// There is a second, less obvious reason a single owner is mandatory:
// PlpClient.start() installs a keepalive through setPumpKeepAlive, which is a
// GLOBAL single slot. Two clients running at once do not merely compete for
// bytes — the second silently replaces the first's keepalive, and the first's
// session then starves and dies.
//
// Deliberately not a queue. A lease is granted or refused immediately; a
// caller that must wait polls or listens. A queue would mean a dialog's Connect
// button hanging on a background job's timeout.

export type LeaseOwner =
  | 'remote-link' | 'printer' | 'infrared' | 'modem' | 'app-delivery' | 'desktop-mount';

/** Higher wins. A user-facing dialog always outranks background work. */
export const LEASE_PRIORITY: Record<LeaseOwner, number> = {
  'remote-link': 10,
  printer: 10,
  infrared: 10,
  modem: 10,
  // App delivery is user-initiated but unattended once started, so it yields
  // to a dialog and outranks the background mount.
  'app-delivery': 5,
  'desktop-mount': 0,
};

export interface UartLease {
  readonly uart: number;
  readonly owner: LeaseOwner;
  /** True until released or preempted. */
  readonly active: boolean;
  release(): void;
}

interface Held {
  uart: number;
  owner: LeaseOwner;
  priority: number;
  onPreempt?: () => void | Promise<void>;
  lease: UartLease;
  released: boolean;
}

const held = new Map<number, Held>();
const waiters = new Set<(uart: number) => void>();

function notifyFree(uart: number): void {
  for (const w of [...waiters]) {
    try { w(uart); } catch { /* a listener throwing must not wedge the port */ }
  }
}

export function uartHolder(uart: number): LeaseOwner | null {
  return held.get(uart)?.owner ?? null;
}

export interface AcquireOptions {
  /** Defaults to the owner's standing priority. */
  priority?: number;
  /**
   * Called when a higher-priority owner wants the port. Must fully stop using
   * it — for a PlpClient that means quiescing so the session stays adoptable,
   * not tearing it down.
   *
   * The lease is revoked whether or not this resolves; it is a chance to stop
   * cleanly, not a veto.
   */
  onPreempt?: () => void | Promise<void>;
}

/**
 * Take the port, or return null if someone with equal or higher standing has it.
 *
 * A lower-priority holder is preempted: its onPreempt is invoked and its lease
 * goes inactive before this resolves, so the new owner never overlaps with it.
 */
export async function acquireUart(
  uart: number,
  owner: LeaseOwner,
  opts: AcquireOptions = {},
): Promise<UartLease | null> {
  const priority = opts.priority ?? LEASE_PRIORITY[owner];
  const current = held.get(uart);

  if (current && !current.released) {
    if (current.priority >= priority) return null;
    // Preempt. Awaited so the old owner has actually stopped before the new one
    // starts writing to the same UART — overlapping two PLP clients is how the
    // keepalive slot gets stolen and a session dies.
    try { await current.onPreempt?.(); }
    catch (err) { console.warn(`[psion] ${current.owner} did not yield cleanly:`, err); }
    current.released = true;
    held.delete(uart);
  }

  const record: Held = {
    uart, owner, priority, onPreempt: opts.onPreempt,
    released: false,
    lease: {
      uart, owner,
      get active() { return !record.released; },
      release() {
        if (record.released) return;
        record.released = true;
        if (held.get(uart) === record) {
          held.delete(uart);
          notifyFree(uart);
        }
      },
    },
  };
  held.set(uart, record);
  return record.lease;
}

/**
 * Listen for a port becoming free, so a preempted background holder can take it
 * back when the dialog that displaced it closes.
 */
export function onUartFree(cb: (uart: number) => void): () => void {
  waiters.add(cb);
  return () => { waiters.delete(cb); };
}

/**
 * Drop every lease. For teardown and for tests — leaking a lease between test
 * cases would make the next one refuse the port for no visible reason.
 */
export function resetUartLeases(): void {
  for (const record of [...held.values()]) record.released = true;
  held.clear();
}
