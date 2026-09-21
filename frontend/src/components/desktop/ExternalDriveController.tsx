// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useRef, useState } from 'react';
import type { EmulatorControls } from '../../hooks/useEmulator';
import type { DeviceProfile } from '../../types/emulator';
import { requireHost } from '../../lib/desktop/host';
import {
  familyFor, familyLabel, familySupportsProjection, projectFolder, readbackImage,
  shouldCompact, type CardFamily, type HostFile, type SkippedFile,
} from '../../lib/extdrive/project';
import { compactPack } from '../../lib/fefs';
import type { NameIndex } from '../../lib/extdrive/nameIndex';

// Headless: keeps one host folder and the machine's removable card in step.
//
// The projection itself is pure and lives in lib/extdrive/project.ts. What this
// owns is the timing, which is most of the difficulty:
//
//   - The card is built and attached after the machine has loaded, so the guest
//     sees media appear the way it would if you had pushed a card in.
//   - A change in the host folder means reading the LIVE image back first, so
//     anything the guest wrote is preserved before the folder is re-projected
//     over it. Losing a document the user just wrote on the Psion because they
//     also touched the folder would be the worst bug this feature could have.
//   - CF and MMC are updated IN PLACE, which keeps the OS mount alive and
//     refreshes the Series 7's cached directory listing. An SSD pack has no
//     in-place equivalent — swapping one is a physical eject on real hardware —
//     so that path only swaps while the guest is idle, since an app with a file
//     open on the pack would see it vanish.
//
// The whole thing is serialised behind one promise chain. Two projections
// racing would attach two different images and the read-back diff would be
// meaningless.

/** Let the machine settle before pushing a card at it. */
const ATTACH_DELAY_MS = 1_200;
/** How long the guest must be idle before an SSD pack may be swapped. */
const SSD_SWAP_IDLE_MS = 3_000;
/** Which SSD slot the shared pack goes in — see the comment at attachProjection. */
const SHARED_SSD_SLOT = 1;

export interface ExternalDriveStatus {
  family: CardFamily;
  /** Null when no folder is chosen, or the machine has no slot. */
  attached: boolean;
  busy: boolean;
  fileCount: number;
  skipped: SkippedFile[];
  freeBytes: number;
  error: string | null;
  /** Human-readable, for the status pill. */
  summary: string;
}

interface Props {
  controls: EmulatorControls;
  profile: DeviceProfile | null;
  /** True while the guest has been idle long enough for a disruptive swap. */
  onStatus?(status: ExternalDriveStatus): void;
}

/** What the projection put on the card, for the read-back diff to compare against. */
interface Projected {
  rel: string;
  size: number;
  mtimeMs: number;
  bytes?: Uint8Array;
}

