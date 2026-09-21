// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useRef } from 'react';
import type { EmulatorControls } from '../../hooks/useEmulator';
import { collectDeviceBundle } from '../../hooks/useEmulator';
import {
  DEFAULT_AUTOSAVE_POLICY, DEFAULT_RETENTION, planRetention, shouldAutoSave,
} from '../../lib/desktop/autosave';
import { requireHost } from '../../lib/desktop/host';

// Headless: saves the running machine on a timer, and mirrors each save to a
// generation file on disk.
//
// IndexedDB stays the live store. The emulator's own save path writes it from
// inside the worker, already gzipped, and rewiring that is the change in this
// codebase most likely to corrupt a session silently. So this calls the
// existing saveState() and then copies the result out — a read, an encode of
// offsets and a file write, with no recompression.
//
// What the disk copy buys that IDB cannot: a history to roll back through,
// and a save that survives a wiped profile or an evicted origin.
//
// The cost being managed is a GUEST FREEZE, not render stutter. saveState()
// pauses the emulated machine for the whole snapshot (see
// frontend/public/emulator-worker.js), so the timer only fires when the user
// has been idle a moment and the machine has actually advanced. That policy
// is pure and tested in lib/desktop/autosave.ts.

/** How often to re-evaluate. Cheap — the policy usually says "not yet". */
const TICK_MS = 5_000;

interface Props {
  controls: EmulatorControls;
  /** True while a drive sync is transferring; a save would quiesce it. */
  syncInFlight?: boolean;
  /** Reports save activity so the chrome can show it. */
  onStatus?(status: { savingSince: number | null; lastSavedAt: number | null }): void;
}

