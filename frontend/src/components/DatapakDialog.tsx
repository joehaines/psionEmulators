// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useState, useEffect, useCallback, type DragEvent } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';

interface Props {
  controls: EmulatorControls;
  slotCount: number;
  onClose: () => void;
}

// Pack-kind discriminant returned by getDatapakKind(). Kept in sync
// with EmuBase::PackKind in core/emubase.h.
const KIND_NONE    = 0;
const KIND_DATAPAK = 1;
const KIND_RAMPAK  = 2;

function kindLabel(k: number): string {
  switch (k) {
    case KIND_DATAPAK: return 'Datapak (read-only)';
    case KIND_RAMPAK:  return 'Rampak (writable)';
    default:           return 'Empty';
  }
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

const btn        = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-40 disabled:cursor-not-allowed';
const btnPrimary = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-40 disabled:cursor-not-allowed';
const btnDanger  = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-mid border border-red-400 text-red-600 hover:bg-red-50 hover:border-red-600 transition-colors disabled:opacity-40 disabled:cursor-not-allowed';

interface SlotView {
  attached: boolean;
  kind:     number;
  size:     number;
}

export default function DatapakDialog({ controls, slotCount, onClose }: Props) {
  const [slots, setSlots] = useState<SlotView[]>(() =>
    Array.from({ length: slotCount }, () => ({ attached: false, kind: KIND_NONE, size: 0 })));
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState('');

  const refresh = useCallback(() => {
    setSlots(Array.from({ length: slotCount }, (_, i) => {
      const attached = controls.datapakAttached[i] ?? false;
      const bytes    = attached ? controls.getDatapakBytes(i) : null;
      return {
        attached,
        kind:  attached ? controls.getDatapakKind(i) : KIND_NONE,
        size:  bytes ? bytes.byteLength : 0,
      };
    }));
  }, [controls, slotCount]);

  useEffect(() => { refresh(); }, [refresh]);

  const onUpload = useCallback(async (slot: number, file: File) => {
    setBusy(true);
    setStatus(`Uploading to Pack ${String.fromCharCode(65 + slot)}…`);
    try {
      const buf  = await file.arrayBuffer();
      const ok   = await controls.attachDatapak(slot, new Uint8Array(buf));
      setStatus(ok ? `Pack ${String.fromCharCode(65 + slot)}: ${file.name}`
                   : `Failed to attach ${file.name}`);
      refresh();
    } finally { setBusy(false); }
  }, [controls, refresh]);

  const onEject = useCallback(async (slot: number) => {
    setBusy(true);
    try {
      await controls.detachDatapak(slot);
      refresh();
      setStatus(`Pack ${String.fromCharCode(65 + slot)}: ejected`);
    } finally { setBusy(false); }
  }, [controls, refresh]);

  const onDownload = useCallback((slot: number) => {
    const bytes = controls.getDatapakBytes(slot);
    if (!bytes) return;
    const tag = controls.getDatapakKind(slot) === KIND_RAMPAK ? 'rampak' : 'datapak';
    downloadBlob(bytes, `pack${String.fromCharCode(65 + slot)}-${tag}.opk`);
  }, [controls]);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60"
         onClick={onClose}>
      <div className="bg-psion-dark border border-psion-accent/40 rounded-lg p-4 w-[520px] max-w-[95vw] max-h-[85vh] overflow-y-auto shadow-xl"
           onClick={e => e.stopPropagation()}>
        <div className="flex justify-between items-center mb-3">
          <h2 className="text-sm font-mono font-semibold text-psion-charcoal">Datapak / Rampak</h2>
          <button className={btn} onClick={onClose}>Close</button>
        </div>

        <p className="text-xs font-mono text-gray-500 mb-4 leading-relaxed">
          Upload an <code>.opk</code> file to plug a pack into Slot A or B.
          Datapaks are read-only EPROM modules; Rampaks are battery-backed
          writable SRAM. The pack kind is detected from the image header.
          Eject saves the current pack state back to browser storage.
        </p>

        {slots.map((s, slot) => (
          <SlotRow
            key={slot}
            slot={slot}
            view={s}
            busy={busy}
            onUpload={onUpload}
            onEject={onEject}
            onDownload={onDownload}
          />
        ))}

        {status && (
          <div className="text-xs font-mono text-gray-500 mt-3">{status}</div>
        )}
      </div>
    </div>
  );
}

interface RowProps {
  slot: number;
  view: SlotView;
  busy: boolean;
  onUpload(slot: number, file: File): void;
  onEject(slot: number): void;
  onDownload(slot: number): void;
}

function SlotRow({ slot, view, busy, onUpload, onEject, onDownload }: RowProps) {
  const label = `Pack ${String.fromCharCode(65 + slot)}`;
  const onPick = (e: React.ChangeEvent<HTMLInputElement>) => {
    const f = e.target.files?.[0];
    if (f) onUpload(slot, f);
    e.target.value = '';
  };
  const onDrop = (e: DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    const f = e.dataTransfer.files?.[0];
    if (f) onUpload(slot, f);
  };
  return (
    <div
      className="border border-psion-accent/40 rounded p-3 mb-3 bg-psion-mid/40"
      onDragOver={e => e.preventDefault()}
      onDrop={onDrop}
    >
      <div className="flex justify-between items-center mb-2">
        <span className="text-xs font-mono text-psion-charcoal font-medium">{label}</span>
        <span className="text-xs font-mono text-gray-500">
          {view.attached ? `${kindLabel(view.kind)} · ${formatBytes(view.size)}` : 'Empty'}
        </span>
      </div>
      <div className="flex gap-2 flex-wrap">
        <label className={`${btn} ${busy ? 'pointer-events-none opacity-50' : ''}`}>
          Upload .opk
          <input type="file" accept=".opk,.bin,application/octet-stream"
                 className="hidden" onChange={onPick} disabled={busy} />
        </label>
        <button
          className={btnPrimary}
          onClick={() => onDownload(slot)}
          disabled={busy || !view.attached}
        >
          Download
        </button>
        <button
          className={btnDanger}
          onClick={() => onEject(slot)}
          disabled={busy || !view.attached}
        >
          Eject
        </button>
      </div>
    </div>
  );
}