export default function ExternalDriveController({ controls, profile, onStatus }: Props) {
  const host = requireHost();
  const { state, currentDeviceId } = controls;
  const family = familyFor(profile);

  const [status, setStatus] = useState<ExternalDriveStatus>(() => ({
    family, attached: false, busy: false, fileCount: 0,
    skipped: [], freeBytes: 0, error: null, summary: 'not set up',
  }));

  // Everything the cycle needs that must not trigger a re-render.
  const chainRef = useRef<Promise<unknown>>(Promise.resolve());
  const indexRef = useRef<NameIndex | null>(null);
  const projectedRef = useRef<Projected[]>([]);
  const attachedRef = useRef(false);
  const lastInputAtRef = useRef(0);
  const statusRef = useRef(status);

  const publish = useCallback((next: Partial<ExternalDriveStatus>) => {
    const merged = { ...statusRef.current, ...next };
    statusRef.current = merged;
    setStatus(merged);
    onStatus?.(merged);
  }, [onStatus]);

  useEffect(() => {
    const touch = () => { lastInputAtRef.current = Date.now(); };
    window.addEventListener('keydown', touch, true);
    window.addEventListener('pointerdown', touch, true);
    return () => {
      window.removeEventListener('keydown', touch, true);
      window.removeEventListener('pointerdown', touch, true);
    };
  }, []);

  /** Serialise every cycle; a second one waits rather than interleaving. */
  const enqueue = useCallback(<T,>(work: () => Promise<T>): Promise<T | undefined> => {
    const next = chainRef.current
      .catch(() => undefined)
      .then(async () => {
        try { return await work(); } catch (err) {
          console.error('[psion] shared drive:', err);
          publish({ busy: false, error: err instanceof Error ? err.message : String(err) });
          return undefined;
        }
      });
    chainRef.current = next;
    return next;
  }, [publish]);

  const indexKey = `mount.cardIndex.${family}`;

  /** Read the host folder, ready for projection. */
  const readHostFolder = useCallback(async (): Promise<HostFile[]> => {
    const mounts = host.mounts;
    if (!mounts) return [];
    const entries = await mounts.list('external');
    const files: HostFile[] = [];
    for (const e of entries) {
      if (e.isDir) continue;
      try {
        files.push({ rel: e.rel, bytes: await mounts.read('external', e.rel), mtimeMs: e.mtimeMs });
      } catch (err) {
        // One unreadable file must not stop the rest of the folder going across.
        console.warn(`[psion] could not read ${e.rel} from the shared folder:`, err);
      }
    }
    return files;
  }, [host]);

  /**
   * Pull the live image back and write anything the guest changed into the host
   * folder. Runs BEFORE any re-projection, which is what stops a host-side
   * change from overwriting a document written on the Psion.
   */
  const readBack = useCallback(async (): Promise<void> => {
    const mounts = host.mounts;
    if (!mounts || !attachedRef.current || !indexRef.current) return;

    const raw = family === 'fefs-ssd'
      ? await controls.getSSDBytes(SHARED_SSD_SLOT)
      : await controls.getCardBytes();
    if (!raw) return;

    const result = readbackImage(raw, family, indexRef.current, projectedRef.current);
    indexRef.current = result.index;
    await host.state.set(indexKey, result.index);

    for (const change of result.changed) {
      try {
        await mounts.write('external', change.rel, change.bytes, change.mtimeMs);
      } catch (err) {
        console.warn(`[psion] could not write ${change.rel} back to the folder:`, err);
      }
    }
    // Deletions the guest made are deliberately NOT propagated here. Removing a
    // file from the user's own folder because the emulated machine no longer
    // lists it is the one direction that destroys data the user did not touch,
    // and a mis-projection (a file that never fitted, say) looks identical to a
    // deletion from this side. The count is surfaced instead.
    if (result.deleted.length) {
      console.log(`[psion] ${result.deleted.length} file(s) are no longer on the card; ` +
                  'left alone in the host folder');
    }
  }, [controls, family, host, indexKey]);

  /** Build the card from the folder and hand it to the machine. */
  const attachProjection = useCallback(async (replaceInPlace: boolean): Promise<void> => {
    const mounts = host.mounts;
    if (!mounts || !familySupportsProjection(family)) return;

    publish({ busy: true, error: null });
    const files = await readHostFolder();
    const stored = await host.state.get<NameIndex>(indexKey);
    const projection = projectFolder(files, family, { index: stored ?? undefined });
    if (!projection.image) { publish({ busy: false }); return; }

    indexRef.current = projection.index;
    await host.state.set(indexKey, projection.index);
    projectedRef.current = files.map((f) => ({
      rel: f.rel,
      size: f.bytes.byteLength,
      mtimeMs: f.mtimeMs,
      // FEFS carries no timestamps, so its diff has to compare content — which
      // means keeping what we wrote. Packs are small enough for that to be
      // cheaper than the alternative.
      bytes: family === 'fefs-ssd' ? f.bytes : undefined,
    }));

    if (family === 'fefs-ssd') {
      // A pack swap is an eject as far as the guest is concerned, so only do it
      // when nobody is mid-keystroke.
      const idleFor = Date.now() - lastInputAtRef.current;
      if (replaceInPlace && idleFor < SSD_SWAP_IDLE_MS) {
        publish({ busy: false, summary: 'waiting for a quiet moment to swap the pack' });
        return;
      }
      // FEFS never reclaims deleted space, so a pack projected onto repeatedly
      // creeps towards full whatever it holds. Rebuild it when it is tight.
      const image = shouldCompact(projection.image, 64 * 1024)
        ? compactPack(projection.image)
        : projection.image;
      // The highest slot: slot 0 holds the user's own packs, and on the MC400
      // the system SSD.
      if (attachedRef.current) await controls.detachSSD(SHARED_SSD_SLOT);
      await controls.attachSSD(SHARED_SSD_SLOT, image, 'flash');
    } else if (replaceInPlace && attachedRef.current) {
      // In place, so the OS mount survives and the Series 7's cached directory
      // listing is refreshed rather than going stale.
      await controls.updateCardInPlace(projection.image);
    } else {
      await controls.attachCard(projection.image);
    }

    attachedRef.current = true;
    const label = familyLabel(family);
    publish({
      busy: false,
      attached: true,
      family,
      fileCount: files.length - projection.skipped.length,
      skipped: projection.skipped,
      freeBytes: projection.freeBytes,
      error: null,
      summary: projection.skipped.length
        ? `${files.length - projection.skipped.length} file(s) on the ${label}, ` +
          `${projection.skipped.length} didn't fit`
        : `${files.length} file(s) on the ${label}`,
    });
  }, [controls, family, host, indexKey, publish, readHostFolder]);

  // ── Attach once the machine is up ──────────────────────────────────────
  useEffect(() => {
    attachedRef.current = false;
    if (!host.mounts || state !== 'running' || !currentDeviceId) return;
    if (!familySupportsProjection(family)) {
      publish({
        family, attached: false, busy: false, fileCount: 0, skipped: [], freeBytes: 0,
        error: null,
        summary: family === 'datapak'
          // Honest rather than silent: there is no Organiser II pack filesystem
          // writer anywhere in the codebase, so per-file projection is not
          // something this can do yet.
          ? 'the Organiser II’s Datapaks need a pack filesystem this build does not have yet'
          : 'this machine has no removable slot',
      });
      return;
    }

    let cancelled = false;
    const timer = window.setTimeout(() => {
      if (cancelled) return;
      void enqueue(async () => {
        const mounts = host.mounts;
        if (!mounts) return;
        const info = await mounts.get();
        if (!info.external.path || !info.external.enabled) {
          publish({ family, attached: false, busy: false, summary: 'no folder chosen' });
          return;
        }
        await attachProjection(false);
      });
    }, ATTACH_DELAY_MS);

    return () => { cancelled = true; clearTimeout(timer); };
  }, [state, currentDeviceId, family, host, enqueue, attachProjection, publish]);

  // ── React to the folder changing ───────────────────────────────────────
  useEffect(() => {
    const mounts = host.mounts;
    if (!mounts) return;
    return mounts.onChange((id) => {
      if (id !== 'external' || !attachedRef.current) return;
      void enqueue(async () => {
        // Read back FIRST. The user may have written a document on the Psion
        // and then dropped a file in the folder; re-projecting without reading
        // back would lose the document.
        await readBack();
        await attachProjection(true);
      });
    });
  }, [host, enqueue, readBack, attachProjection]);

  // ── Read back at the moments the session is being captured ─────────────
  useEffect(() => {
    if (!host.mounts) return;
    return host.lifecycle.onPrepareQuit(async () => { await enqueue(readBack); });
  }, [host, enqueue, readBack]);

  // A device switch means the outgoing machine's card has to come home before
  // the next machine's projection replaces it.
  const previousDeviceRef = useRef<string | null>(null);
  useEffect(() => {
    const previous = previousDeviceRef.current;
    previousDeviceRef.current = currentDeviceId;
    if (!previous || previous === currentDeviceId || !attachedRef.current) return;
    void enqueue(readBack);
  }, [currentDeviceId, enqueue, readBack]);

  useEffect(() => {
    const id = window.setInterval(() => {
      if (attachedRef.current) void enqueue(readBack);
    }, 60_000);
    return () => clearInterval(id);
  }, [enqueue, readBack]);

  return null;
}
