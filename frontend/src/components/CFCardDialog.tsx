// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useState, useEffect, useMemo, useRef, useCallback, type DragEvent } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import {
  createBlankImage, isFat16, listDirectory, readInfo, freeSpace,
  addFileToDirectory, createDirectory, deleteEntryRecursive,
  readFileBytes, setVolumeLabel, MAX_VOLUME_LABEL_LEN,
  ROOT_DIR_CLUSTER, type Fat16Entry,
} from '../lib/fat16';
import {
  tryConvert, isPasswordProtected, canRecoverPassword, recoveryAcceptsCrib,
} from '../lib/converters';
import { sameBytes } from '../lib/bytes';

interface Props {
  controls: EmulatorControls;
  // Which kind of removable card the device takes. Both are a raw FAT16
  // image the user hands to the machine, and the whole dialog is shared;
  // only the nouns differ. 'mmc' is the netpad's board-FPGA MMC slot,
  // 'cf' the PC-Card / CompactFlash socket on every other machine here.
  slotKind?: 'cf' | 'mmc';
  onClose: () => void;
}

const SIZE_PRESETS = [
  { label: '4 MB',   bytes: 4   * 1024 * 1024 },
  { label: '8 MB',   bytes: 8   * 1024 * 1024 },
  { label: '16 MB',  bytes: 16  * 1024 * 1024 },
  { label: '32 MB',  bytes: 32  * 1024 * 1024 },
  { label: '64 MB',  bytes: 64  * 1024 * 1024 },
  { label: '128 MB', bytes: 128 * 1024 * 1024 },
];

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

