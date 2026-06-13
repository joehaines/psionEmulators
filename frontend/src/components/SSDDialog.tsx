// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useState, useEffect, useCallback, useRef, type DragEvent } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import {
  createFlashPack, addFileToPack, removeFileFromPack, listFiles,
  classifyPack, readVolumeName, FLASH_PACK_SIZES,
  type FefsFile, type PackKind,
} from '../lib/fefs';

interface Props {
  controls: EmulatorControls;
  slotCount: number;
  onClose: () => void;
}

// Files with a recognised Psion type go into the matching app folder so
// they appear in the right file list on the System screen (which scans
// \WRD\, \AGN\, ... on every drive). Anything else lands in the root,
// still reachable via Disk → Directory.
const APP_DIR_BY_EXT: Record<string, string> = {
  WRD: 'WRD',   // Word
  AGN: 'AGN',   // Agenda
  SPR: 'SPR',   // Sheet
  DBF: 'DAT',   // Data
  WLD: 'WLD',   // World
  OPL: 'OPL',   // OPL source
  OPO: 'OPO',   // compiled OPL
  OPA: 'APP',   // OPL applications
  APP: 'APP',
  IMG: 'APP',   // RunImg apps ship alongside .APP on factory packs
};

function dirForFile(name: string): string | undefined {
  const dot = name.lastIndexOf('.');
  if (dot < 0) return undefined;
  return APP_DIR_BY_EXT[name.slice(dot + 1).toUpperCase()];
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

function downloadBlob(bytes: Uint8Array, filename: string) {
  const blob = new Blob([bytes as BlobPart], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// Working copy of an SSD image the user is editing in the dialog.
// Independent of whatever is currently inserted in the device — the
// "Insert" button pushes this into WASM; "Eject" pulls bytes back out.
//
// `kind` is the pack type presented to the Psion (a hardware strap on
// real packs, chosen here at create/load time — NOT derived from the
// bytes): 'flash' mounts read-only with the staged files visible;
// 'ram' is for dumps of real RAM packs.
interface SlotState {
  image: Uint8Array | null;
  kind: PackKind;
  volume: string;
  files: FefsFile[];
}

const EMPTY_SLOT: SlotState = { image: null, kind: 'flash', volume: '', files: [] };

function slotFromImage(image: Uint8Array, kind?: PackKind): SlotState {
  const k = kind ?? classifyPack(image);
  return {
    image,
    kind: k,
    volume: readVolumeName(image),
    files: k === 'flash' ? listFiles(image) : [],
  };
}

const btn        = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-40 disabled:cursor-not-allowed';
const btnPrimary = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-40 disabled:cursor-not-allowed';
const btnDanger  = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-mid border border-red-400 text-red-600 hover:bg-red-50 hover:border-red-600 transition-colors disabled:opacity-40 disabled:cursor-not-allowed';
const inputCls   = 'px-2 py-1 rounded text-xs font-mono bg-white border border-psion-accent/60 text-psion-charcoal focus:border-psion-accent focus:outline-none';

export default function SSDDialog({ controls, slotCount, onClose }: Props) {
  const [slots, setSlots] = useState<SlotState[]>(() =>
    Array.from({ length: slotCount }, () => ({ ...EMPTY_SLOT })));
  const [status, setStatus] = useState('');
  const [busy, setBusy] = useState(false);

  // On open, pull whatever's currently inserted into the working state.
  // getSSDBytes is sync on the main thread but a Promise in worker mode,
  // so resolve per slot and patch the state as each arrives.
  useEffect(() => {
    for (let i = 0; i < slotCount; i++) {
      if (!controls.ssdAttached[i]) continue;
      const slot = i;
      void Promise.resolve(controls.getSSDBytes(slot)).then(bytes => {
        if (!bytes) return;
        setSlots(prev => prev.map((s, j) => (j === slot ? slotFromImage(bytes) : s)));
      });
    }
  // Mount-only — re-running would clobber edits in progress.
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const updateSlot = useCallback((idx: number, next: SlotState) => {
    setSlots(prev => prev.map((s, i) => (i === idx ? next : s)));
  }, []);

  const handleCreate = useCallback((idx: number, sizeBytes: number, name: string) => {
    try {
      const bytes = createFlashPack(sizeBytes, name || 'FLASH');
      updateSlot(idx, slotFromImage(bytes, 'flash'));
      setStatus(`Created blank pack (${formatBytes(sizeBytes)}, vol "${name}") for slot ${slotLabel(idx)}. Add files, then insert.`);
    } catch (err) {
      setStatus(`Error: ${msg(err)}`);
    }
  }, [updateSlot]);

  const handleLoadFile = useCallback(async (idx: number, file: File) => {
    try {
      const bytes = new Uint8Array(await file.arrayBuffer());
      const ok = bytes.byteLength >= 0x10000 &&
                 bytes.byteLength <= 0x800000 &&
                 (bytes.byteLength & (bytes.byteLength - 1)) === 0;
      if (!ok) {
        setStatus(`Error: image size ${formatBytes(bytes.byteLength)} is invalid (must be a power of two between 64 KB and 8 MB).`);
        return;
      }
      const kind = classifyPack(bytes);
      updateSlot(idx, slotFromImage(bytes, kind));
      setStatus(kind === 'flash'
        ? `Loaded "${file.name}" (${formatBytes(bytes.byteLength)}) — Flash pack, will mount read-only.`
        : `Loaded "${file.name}" (${formatBytes(bytes.byteLength)}) — no FEFS header, will attach as a RAM pack.`);
    } catch (err) {
      setStatus(`Error: ${msg(err)}`);
    }
  }, [updateSlot]);

  const handleAddFiles = useCallback(async (idx: number, files: FileList | File[]) => {
    const slot = slots[idx];
    if (!slot.image) {
      setStatus('Create or load a pack first, then add files.');
      return;
    }
    if (slot.kind !== 'flash') {
      setStatus('Files can only be added to Flash packs. RAM images are kept byte-exact.');
      return;
    }
    setBusy(true);
    try {
      let cur = slot.image;
      const list = Array.from(files);
      const routed: string[] = [];
      for (const f of list) {
        const bytes = new Uint8Array(await f.arrayBuffer());
        const dir = dirForFile(f.name);
        cur = addFileToPack(cur, f.name, bytes, dir);
        if (dir) routed.push(`${f.name} → \\${dir}\\`);
      }
      updateSlot(idx, slotFromImage(cur, 'flash'));
      const routedNote = routed.length ? ` (${routed.join(', ')})` : '';
      setStatus(`Added ${list.length} file(s) to slot ${slotLabel(idx)}${routedNote}. Insert into the device to make them visible.`);
    } catch (err) {
      setStatus(`Error: ${msg(err)}`);
    } finally {
      setBusy(false);
    }
  }, [slots, updateSlot]);

  const handleRemoveFile = useCallback((idx: number, name: string) => {
    const slot = slots[idx];
    if (!slot.image) return;
    try {
      updateSlot(idx, slotFromImage(removeFileFromPack(slot.image, name), slot.kind));
      setStatus(`Removed "${name}" from slot ${slotLabel(idx)}.`);
    } catch (err) {
      setStatus(`Error: ${msg(err)}`);
    }
  }, [slots, updateSlot]);

  const handleInsert = useCallback(async (idx: number) => {
    const slot = slots[idx];
    if (!slot.image) return;
    setBusy(true);
    try {
      const ok = await controls.attachSSD(idx, slot.image, slot.kind);
      setStatus(ok
        ? `Inserted slot ${slotLabel(idx)} (${formatBytes(slot.image.length)}). The device should detect it within a moment.`
        : `Insert failed for slot ${slotLabel(idx)}.`);
    } finally {
      setBusy(false);
    }
  }, [slots, controls]);

  const handleEject = useCallback(async (idx: number) => {
    setBusy(true);
    try {
      // Pull the current bytes out of WASM BEFORE detaching, then keep
      // them (and the pack kind) as the working copy.
      const snap = await Promise.resolve(controls.getSSDBytes(idx));
      await controls.detachSSD(idx);
      if (snap) updateSlot(idx, slotFromImage(snap, slots[idx].kind));
      setStatus(`Ejected slot ${slotLabel(idx)}. The pack is still here — re-insert to put it back.`);
    } finally {
      setBusy(false);
    }
  }, [controls, slots, updateSlot]);

  const handleDiscard = useCallback((idx: number) => {
    updateSlot(idx, { ...EMPTY_SLOT });
    setStatus(`Discarded staged pack in slot ${slotLabel(idx)}.`);
  }, [updateSlot]);

  const handleSaveToFile = useCallback(async (idx: number) => {
    const slot = slots[idx];
    // For an inserted pack, save the LIVE device bytes, not the working
    // copy captured when the dialog opened.
    const bytes = controls.ssdAttached[idx]
      ? ((await Promise.resolve(controls.getSSDBytes(idx))) ?? slot.image)
      : slot.image;
    if (!bytes) return;
    const ext = slot.kind === 'flash' ? 'flash.ssd' : 'ram.ssd';
    downloadBlob(bytes, `pack-${String.fromCharCode(97 + idx)}-${slot.volume || 'untitled'}.${ext}`);
    setStatus(`Downloaded slot ${slotLabel(idx)} as a .${ext} file.`);
  }, [slots, controls]);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/70 p-4"
         onClick={onClose}>
      <div className="bg-psion-dark border border-psion-accent rounded-lg w-full max-w-3xl max-h-[90vh] overflow-hidden flex flex-col"
           onClick={e => e.stopPropagation()}>
        <div className="flex items-center justify-between px-4 py-3 border-b border-psion-accent/40 bg-psion-mid">
          <h2 className="text-sm font-mono font-semibold text-psion-charcoal">
            SSD Pack {slotCount === 1 ? '(1 slot)' : `(${slotCount} slots)`}
          </h2>
          <button onClick={onClose}
                  className="text-gray-400 hover:text-psion-charcoal font-mono text-xs transition-colors">
            Close ✕
          </button>
        </div>

        <div className="px-4 py-3 border-b border-psion-accent/30 text-xs font-mono text-gray-500 leading-relaxed">
          SSD packs are removable memory cards the Psion sees as extra
          drives. Create a pack here, drop files into it (Psion file
          types are sorted into the right app folders automatically),
          then insert it — the files appear on the device immediately.
          You can also load <span className="text-psion-charcoal font-medium">.bin
          / .img / .ssd images</span> dumped from real packs.
          Packs mount <span className="text-psion-charcoal font-medium">read-only</span> on
          the Psion: in-device formatting and saving are not supported
          by the emulator, so make changes here and re-insert.
        </div>

        <div className="flex-grow overflow-auto p-4 space-y-4">
          {slots.map((slot, idx) => (
            <SlotPanel
              key={idx}
              idx={idx}
              slot={slot}
              attached={controls.ssdAttached[idx]}
              busy={busy}
              onCreate={handleCreate}
              onLoadFile={handleLoadFile}
              onAddFiles={handleAddFiles}
              onRemoveFile={handleRemoveFile}
              onInsert={handleInsert}
              onEject={handleEject}
              onDiscard={handleDiscard}
              onSaveToFile={handleSaveToFile}
            />
          ))}
        </div>

        {status && (
          <div className="px-4 py-2 border-t border-psion-accent/30 text-xs font-mono text-psion-charcoal bg-psion-mid">
            {status}
          </div>
        )}
      </div>
    </div>
  );
}

// ── Per-slot panel ────────────────────────────────────────────────────

interface SlotPanelProps {
  idx: number;
  slot: SlotState;
  attached: boolean;
  busy: boolean;
  onCreate: (idx: number, sizeBytes: number, name: string) => void;
  onLoadFile: (idx: number, file: File) => void;
  onAddFiles: (idx: number, files: FileList | File[]) => void;
  onRemoveFile: (idx: number, name: string) => void;
  onInsert: (idx: number) => void;
  onEject: (idx: number) => void;
  onDiscard: (idx: number) => void;
  onSaveToFile: (idx: number) => void;
}

function SlotPanel({
  idx, slot, attached, busy,
  onCreate, onLoadFile, onAddFiles, onRemoveFile,
  onInsert, onEject, onDiscard, onSaveToFile,
}: SlotPanelProps) {
  // Form state for the inline "Create blank pack" UI.
  const [createSize, setCreateSize] = useState<number>(0x20000);  // 128K
  const [createName, setCreateName] = useState<string>('PACK' + slotLabel(idx));
  const [dragOver,   setDragOver]   = useState(false);

  const loadInputRef = useRef<HTMLInputElement>(null);
  const addInputRef  = useRef<HTMLInputElement>(null);

  const status: 'empty' | 'staged' | 'inserted' =
    attached ? 'inserted' : slot.image ? 'staged' : 'empty';

  const handleDragOver = (e: DragEvent) => { e.preventDefault(); setDragOver(true); };
  const handleDragLeave = () => setDragOver(false);
  const handleDrop = (e: DragEvent) => {
    e.preventDefault();
    setDragOver(false);
    if (!slot.image) return;
    if (e.dataTransfer.files.length) onAddFiles(idx, e.dataTransfer.files);
  };

  return (
    <section className="border border-psion-accent/40 rounded-lg overflow-hidden">
      <header className="flex items-center justify-between px-3 py-2 bg-psion-mid">
        <div className="flex items-center gap-2">
          <h3 className="font-mono text-sm text-psion-charcoal font-medium">Pack {slotLabel(idx)}</h3>
          <StatusBadge status={status} />
        </div>
        <span className="font-mono text-xs text-gray-500">
          {slot.image
            ? `${slot.kind === 'ram' ? 'RAM' : 'Flash'} ${formatBytes(slot.image.length)} · vol "${slot.volume || '—'}"`
            : 'no pack'}
        </span>
      </header>

      <div className="p-3 space-y-4">

        {/* Empty state: prominent "get a pack" form. */}
        {status === 'empty' && (
          <div className="space-y-3">
            <div>
              <h4 className="font-mono text-xs text-psion-charcoal font-medium mb-2">Create a blank pack</h4>
              <div className="flex flex-wrap items-center gap-2">
                <label className="font-mono text-xs text-psion-charcoal flex items-center gap-1">
                  Size
                  <select className={inputCls}
                          value={createSize}
                          onChange={e => setCreateSize(Number(e.target.value))}>
                    {FLASH_PACK_SIZES.map(s => (
                      <option key={s.bytes} value={s.bytes}>{s.label}</option>
                    ))}
                  </select>
                </label>
                <label className="font-mono text-xs text-psion-charcoal flex items-center gap-1">
                  Volume name
                  <input className={inputCls + ' w-24'}
                         maxLength={8}
                         value={createName}
                         onChange={e => setCreateName(e.target.value.toUpperCase())} />
                </label>
                <button className={btnPrimary}
                        disabled={busy || !createName.trim()}
                        onClick={() => onCreate(idx, createSize, createName.trim())}>
                  Create
                </button>
              </div>
              <p className="font-mono text-[10px] text-gray-500 mt-1">
                A FEFS-formatted Flash pack, identical to a factory Psion SSD.
                The Psion mounts it read-only with your files visible.
              </p>
            </div>

            <div>
              <h4 className="font-mono text-xs text-psion-charcoal font-medium mb-2">Or load a saved image</h4>
              <input ref={loadInputRef}
                     type="file"
                     accept=".bin,.img,.ssd,.rom"
                     className="hidden"
                     onChange={e => {
                       const f = e.target.files?.[0];
                       if (f) onLoadFile(idx, f);
                       e.target.value = '';
                     }} />
              <button className={btn}
                      disabled={busy}
                      onClick={() => loadInputRef.current?.click()}>
                Choose .bin / .img / .ssd file…
              </button>
              <p className="font-mono text-[10px] text-gray-500 mt-1">
                Images with a FEFS header attach as Flash packs; anything
                else attaches as a RAM pack (for dumps of real RAM SSDs).
              </p>
            </div>
          </div>
        )}

        {/* Staged state: pack exists but isn't in the device. */}
        {status === 'staged' && (
          <div className="space-y-3">
            <div className="flex flex-wrap gap-2">
              <button className={btnPrimary} disabled={busy} onClick={() => onInsert(idx)}>
                Insert into device
              </button>
              <button className={btn} disabled={busy} onClick={() => onSaveToFile(idx)}>
                Save image to file
              </button>
              <button className={btnDanger} disabled={busy} onClick={() => onDiscard(idx)}>
                Discard
              </button>
            </div>
            <FilesPanel slot={slot} idx={idx} busy={busy}
                        addInputRef={addInputRef}
                        dragOver={dragOver}
                        onDragOver={handleDragOver}
                        onDragLeave={handleDragLeave}
                        onDrop={handleDrop}
                        onAddFiles={onAddFiles}
                        onRemoveFile={onRemoveFile} />
          </div>
        )}

        {/* Inserted state: pack is currently in the device. */}
        {status === 'inserted' && (
          <div className="space-y-3">
            <div className="flex flex-wrap gap-2">
              <button className={btnDanger} disabled={busy} onClick={() => onEject(idx)}>
                Eject
              </button>
              <button className={btn} disabled={busy} onClick={() => onSaveToFile(idx)}>
                Save image to file
              </button>
            </div>
            <p className="font-mono text-xs text-gray-500">
              The pack is in the device. To add or remove files, eject
              it first, edit, and re-insert.
            </p>
            <FilesPanel slot={slot} idx={idx} busy={true}
                        addInputRef={addInputRef}
                        dragOver={false}
                        onDragOver={handleDragOver}
                        onDragLeave={handleDragLeave}
                        onDrop={handleDrop}
                        onAddFiles={onAddFiles}
                        onRemoveFile={onRemoveFile} />
          </div>
        )}
      </div>
    </section>
  );
}

// ── File list + add zone ──────────────────────────────────────────────

interface FilesPanelProps {
  slot: SlotState;
  idx: number;
  busy: boolean;
  addInputRef: React.RefObject<HTMLInputElement>;
  dragOver: boolean;
  onDragOver: (e: DragEvent) => void;
  onDragLeave: () => void;
  onDrop: (e: DragEvent) => void;
  onAddFiles: (idx: number, files: FileList | File[]) => void;
  onRemoveFile: (idx: number, name: string) => void;
}

function FilesPanel({
  slot, idx, busy, addInputRef, dragOver,
  onDragOver, onDragLeave, onDrop, onAddFiles, onRemoveFile,
}: FilesPanelProps) {
  if (slot.kind !== 'flash') {
    return (
      <p className="font-mono text-xs text-gray-500">
        RAM pack image — kept byte-exact; file editing isn't available.
      </p>
    );
  }

  return (
    <div className="border border-psion-accent/30 rounded bg-white">
      <div className="flex items-center justify-between px-3 py-2 bg-psion-mid/60">
        <h4 className="font-mono text-xs text-psion-charcoal">
          Files in pack ({slot.files.length})
        </h4>
        <div>
          <input ref={addInputRef}
                 type="file"
                 multiple
                 className="hidden"
                 onChange={e => {
                   if (e.target.files?.length) onAddFiles(idx, e.target.files);
                   e.target.value = '';
                 }} />
          <button className={btn}
                  disabled={busy}
                  title='Pick files from your computer to drop into the pack'
                  onClick={() => addInputRef.current?.click()}>
            + Add files…
          </button>
        </div>
      </div>

      <div className={`px-3 py-2 ${dragOver ? 'bg-psion-highlight/10' : ''}`}
           onDragOver={onDragOver}
           onDragLeave={onDragLeave}
           onDrop={onDrop}>
        {slot.files.length === 0 ? (
          <p className="font-mono text-xs text-gray-500 py-2">
            No files yet — click "Add files…" or drag files into this
            area. Psion file types (.wrd, .agn, .spr, .dbf, .opl, .opa …)
            are placed in the matching app folder automatically.
          </p>
        ) : (
          <ul className="font-mono text-xs divide-y divide-psion-accent/20">
            {slot.files.map(f => (
              <li key={f.name} className="flex items-center justify-between py-1.5">
                <span className="truncate text-psion-charcoal flex-grow">{f.name}</span>
                <span className="text-gray-500 mx-3">{formatBytes(f.size)}</span>
                <button className="text-red-600 hover:text-red-800 disabled:opacity-40 text-xs transition-colors"
                        disabled={busy}
                        title='Mark this file as deleted (space is reclaimed when the pack is recreated)'
                        onClick={() => onRemoveFile(idx, f.name)}>
                  remove
                </button>
              </li>
            ))}
          </ul>
        )}
      </div>
    </div>
  );
}

// ── Helpers ───────────────────────────────────────────────────────────

function StatusBadge({ status }: { status: 'empty' | 'staged' | 'inserted' }) {
  if (status === 'inserted') {
    return <span className="font-mono text-[10px] px-2 py-0.5 rounded bg-green-100 border border-green-400 text-green-700">● in device</span>;
  }
  if (status === 'staged') {
    return <span className="font-mono text-[10px] px-2 py-0.5 rounded bg-amber-100 border border-amber-400 text-amber-700">staged</span>;
  }
  return <span className="font-mono text-[10px] px-2 py-0.5 rounded bg-gray-100 border border-gray-300 text-gray-500">empty</span>;
}

function slotLabel(idx: number): string {
  return String.fromCharCode(65 + idx); // 0 -> "A", 1 -> "B"
}

function msg(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}
