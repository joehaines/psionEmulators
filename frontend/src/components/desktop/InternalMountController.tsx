// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { EmulatorControls } from '../../hooks/useEmulator';
import type { DeviceProfile } from '../../types/emulator';
import { requireHost } from '../../lib/desktop/host';
import { PlpClient } from '../../lib/plp/client-spec';
import { acquireUart, onUartFree, type UartLease } from '../../lib/plp/uartLease';
import { applyMirror, planMirror } from '../../lib/hostsync/mirror';
import { createDeviceFs, defaultMirrorRoot } from '../../lib/hostsync/deviceFsPlp';
import {
  DEFAULT_MIRROR_POLICY, adoptMirrorState, createMirrorState,
  type HostFile, type HostFs, type MirrorState,
} from '../../lib/hostsync/types';
import { adoptIndex, deviceNameFor, hostNameForNew, type NameIndex }
  from '../../lib/extdrive/nameIndex';

// Headless: keeps a host folder in step with the machine's own internal drive,
// over the emulated Remote Link cable.
//
// ── What this is ──
//
// A folder sync with visible latency. Measured against a real Series 5mx
// through the harness: 0.81 KB/s (see docs/desktop-drive-sync.md). A 4 KB note
// crosses in five seconds; a megabyte takes twenty minutes. So the status it
// reports says "synced a moment ago", never "mounted", and the folder is meant
// to be small — the shared card is the road for anything bulky.
//
// ── The three things that make it awkward ──
//
// 1. The port is shared. Four dialogs and the app library all want the same
//    UART, and PlpClient's keepalive is a global single slot, so two clients at
//    once kill each other's session. This holds the lowest-priority lease and
//    yields the moment a dialog wants it.
//
// 2. The R5 link server does not forgive. On Windermere and SA-1100 machines it
//    goes dormant after an ungraceful session loss and only comes back when
//    Remote Link is toggled OFF and ON *on the device*. So the session is
//    parked, never torn down, and a failure to connect with no bytes seen stops
//    retrying rather than hammering something that cannot answer.
//
// 3. The device cannot be watched. RFSV has no change notification, so device
//    changes are found by polling — one listing per cycle, which at ~0.5 s a
//    time is affordable at this cadence and would not be at a shorter one.

/** How long after the machine is up before the cable is plugged in. */
const SETTLE_MS = 13_000;
/** Poll cadence for device-side changes. */
const POLL_MS = 15_000;
/** Give up on a connect after this long. */
const CONNECT_TIMEOUT_MS = 60_000;
/** Stop after this many failed adoptions rather than thrashing a dormant server. */
const MAX_ADOPT_FAILURES = 2;

export type SyncPhase =
  | 'off'            // no folder, or this machine cannot link
  | 'connecting'
  | 'idle'           // connected, nothing to do
  | 'syncing'
  | 'paused'         // a dialog has the cable
  | 'dormant'        // the link server needs toggling on the device
  | 'error';

export interface InternalMountStatus {
  phase: SyncPhase;
  /** One line, ready for the status pill. */
  summary: string;
  lastSyncedAt: number | null;
  fileCount: number;
  /** Set when there is something the user must do on the device. */
  actionable: boolean;
  warnings: string[];
}

interface Props {
  controls: EmulatorControls;
  profile: DeviceProfile | null;
  onStatus?(status: InternalMountStatus): void;
}

/** Machines whose ROM speaks a protocol our client can talk. */
function canLink(profile: DeviceProfile | null): boolean {
  if (!profile) return false;
  return (profile.remoteLinkUart ?? -1) >= 0 && (profile.linkProtocol ?? 0) > 0;
}