function downloadBlob(bytes: Uint8Array, filename: string, mime = 'application/octet-stream') {
  const blob = new Blob([bytes as BlobPart], { type: mime });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// Navigation breadcrumb — one segment per directory the user has descended
// into below the root. The cluster identifies that directory in the image.
interface PathSegment { name: string; cluster: number }

// How often to re-pull bytes from the device while the dialog is open and
// the user hasn't staged any local edits. 1.5s feels responsive without
// burning CPU on the per-poll memcpy for larger images.
const DEVICE_POLL_INTERVAL_MS = 1500;

// Upper size for the "is this file password-protected?" scan the listing
// runs (see `protectedFiles`). A Psion document with a password on it is
// kilobytes; anything this big is a disk image or an archive.
const MAX_PROTECTION_SCAN_BYTES = 2 * 1024 * 1024;

export default function CFCardDialog({ controls, slotKind = 'cf', onClose }: Props) {
  // Noun for the card in headings and tooltips (see Props.slotKind).
  const cardName = slotKind === 'mmc' ? 'MMC' : 'CompactFlash';
  // Working-copy of the image bytes as the user edits. `null` until we've
  // pulled whatever's currently attached (or the user creates/loads an image).
  const [image, setImage] = useState<Uint8Array | null>(null);
  const [entries, setEntries] = useState<Fat16Entry[]>([]);
  const [info, setInfo] = useState<ReturnType<typeof readInfo> | null>(null);
  const [space, setSpace] = useState<ReturnType<typeof freeSpace> | null>(null);
  const [status, setStatus] = useState<string>('');
  const [busy, setBusy] = useState(false);
  const [dragOver, setDragOver] = useState(false);
  // Directories below the root that the user is currently navigated into.
  // Empty array == sitting in the root directory.
  const [path, setPath] = useState<PathSegment[]>([]);
  // "Convert on download" default: on.  When on, individual file
  // downloads run through the converter registry; falls back to raw
  // download for any file we don't have a converter for.
  const [convertOnDownload, setConvertOnDownload] = useState(true);
  // True when the working-copy `image` has user edits that aren't yet on
  // the device. While dirty, the auto-refresh poller stays quiet so we
  // don't overwrite the user's in-progress changes with the device's
  // (older) bytes. Cleared by a successful Attach or an explicit Refresh.
  const [localDirty, setLocalDirty] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const imageInputRef = useRef<HTMLInputElement>(null);
  // Latest `image` made available to the poll callback without re-creating
  // the interval on every byte update.
  const imageRef = useRef<Uint8Array | null>(null);
  useEffect(() => { imageRef.current = image; }, [image]);

  const { cardAttached, getCardBytes } = controls;

  const currentDirCluster = path.length === 0 ? ROOT_DIR_CLUSTER : path[path.length - 1].cluster;

  // Pull the currently attached image when the dialog opens so the user sees
  // on-device edits too. getCardBytes is sync on the main thread, a Promise in
  // worker mode — Promise.resolve() handles both.
  useEffect(() => {
    if (controls.cardAttached) {
      void Promise.resolve(controls.getCardBytes()).then(bytes => { if (bytes) setImage(bytes); });
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Auto-refresh: while a card is attached and we have no pending local
  // edits, periodically re-pull the device's bytes so files the device
  // writes (e.g. via the Psion's File Manager) appear without the user
  // having to close and re-open the dialog. We compare before calling
  // setImage so identical bytes don't trigger a needless re-render or
  // FAT re-parse. The pull may be async (worker mode): skip a tick while one
  // is in flight, and drop the result if the poller was torn down meanwhile
  // (dialog closed, card detached, or the user started editing — localDirty
  // flips this effect off, and a late result must not clobber their edits).
  useEffect(() => {
    if (!cardAttached || localDirty) return;
    let inFlight = false;
    let stopped = false;
    const id = setInterval(() => {
      if (inFlight || !imageRef.current) return;
      inFlight = true;
      void Promise.resolve(getCardBytes()).then(bytes => {
        inFlight = false;
        if (stopped || !bytes) return;
        const current = imageRef.current;
        if (current && !sameBytes(bytes, current)) setImage(bytes);
      }).catch(() => { inFlight = false; });
    }, DEVICE_POLL_INTERVAL_MS);
    return () => { stopped = true; clearInterval(id); };
  }, [cardAttached, localDirty, getCardBytes]);

  // Recompute directory listing + stats whenever the image or current dir changes.
  useEffect(() => {
    if (!image) { setEntries([]); setInfo(null); setSpace(null); return; }
    if (!isFat16(image)) {
      setStatus('Image is not a FAT16 volume');
      setEntries([]); setInfo(null); setSpace(null);
      return;
    }
    setInfo(readInfo(image));
    setEntries(listDirectory(image, currentDirCluster));
    setSpace(freeSpace(image));
  }, [image, currentDirCluster]);

  const handleCreateBlank = useCallback((bytes: number) => {
    setBusy(true);
    try {
      const img = createBlankImage(bytes);
      setImage(img);
      setPath([]);
      setLocalDirty(true);
      setStatus(`Formatted ${formatBytes(bytes)} blank FAT16 image`);
    } catch (err) {
      setStatus(String(err));
    } finally {
      setBusy(false);
    }
  }, []);

  const handleLoadImage = useCallback(async (file: File) => {
    setBusy(true);
    try {
      const buf = new Uint8Array(await file.arrayBuffer());
      if (!isFat16(buf)) {
        setStatus(`"${file.name}" is not a FAT16 image`);
        return;
      }
      setImage(buf);
      setPath([]);
      setLocalDirty(true);
      setStatus(`Loaded ${file.name} (${formatBytes(buf.byteLength)})`);
    } finally {
      setBusy(false);
    }
  }, []);

  const handleAddFiles = useCallback(async (files: FileList | File[]) => {
    if (!image) { setStatus('Create or load an image first'); return; }
    setBusy(true);
    try {
      const working = new Uint8Array(image);
      const added: string[] = [];
      const failed: string[] = [];
      for (const file of Array.from(files)) {
        const bytes = new Uint8Array(await file.arrayBuffer());
        const res = addFileToDirectory(working, currentDirCluster, file.name, bytes);
        if (res.ok) added.push(file.name);
        else failed.push(`${file.name}: ${res.reason}`);
      }
      setImage(working);
      setLocalDirty(true);
      if (added.length > 0) setStatus(`Added ${added.length} file(s)${failed.length ? `; ${failed.length} failed` : ''}`);
      if (failed.length > 0 && added.length === 0) setStatus(failed.join(' • '));
    } finally {
      setBusy(false);
    }
  }, [image, currentDirCluster]);

  const handleCreateFolder = useCallback(() => {
    if (!image) { setStatus('Create or load an image first'); return; }
    const name = window.prompt('New folder name (8.3 short name):');
    if (!name) return;
    const working = new Uint8Array(image);
    const res = createDirectory(working, currentDirCluster, name);
    if (!res.ok) { setStatus(res.reason ?? 'Failed to create folder'); return; }
    setImage(working);
    setLocalDirty(true);
    setStatus(`Created folder ${name}`);
  }, [image, currentDirCluster]);

  // Rename the volume / CF partition name. Only offered before the card is
  // attached — renaming a mounted volume would need to go through the device's
  // own file server (and would race the auto-refresh poller), so we gate it on
  // !cardAttached and stage the edit into the working copy like any other.
  const handleRenameLabel = useCallback(() => {
    if (!image) { setStatus('Create or load an image first'); return; }
    if (cardAttached) {
      setStatus('Detach the card first — renaming is only available before attaching');
      return;
    }
    const current = info?.volumeLabel ?? '';
    const next = window.prompt(
      `Volume label (max ${MAX_VOLUME_LABEL_LEN} characters, blank to clear):`,
      current,
    );
    if (next === null) return; // user cancelled
    const working = new Uint8Array(image);
    const res = setVolumeLabel(working, next);
    if (!res.ok) { setStatus(res.reason ?? 'Failed to rename volume'); return; }
    setImage(working);
    setLocalDirty(true);
    setStatus(`Volume label set to ${next.trim().toUpperCase() || '(none)'}`);
  }, [image, cardAttached, info]);

  const handleDeleteEntry = useCallback((entry: Fat16Entry) => {
    if (!image) return;
    if (entry.isDirectory) {
      const ok = window.confirm(
        `Delete folder "${entry.name}" and all its contents?`,
      );
      if (!ok) return;
    }
    const working = new Uint8Array(image);
    deleteEntryRecursive(working, entry);
    setImage(working);
    setLocalDirty(true);
    setStatus(`Deleted ${entry.name}`);
  }, [image]);

  // Manual escape hatch for the auto-refresh — pulls the device's current
  // bytes and replaces the working copy, regardless of local dirty state.
  // Preserves the current breadcrumb so the user stays in the folder
  // they were viewing (the cluster IDs survive in-place writes; only a
  // reformat would invalidate them, in which case the listing falls back
  // to empty and the user can click D:\ to return to root).
  const handleRefreshFromDevice = useCallback(async () => {
    if (!cardAttached) return;
    if (localDirty) {
      const ok = window.confirm('Discard local edits and refresh the files list from the device?');
      if (!ok) return;
    }
    const bytes = await Promise.resolve(getCardBytes());   // sync (main) or Promise (worker)
    if (!bytes) { setStatus('Device returned no card bytes'); return; }
    setImage(bytes);
    setLocalDirty(false);
    setStatus('Refreshed files list from device');
  }, [cardAttached, localDirty, getCardBytes]);

  // Which files in this directory are password-protected Psion
  // documents, and which of those we can actually read back. The list
  // badges them, and offers the crib-assisted "recover…" action for the
  // ones a plain download might only half-recover. Telling needs the
  // file's section table, which sits at its end, so this reads each
  // candidate whole — hence the size cap: no Psion document comes near
  // it, and a card full of large files shouldn't pay for the check on
  // every poll. Recomputed only when the listing or the bytes change.
  const protectedFiles = useMemo(() => {
    const flagged = new Set<string>();
    const recoverable = new Set<string>();
    const cribbable = new Set<string>();
    if (!image) return { flagged, recoverable, cribbable };
    for (const e of entries) {
      if (e.isDirectory || e.size > MAX_PROTECTION_SCAN_BYTES) continue;
      try {
        const bytes = readFileBytes(image, e);
        if (!isPasswordProtected(bytes)) continue;
        flagged.add(e.name);
        if (canRecoverPassword(bytes)) recoverable.add(e.name);
        if (recoveryAcceptsCrib(bytes)) cribbable.add(e.name);
      } catch { /* unreadable — leave unflagged */ }
    }
    return { flagged, recoverable, cribbable };
  }, [image, entries]);

  const handleDownloadEntry = useCallback((entry: Fat16Entry, crib?: Uint8Array) => {
    if (!image) return;
    if (entry.isDirectory) {
      setStatus('Folder download not supported yet — download the whole image instead');
      return;
    }
    const bytes = readFileBytes(image, entry);
    if (convertOnDownload) {
      const converted = tryConvert(bytes, entry.name, crib);
      if (converted) {
        downloadBlob(converted.bytes, converted.filename, converted.mime);
        if (converted.recoveredPassword) {
          setStatus(converted.needsCrib
            ? `Recovered ${entry.name}, but there is too little text in it to be sure of every character — "recover…" takes its first words for an exact result.`
            : `Recovered password-protected ${entry.name} (${converted.formatLabel}).`);
        } else {
          setStatus(`Converted ${entry.name} (${converted.formatLabel})`);
        }
        return;
      }
      // No converter matched — silently fall back to raw download but
      // tell the user why, so unchecked-vs-checked behaviour isn't
      // mysteriously different for these files.
      setStatus(isPasswordProtected(bytes)
        ? `Downloaded original — ${entry.name} is password-protected and this file type can't be read back.`
        : `Downloaded original — no converter for ${entry.name}`);
    }
    downloadBlob(bytes, entry.name);
  }, [image, convertOnDownload]);

  // Assisted recovery for a password-protected document: the user types
  // the document's first characters, which pin the keystream exactly.
  // This is the reliable route for short documents, where there isn't
  // enough text for the automatic search to settle on its own.
  const handleRecoverEntry = useCallback((entry: Fat16Entry) => {
    const crib = window.prompt(
      `Recover "${entry.name}".\n\nType the first few characters of the document as you remember ` +
      `them (the first 32 pin it exactly). Leave blank to let the recovery guess on its own.`, '');
    if (crib === null) return;                      // cancelled
    // In a document body a paragraph break is 0x06 and a tab 0x09; map
    // what the user typed the same way so their newlines line up with
    // the document's own paragraph marks.
    const cribBytes = crib
      ? new Uint8Array(Array.from(crib, ch =>
          ch === '\n' ? 0x06 : ch === '\t' ? 0x09 : ch.charCodeAt(0) & 0xff))
      : undefined;
    handleDownloadEntry(entry, cribBytes);
  }, [handleDownloadEntry]);

  const handleNavigateInto = useCallback((entry: Fat16Entry) => {
    if (!entry.isDirectory) return;
    setPath(p => [...p, { name: entry.name, cluster: entry.firstCluster }]);
  }, []);

  const handleNavigateTo = useCallback((depth: number) => {
    // depth 0 == root; depth N == after navigating into N segments.
    setPath(p => p.slice(0, depth));
  }, []);

  const handleAttachToDevice = useCallback(async () => {
    if (!image) return;
    setBusy(true);
    try {
      const swapping = controls.cardAttached;
      // The netBook mounts the CF on its second PC-Card socket (drive E:);
      // every other machine here uses drive D:.  The Series 7 CF is E:.
      const isSeries7 = controls.currentDeviceId === 'series7';
      const isNetbook = controls.currentDeviceId === 'netbook';
      const driveLetter = isNetbook || isSeries7 ? 'E:' : 'D:';

      if (swapping && (isSeries7 || isNetbook)) {
        // Series 7 / netBook "push edits to the device": update the card bytes
        // IN PLACE.  The OS mount + medata driver stay alive and the core
        // patches the EPOC R5 F32 server's cached root-directory sector, so a
        // fresh E: navigation shows the new files.  This sidesteps the hot-swap
        // re-mount wall (a real eject/insert never re-enumerates the swapped
        // socket -> "Corrupt"); see
        // docs/series7-cf-hotswap-and-dual-socket-2026-06-05.md.  Verified
        // end-to-end on both devices (E: shows the updated file list).
        setStatus('Updating card on the device…');
        const ok = await controls.updateCardInPlace(image);
        if (ok) setLocalDirty(false);
        setStatus(
          ok
            ? `Card updated — drive ${driveLetter} will refresh on the device`
            : 'Update failed',
        );
        return;
      }

      // Faithful hot-swap (netBook / others) or first attach: if a card is
      // already attached, EJECT it first (the device powers the socket down /
      // fires the media-change), pause, then INSERT the new image (re-power ->
      // re-mount).  This mirrors a real card swap and the harness
      // --swap-card-path sequence (detach -> attach); attaching straight over a
      // present card would skip the eject the device needs to re-read the new
      // volume.
      if (swapping) {
        setStatus('Swapping card — ejecting current card…');
        await controls.detachCard();
        await new Promise((r) => setTimeout(r, 600));
      }
      const ok = await controls.attachCard(image);
      if (ok) setLocalDirty(false);
      setStatus(
        ok
          ? `Card ${swapping ? 'swapped' : 'attached'} — drive ${driveLetter} ${swapping ? 'will refresh' : 'should appear'} on the device`
          : 'Attach failed',
      );
    } finally {
      setBusy(false);
    }
  }, [image, controls]);

  const handleDetach = useCallback(async () => {
    setBusy(true);
    try {
      await controls.detachCard();
      setStatus('Card detached');
    } finally {
      setBusy(false);
    }
  }, [controls]);

  const handleDownloadImage = useCallback(() => {
    if (!image) return;
    downloadBlob(image, 'psion-cf.img');
  }, [image]);

  const onDragOver = (e: DragEvent<HTMLDivElement>) => { e.preventDefault(); setDragOver(true); };
  const onDragLeave = () => setDragOver(false);
  const onDrop = (e: DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    setDragOver(false);
    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      void handleAddFiles(e.dataTransfer.files);
    }
  };

  const btn = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  const btnPrimary = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  // "Selected" variant: the size preset matching the card currently loaded in
  // the dialog (ring + filled accent), so the active card's capacity is obvious.
  const btnSelected = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-highlight border border-psion-accent text-psion-charcoal ring-2 ring-psion-accent transition-colors disabled:opacity-50 disabled:cursor-not-allowed';

  // The Series 7 / netBook push edits IN PLACE (the device re-reads the patched
  // directory while the card stays mounted), so once a card is attached the
  // primary action becomes "Update files".  A hardware "Detach" can't be made to
  // visibly remove the card on these devices (the OS keeps the mount cached), so
  // we hide that button once attached rather than offer a control that does
  // nothing.  Other devices keep the plain attach/detach swap.
  const isInPlaceDevice =
    controls.currentDeviceId === 'series7' || controls.currentDeviceId === 'netbook';
  const attached = controls.cardAttached;

  return (
    <div className="w-full max-w-3xl bg-psion-dark border border-psion-accent/40 rounded-lg overflow-hidden flex-shrink-0 mx-4 shadow-sm flex flex-col">
        {/* Header */}
        <div className="flex items-center justify-between px-4 py-3 border-b border-psion-accent/40 bg-psion-mid">
          <h2 className="text-sm font-mono font-semibold text-psion-charcoal">{cardName} Card</h2>
          <button onClick={onClose} className={btn}>Close</button>
        </div>

        {/* Top controls */}
        <div className="flex flex-wrap gap-2 items-center px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/60">
          <span className="text-xs font-mono text-gray-500 mr-2">New blank image:</span>
          {SIZE_PRESETS.map(p => {
            // Highlight the preset matching the loaded/attached card's size so
            // the current card's capacity reads back as "selected" — clicking
            // it still reformats a fresh blank image of that size.
            const isCurrent = image != null && image.byteLength === p.bytes;
            return (
              <button
                key={p.label}
                onClick={() => handleCreateBlank(p.bytes)}
                disabled={busy}
                aria-pressed={isCurrent}
                title={isCurrent ? `Current card size (${p.label})` : `New blank ${p.label} image`}
                className={isCurrent ? btnSelected : btn}
              >
                {p.label}
              </button>
            );
          })}
          <span className="flex-grow" />
          <button
            onClick={() => imageInputRef.current?.click()}
            disabled={busy}
            className={btn}
          >
            Load image…
          </button>
          <input
            ref={imageInputRef}
            type="file"
            accept=".img,.bin,application/octet-stream"
            style={{ display: 'none' }}
            onChange={e => {
              const f = e.target.files?.[0];
              if (f) void handleLoadImage(f);
              e.target.value = '';
            }}
          />
        </div>

        {/* Directory browser / drop zone.
            h-72 keeps the panel a sensible size inline next to the
            device (it used to be a modal with max-h-[90vh] doing the
            same job). The list scrolls internally when the card has
            more files than fit. */}
        <div
          className={`h-72 overflow-auto p-4 ${dragOver ? 'bg-psion-highlight/10' : ''}`}
          onDragOver={onDragOver}
          onDragLeave={onDragLeave}
          onDrop={onDrop}
        >
          {!image && (
            <div className="text-center text-xs font-mono text-gray-500 py-16">
              Create a blank image or load one from your machine to get started.
              <br />
              Then drag files here to copy them onto the card.
            </div>
          )}
          {image && info && (
            <div>
              <div className="text-xs font-mono text-gray-500 mb-3 flex flex-wrap gap-4">
                <span>Size: <span className="text-psion-charcoal font-medium">{formatBytes(image.byteLength)}</span></span>
                <span>
                  State:{' '}
                  <span className={cardAttached ? 'text-psion-charcoal font-medium' : 'text-gray-500 font-medium'}>
                    {cardAttached ? 'Attached ●' : 'Not attached'}
                  </span>
                </span>
                <span className="flex items-center gap-1">
                  Label: <span className="text-psion-charcoal font-medium">{info.volumeLabel || '(none)'}</span>
                  {/* Renaming the CF partition is only offered before attaching:
                      a mounted volume would have to be renamed through the
                      device's file server. Once detached it's available again. */}
                  {!cardAttached && (
                    <button
                      onClick={handleRenameLabel}
                      disabled={busy}
                      title={`Rename the ${cardName} card volume label (before attaching)`}
                      className="text-psion-accent hover:underline cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
                    >Rename…</button>
                  )}
                </span>
                {/* Which drive letter the device's OS mounts this card on.
                    The netBook exposes two PC-Card sockets and puts the
                    CompactFlash on the second one (drive E:); every other
                    SA-1100/Windermere machine here — the netpad's MMC slot
                    included — mounts it as D:. */}
                <span>Drive: <span className="text-psion-charcoal font-medium">
                  {controls.currentDeviceId === 'netbook' ? 'E:' : 'D:'}</span></span>
                <span>Cluster: {info.clusterBytes} B</span>
                {space && (
                  <span>
                    Free: <span className="text-psion-charcoal font-medium">{formatBytes(space.free)}</span>
                    {' / '}
                    <span className="text-psion-charcoal font-medium">{formatBytes(space.total)}</span>
                  </span>
                )}
              </div>
              {/* Breadcrumb path. Root is always clickable; intermediate dirs
                  jump straight there when clicked. Refresh button on the
                  right re-pulls bytes from the device — backstop for the
                  auto-refresh poller, and the only way to discard local
                  edits without re-opening the dialog. */}
              <div className="text-xs font-mono mb-2 flex flex-wrap items-center gap-1">
                <button
                  className="text-psion-accent hover:underline cursor-pointer"
                  onClick={() => handleNavigateTo(0)}
                  disabled={path.length === 0}
                >{controls.currentDeviceId === 'netbook' ? 'E:\\' : 'D:\\'}</button>
                {path.map((seg, i) => (
                  <span key={i} className="flex items-center gap-1">
                    <span className="text-gray-500">/</span>
                    <button
                      className="text-psion-accent hover:underline cursor-pointer"
                      onClick={() => handleNavigateTo(i + 1)}
                      disabled={i === path.length - 1}
                    >{seg.name}</button>
                  </span>
                ))}
                {cardAttached && (
                  <>
                    <span className="flex-grow" />
                    <button
                      onClick={() => void handleRefreshFromDevice()}
                      title="Refresh files list from device"
                      className="text-psion-accent hover:underline cursor-pointer px-1"
                    >↻ Refresh</button>
                  </>
                )}
              </div>
              {entries.length === 0 && (
                <div className="text-xs font-mono text-gray-500 italic">(empty — drag files here)</div>
              )}
              {entries.length > 0 && (
                <table className="w-full text-xs font-mono">
                  <thead>
                    <tr className="text-gray-500 text-left border-b border-psion-accent/40">
                      <th className="py-1">Name</th>
                      <th className="py-1 w-24 text-right">Size</th>
                      <th className="py-1 w-48 text-right">Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    {entries.map((e, i) => (
                      <tr key={i} className="border-b border-psion-accent/20 hover:bg-psion-mid/50">
                        <td className="py-1 text-psion-charcoal">
                          {e.isDirectory ? (
                            <button
                              className="text-left hover:underline cursor-pointer"
                              onClick={() => handleNavigateInto(e)}
                            >📁 {e.name}</button>
                          ) : (
                            e.name
                          )}
                          {protectedFiles.flagged.has(e.name) && (
                            <span className="ml-2 text-amber-600"
                                  title={protectedFiles.recoverable.has(e.name)
                                    ? 'This document is password-protected. "Download" (with Convert on) reads it back anyway; "recover…" lets you supply its first words for an exact result.'
                                    : 'This document is password-protected, and this file type can only be downloaded as-is.'}>🔒</span>
                          )}
                        </td>
                        <td className="py-1 text-right text-gray-500">
                          {e.isDirectory ? '' : formatBytes(e.size)}
                        </td>
                        <td className="py-1 text-right">
                          {!e.isDirectory && (
                            <button
                              className={`${btn} mr-1`}
                              onClick={() => handleDownloadEntry(e)}
                            >Download</button>
                          )}
                          {protectedFiles.cribbable.has(e.name) && (
                            <button
                              className={`${btn} mr-1`}
                              title="Recover this password-protected document, optionally giving its first few characters"
                              onClick={() => handleRecoverEntry(e)}
                            >recover…</button>
                          )}
                          <button
                            className={btn}
                            onClick={() => handleDeleteEntry(e)}
                          >Delete</button>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </div>
          )}
        </div>

        {/* Status bar */}
        {status && (
          <div className="px-4 py-2 border-t border-psion-accent/30 text-xs font-mono text-gray-500">
            {status}
          </div>
        )}

        {/* Bottom action bar */}
        <div className="flex flex-wrap gap-2 items-center px-4 py-3 border-t border-psion-accent/40 bg-psion-mid">
          <button
            onClick={() => fileInputRef.current?.click()}
            disabled={!image || busy}
            className={btn}
          >
            Add files…
          </button>
          <input
            ref={fileInputRef}
            type="file"
            multiple
            style={{ display: 'none' }}
            onChange={e => {
              if (e.target.files && e.target.files.length > 0) void handleAddFiles(e.target.files);
              e.target.value = '';
            }}
          />
          <button
            onClick={handleCreateFolder}
            disabled={!image || busy}
            className={btn}
          >
            New folder…
          </button>
          <button
            onClick={handleDownloadImage}
            disabled={!image || busy}
            className={btn}
          >
            Download image
          </button>
          <label className="flex items-center gap-1.5 text-xs font-mono text-psion-charcoal select-none cursor-pointer ml-2"
                 title="Turn Psion documents into modern formats as they download (Word → RTF, Sheet → CSV, Sketch → PNG, Record → WAV). Password-protected Word files are recovered automatically.">
            <input
              type="checkbox"
              checked={convertOnDownload}
              onChange={e => setConvertOnDownload(e.target.checked)}
              className="cursor-pointer"
            />
            Convert on download
          </label>
          <span className="flex-grow" />
          {/* Hide "Detach" on the 7 / netBook once attached — a hardware eject
              can't visibly remove the card there (the OS keeps the mount cached),
              so the control would do nothing.  Other devices keep it. */}
          {attached && !isInPlaceDevice && (
            <button onClick={handleDetach} disabled={busy} className={btn}>
              Detach from device
            </button>
          )}
          {/* The "Attach to device" handler no longer auto-closes the panel
              now that we render inline alongside Remote Link / Modem /
              Logs — keeping it open lets the user keep adding files
              after the attach without re-opening from the control bar.
              On the 7 / netBook a re-press after attach pushes edits in place,
              so the label becomes "Update files". */}
          <button
            onClick={handleAttachToDevice}
            disabled={!image || busy}
            className={btnPrimary}
          >
            {attached && isInPlaceDevice ? 'Update files' : 'Attach to device'}
          </button>
        </div>
    </div>
  );
}