export default function AutoSaveController({ controls, syncInFlight = false, onStatus }: Props) {
  const host = requireHost();
  const { state, currentDeviceId, paused, profiles } = controls;

  const lastSaveAtRef = useRef(0);
  const lastInputAtRef = useRef(Date.now());
  const cyclesAtLastSaveRef = useRef(0);
  const savingRef = useRef(false);

  // Track user activity. Window-level and capturing, so it sees input on its
  // way to the emulator rather than competing with it.
  useEffect(() => {
    const touch = () => { lastInputAtRef.current = Date.now(); };
    window.addEventListener('keydown', touch, true);
    window.addEventListener('pointerdown', touch, true);
    window.addEventListener('wheel', touch, true);
    return () => {
      window.removeEventListener('keydown', touch, true);
      window.removeEventListener('pointerdown', touch, true);
      window.removeEventListener('wheel', touch, true);
    };
  }, []);

  /**
   * Save, then mirror to disk and prune. Serialised by savingRef: two
   * overlapping snapshots of the same heap would both pause the guest and
   * race each other's generation files.
   */
  const saveAndMirror = useCallback(async (reason: string) => {
    const deviceId = currentDeviceId;
    if (!deviceId || savingRef.current) return;
    savingRef.current = true;
    onStatus?.({ savingSince: Date.now(), lastSavedAt: lastSaveAtRef.current || null });
    try {
      // The existing path: quiesces any live link session, snapshots the
      // heap, gzips it in the worker, writes IDB.
      await controls.saveState();
      lastSaveAtRef.current = Date.now();
      cyclesAtLastSaveRef.current = controls.getSimCycles?.() ?? 0;

      const saves = host.saves;
      if (!saves) return;
      const displayName = profiles.find((p) => p.id === deviceId)?.displayName;
      const bundle = await collectDeviceBundle(deviceId, displayName);
      if (!bundle) return;
      const bytes = new Uint8Array(await bundle.blob.arrayBuffer());
      const savedAt = lastSaveAtRef.current;
      await saves.write(deviceId, bytes, { savedAt, schemaVersion: bundle.schemaVersion });

      const generations = await saves.list(deviceId);
      const doomed = planRetention(generations, DEFAULT_RETENTION, savedAt);
      if (doomed.length) await saves.prune(deviceId, doomed);
    } catch (err) {
      // A failed mirror must not take the session down; IDB still holds the
      // save, which is the copy the app actually restores from.
      console.error(`[psion] autosave (${reason}) failed:`, err);
    } finally {
      savingRef.current = false;
      onStatus?.({ savingSince: null, lastSavedAt: lastSaveAtRef.current || null });
    }
  }, [controls, currentDeviceId, host, profiles, onStatus]);

  // ── The timer ──────────────────────────────────────────────────────────
  useEffect(() => {
    const id = window.setInterval(() => {
      const cycles = controls.getSimCycles?.();
      const decision = shouldAutoSave({
        now: Date.now(),
        lastSaveAt: lastSaveAtRef.current,
        lastInputAt: lastInputAtRef.current,
        running: state === 'running' && !!currentDeviceId,
        loading: state !== 'running',
        paused,
        syncInFlight,
        // A path that cannot report cycles must be assumed to have advanced,
        // or it would never save at all.
        simCyclesDelta: cycles === undefined ? 1 : cycles - cyclesAtLastSaveRef.current,
      }, DEFAULT_AUTOSAVE_POLICY);
      if (decision.save) void saveAndMirror(decision.reason);
    }, TICK_MS);
    return () => clearInterval(id);
  }, [controls, state, currentDeviceId, paused, syncInFlight, saveAndMirror]);

  // ── Losing focus is a good moment ──────────────────────────────────────
  // Clicking away is the clearest "I have stopped using this" signal there is,
  // and the machine is idle by definition at that point.
  useEffect(() => {
    const onBlur = () => {
      if (state !== 'running' || !currentDeviceId) return;
      // Still respect the interval, so alt-tabbing repeatedly does not freeze
      // the guest once per switch.
      if (Date.now() - lastSaveAtRef.current < DEFAULT_AUTOSAVE_POLICY.intervalMs) return;
      void saveAndMirror('blur');
    };
    window.addEventListener('blur', onBlur);
    return () => window.removeEventListener('blur', onBlur);
  }, [state, currentDeviceId, saveAndMirror]);

  // ── Quit ───────────────────────────────────────────────────────────────
  // Main defers exiting until this resolves, so the last minute of a session
  // is not lost. Unconditional: the interval does not apply when the
  // alternative is losing the work.
  useEffect(() => host.lifecycle.onPrepareQuit(async () => {
    if (state !== 'running' || !currentDeviceId) return;
    await saveAndMirror('quit');
  }), [host, state, currentDeviceId, saveAndMirror]);

  // ── A device switch ────────────────────────────────────────────────────
  // The hook already saves the outgoing machine to IDB on a switch, so what
  // is left is mirroring that save to disk before its device id changes
  // under us.
  const previousDeviceRef = useRef<string | null>(null);
  useEffect(() => {
    const previous = previousDeviceRef.current;
    previousDeviceRef.current = currentDeviceId;
    // Captured before the closure: the narrowing from the guard above does
    // not survive into an async body.
    const saves = host.saves;
    if (!previous || previous === currentDeviceId || !saves) return;
    void (async () => {
      try {
        const displayName = profiles.find((p) => p.id === previous)?.displayName;
        const bundle = await collectDeviceBundle(previous, displayName);
        if (!bundle) return;
        const bytes = new Uint8Array(await bundle.blob.arrayBuffer());
        const savedAt = Date.now();
        await saves.write(previous, bytes, { savedAt, schemaVersion: bundle.schemaVersion });
        const generations = await saves.list(previous);
        const doomed = planRetention(generations, DEFAULT_RETENTION, savedAt);
        if (doomed.length) await saves.prune(previous, doomed);
      } catch (err) {
        console.error('[psion] mirroring the outgoing device failed:', err);
      }
    })();
  }, [currentDeviceId, host, profiles]);

  return null;
}