export default function InternalMountController({ controls, profile, onStatus }: Props) {
  const host = requireHost();
  const { state, currentDeviceId } = controls;

  const [status, setStatus] = useState<InternalMountStatus>({
    phase: 'off', summary: 'not set up', lastSyncedAt: null,
    fileCount: 0, actionable: false, warnings: [],
  });

  const statusRef = useRef(status);
  const clientRef = useRef<PlpClient | null>(null);
  const leaseRef = useRef<UartLease | null>(null);
  const mirrorRef = useRef<MirrorState | null>(null);
  const indexRef = useRef<NameIndex | null>(null);
  const chainRef = useRef<Promise<unknown>>(Promise.resolve());
  const adoptFailuresRef = useRef(0);
  const stoppedRef = useRef(false);

  const publish = useCallback((next: Partial<InternalMountStatus>) => {
    const merged = { ...statusRef.current, ...next };
    statusRef.current = merged;
    setStatus(merged);
    onStatus?.(merged);
  }, [onStatus]);

  const enqueue = useCallback(<T,>(work: () => Promise<T>): Promise<T | undefined> => {
    const next = chainRef.current.catch(() => undefined).then(async () => {
      try { return await work(); } catch (err) {
        console.error('[psion] drive sync:', err);
        publish({
          phase: 'error',
          summary: `sync failed — ${err instanceof Error ? err.message : String(err)}`,
        });
        return undefined;
      }
    });
    chainRef.current = next;
    return next;
  }, [publish]);

  const uart = profile?.remoteLinkUart ?? -1;
  const linkProtocol = profile?.linkProtocol ?? 0;
  const root = defaultMirrorRoot(linkProtocol);
  const stateKey = `sync.mirror.${currentDeviceId ?? 'none'}`;
  const indexKey = `sync.names.${currentDeviceId ?? 'none'}`;

  // ── The host side of the mirror, over the bridge ────────────────────────
  // Memoised: rebuilt every render it would change syncOnce's identity every
  // render, and the poll interval below would be torn down and re-armed each
  // time — so the cadence would depend on how often React happened to render.
  const hostFs: HostFs | null = useMemo(() => {
    const mounts = host.mounts;
    if (!mounts) return null;
    return {
      list: async (): Promise<HostFile[]> => {
        const entries = await mounts.list('internal');
        return entries.filter((e) => !e.isDir)
          .map((e) => ({ rel: e.rel, size: e.size, mtimeMs: e.mtimeMs }));
      },
      read: (rel) => mounts.read('internal', rel),
      write: (rel, data, mtimeMs) => mounts.write('internal', rel, data, mtimeMs),
      remove: (rel) => mounts.remove('internal', rel),
      mkdirp: (rel) => mounts.mkdirp('internal', rel),
    };
  }, [host]);

  /** Park the session without tearing it down — see note 2 at the top. */
  const yieldPort = useCallback(async () => {
    const client = clientRef.current;
    publish({ phase: 'paused', summary: 'paused — the cable is in use' });
    if (client) {
      // quiesce, NOT stop: the R5 link server never recovers from a Disc, so
      // the session has to stay adoptable.
      try { await client.quiesce(4000); } catch { /* best effort */ }
    }
  }, [publish]);

  const teardown = useCallback(() => {
    const client = clientRef.current;
    clientRef.current = null;
    if (client) {
      try { void client.releaseQuiesced(); } catch { /* best effort */ }
      try { client.stop(); } catch { /* best effort */ }
    }
    leaseRef.current?.release();
    leaseRef.current = null;
  }, []);

  /** Connect, or explain why not. */
  const connect = useCallback(async (): Promise<PlpClient | null> => {
    if (clientRef.current) return clientRef.current;
    if (uart < 0) return null;

    const lease = await acquireUart(uart, 'desktop-mount', { onPreempt: yieldPort });
    if (!lease) {
      publish({ phase: 'paused', summary: 'paused — the cable is in use' });
      return null;
    }
    leaseRef.current = lease;

    if (!controls.serialIsAttached(uart) && !controls.serialAttachHost(uart)) {
      lease.release();
      leaseRef.current = null;
      publish({ phase: 'error', summary: `could not attach to UART${uart}` });
      return null;
    }

    // Same parameters as appLibrary's delivery path, which is the flow proven
    // against these ROMs: the CL-PS711x machines need the 0x22 Req_Con flavour,
    // and SA-1100 takes a 2 KB chunk because it has no host RX FIFO cap.
    const conSeq = (profile?.id === 'series5' || profile?.id === 'osaris') ? 2 : 4;
    // Counts anything the device says. The dormancy test below turns on it:
    // total silence on a parkable machine means the link server is asleep and
    // only the user can wake it, whereas bytes-but-no-handshake is a different
    // fault and must not send them to toggle a switch that was working.
    let rawBytesSeen = 0;
    const client = new PlpClient(
      () => controls.serialReadBytes(uart),
      (data) => controls.serialWriteBytes(uart, data),
      {
        conSeq,
        chunkSize: uart === 3 ? 2048 : undefined,
        protocol: linkProtocol === 2 ? 'rfsv16' : 'rfsv32',
        onRawRx: (chunk) => { rawBytesSeen += chunk.length; },
      },
    );
    client.start();
    publish({ phase: 'connecting', summary: 'connecting…' });
    try {
      await client.connect(CONNECT_TIMEOUT_MS);
    } catch (err) {
      try { client.stop(); } catch { /* best effort */ }
      lease.release();
      leaseRef.current = null;
      adoptFailuresRef.current++;

      // Nothing at all on the wire on a parkable machine means the link server
      // has gone dormant, and only the user can revive it. Retrying cannot
      // succeed, so it stops — and says what to do.
      const dormant = rawBytesSeen === 0 && conSeq !== 2;
      if (dormant || adoptFailuresRef.current >= MAX_ADOPT_FAILURES) {
        publish({
          phase: 'dormant',
          actionable: true,
          summary: 'the device is not answering — turn Remote Link off and then on '
                 + 'again on the machine, then sync again',
        });
        return null;
      }
      publish({
        phase: 'error',
        summary: `could not connect — ${err instanceof Error ? err.message : String(err)}`,
      });
      return null;
    }

    adoptFailuresRef.current = 0;
    clientRef.current = client;
    return client;
  }, [controls, linkProtocol, profile?.id, publish, uart, yieldPort]);

  /** One cycle: list both sides, plan, apply. */
  const syncOnce = useCallback(async (): Promise<void> => {
    if (stoppedRef.current || !hostFs || !host.mounts) return;
    const mounts = await host.mounts.get();
    if (!mounts.internal.path || !mounts.internal.enabled) {
      publish({ phase: 'off', summary: 'no folder chosen' });
      return;
    }

    const client = await connect();
    if (!client) return;

    const device = createDeviceFs(client, {
      maxDepth: 1,
      excludeDirs: DEFAULT_MIRROR_POLICY.excludeDirs,
    });

    if (!mirrorRef.current) {
      mirrorRef.current = adoptMirrorState(await host.state.get(stateKey), root)
        ?? createMirrorState(root);
    }
    if (!indexRef.current) {
      indexRef.current = adoptIndex(await host.state.get(indexKey));
    }

    publish({ phase: 'syncing', summary: 'syncing…' });

    const hostFiles = await hostFs.list();
    const deviceFiles = await device.list(root).catch(() => []);

    const index = indexRef.current;
    const plan = planMirror({
      hostFiles, deviceFiles,
      state: mirrorRef.current,
      // Names go through the same 8.3 machinery the shared card uses: EPOC16
      // refuses anything else, and EPOC32 reports short names in listings.
      toDeviceName: (rel) => deviceNameFor(index, rel).replace(/\//g, '\\'),
      toHostName: (devRel) => hostNameForNew(index, devRel, { lowercase: true }),
    });
    await host.state.set(indexKey, index);

    if (plan.refused) {
      publish({
        phase: 'error',
        actionable: true,
        summary: plan.warnings[0] ?? 'refused to sync',
        warnings: plan.warnings,
      });
      return;
    }

    if (plan.steps.length === 0) {
      publish({
        phase: 'idle',
        summary: describeIdle(statusRef.current.lastSyncedAt, hostFiles.length),
        fileCount: hostFiles.length,
        warnings: plan.warnings,
        actionable: false,
      });
      return;
    }

    const result = await applyMirror(plan.steps, hostFs, device, mirrorRef.current, {
      onStep: (step, i, total) => {
        publish({
          phase: 'syncing',
          summary: `${step.op === 'download' ? '↓' : '↑'} ${step.rel} (${i + 1}/${total})`,
        });
      },
      // A dialog taking the port mid-plan must stop the cycle cleanly rather
      // than fail every remaining step.
      shouldAbort: () => !leaseRef.current?.active,
    });
    mirrorRef.current = result.state;
    await host.state.set(stateKey, result.state);

    const now = Date.now();
    if (result.failed.length) {
      publish({
        phase: 'idle',
        lastSyncedAt: now,
        fileCount: hostFiles.length,
        warnings: [...plan.warnings,
                   ...result.failed.map((f) => `${f.step.rel}: ${f.error}`)],
        summary: `synced, ${result.failed.length} file(s) could not be moved`,
        actionable: false,
      });
    } else {
      publish({
        phase: 'idle',
        lastSyncedAt: now,
        fileCount: hostFiles.length,
        warnings: plan.warnings,
        summary: describeIdle(now, hostFiles.length),
        actionable: false,
      });
    }
  }, [connect, host, hostFs, indexKey, publish, root, stateKey]);

  // ── Lifecycle ──────────────────────────────────────────────────────────
  useEffect(() => {
    stoppedRef.current = false;
    adoptFailuresRef.current = 0;
    mirrorRef.current = null;
    indexRef.current = null;

    if (!host.mounts) return;
    if (!canLink(profile)) {
      publish({
        phase: 'off',
        actionable: false,
        summary: profile
          // Said plainly rather than left as a mystery: on these machines the
          // cable is not a road, and the shared card is.
          ? `the ${profile.displayName} has no cable link this emulator can drive — `
            + 'use the shared card folder instead'
          : 'no machine running',
      });
      return;
    }
    if (state !== 'running' || !currentDeviceId) return;

    const settle = window.setTimeout(() => void enqueue(syncOnce), SETTLE_MS);
    return () => {
      stoppedRef.current = true;
      clearTimeout(settle);
      teardown();
    };
  }, [state, currentDeviceId, profile, host, enqueue, syncOnce, teardown, publish]);

  // Host folder changed.
  useEffect(() => {
    if (!host.mounts || !canLink(profile)) return;
    return host.mounts.onChange((id) => {
      if (id === 'internal') void enqueue(syncOnce);
    });
  }, [host, profile, enqueue, syncOnce]);

  // Device polling — the only way to notice a change there.
  useEffect(() => {
    if (!host.mounts || !canLink(profile) || state !== 'running') return;
    const id = window.setInterval(() => {
      // Skip while paused or dormant: neither can make progress, and both would
      // spend a round-trip finding that out.
      const phase = statusRef.current.phase;
      if (phase === 'paused' || phase === 'dormant' || phase === 'off') return;
      void enqueue(syncOnce);
    }, POLL_MS);
    return () => clearInterval(id);
  }, [host, profile, state, enqueue, syncOnce]);

  // The cable came free again — take it back.
  useEffect(() => onUartFree((freed) => {
    if (freed !== uart || stoppedRef.current) return;
    if (statusRef.current.phase !== 'paused') return;
    void enqueue(syncOnce);
  }), [uart, enqueue, syncOnce]);

  // Before a save or a quit, park the session so the snapshot holds an
  // adoptable one rather than a session that dies on restore.
  useEffect(() => host.lifecycle.onPrepareQuit(async () => {
    stoppedRef.current = true;
    const client = clientRef.current;
    if (client) { try { await client.quiesce(4000); } catch { /* best effort */ } }
  }), [host]);

  return null;
}

function describeIdle(lastSyncedAt: number | null, fileCount: number): string {
  if (!lastSyncedAt) return `${fileCount} file(s) — not synced yet`;
  const secs = Math.round((Date.now() - lastSyncedAt) / 1000);
  const when = secs < 10 ? 'just now'
    : secs < 90 ? `${secs}s ago`
    : `${Math.round(secs / 60)} min ago`;
  // Deliberate wording: "synced", never "mounted". At under a KB/s this is not
  // a drive and must not read like one.
  return `synced ${when} · ${fileCount} file(s)`;
}

