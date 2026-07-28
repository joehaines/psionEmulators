// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useRef, useState } from 'react';
import type { DeviceProfile } from '../types/emulator';
import { getEpocDeliveryPref, setEpocDeliveryPref } from '../lib/appLibrary';
import type { LeaderboardRow } from '../lib/analytics';
import { DEVICE_RELEASE_YEARS, DEVICE_LOGO_MAP } from '../lib/deviceMeta';
import {
  isSiboWord, isSiboWordEncrypted, extractSiboWordText,
} from '../lib/converters/psion-word-sibo';

export type SortMode = 'age-desc' | 'age-asc' | 'popularity-desc' | 'popularity-asc';

interface Props {
  profiles: DeviceProfile[];
  baseUrl: string;
  sortMode: SortMode;
  onSortModeChange(next: SortMode): void;
  nonFavouriteDevices: Set<string>;
  onNonFavouriteDevicesChange(next: Set<string>): void;
  showDebugging: boolean;
  experimentalFeatures: boolean;
  onShowDebuggingChange(next: boolean): void;
  onExperimentalFeaturesChange(next: boolean): void;
  lcdAccuracyMode: boolean;
  onLcdAccuracyModeChange(next: boolean): void;
  // `null` while in flight, `[]` if the request failed / returned empty.
  // Used to surface the live load count next to each device in the
  // favourites checklist when the popularity sort is selected.
  leaderboard: LeaderboardRow[] | null;
  onClose(): void;
}

const SORT_OPTIONS: { value: SortMode; label: string; sub: string }[] = [
  { value: 'age-desc',        label: 'Newest first',     sub: 'By release year, modern devices on top' },
  { value: 'age-asc',         label: 'Oldest first',     sub: 'By release year, vintage devices on top' },
  { value: 'popularity-desc', label: 'Most popular',     sub: 'By total loads from the usage leaderboard' },
  { value: 'popularity-asc',  label: 'Least popular',    sub: 'Rare-loaded devices first' },
];

// Standalone recovery tool for password-protected Psion Word (.WRD)
// documents from a Series 3/3a/3c/Siena/Workabout. Drop in a .WRD file
// and the tool reads the text out, recovering the password automatically;
// if the document is very short the automatic result can be imperfect, so
// an optional "first characters" crib pins the key exactly.
function WordRecoveryTool() {
  const fileRef = useRef<HTMLInputElement>(null);
  const [fileName, setFileName] = useState<string>('');
  const [bytes, setBytes] = useState<Uint8Array | null>(null);
  const [encrypted, setEncrypted] = useState(false);
  const [crib, setCrib] = useState('');
  const [preview, setPreview] = useState('');
  const [status, setStatus] = useState('');

  const onPick = async (file: File | undefined) => {
    setPreview(''); setStatus(''); setCrib('');
    if (!file) return;
    const data = new Uint8Array(await file.arrayBuffer());
    if (!isSiboWord(data)) {
      setBytes(null); setFileName(file.name);
      setStatus('That is not a Psion Series 3 Word (.WRD) file.');
      return;
    }
    setBytes(data);
    setFileName(file.name);
    setEncrypted(isSiboWordEncrypted(data));
    setStatus(isSiboWordEncrypted(data)
      ? 'Password-protected document loaded. Click Recover to read it.'
      : 'Document loaded (no password). Click Recover to read it.');
  };

  const recover = () => {
    if (!bytes) return;
    const cribBytes = crib
      ? new Uint8Array(Array.from(crib, ch =>
          ch === '\n' ? 0x00 : ch === '\t' ? 0x09 : ch.charCodeAt(0) & 0xff))
      : undefined;
    let extracted;
    try {
      extracted = extractSiboWordText(bytes, cribBytes);
    } catch {
      extracted = null;
    }
    if (!extracted) { setStatus('Could not read this file as a Word document.'); return; }
    setPreview(extracted.text);
    if (!extracted.wasEncrypted) {
      setStatus('Text extracted.');
    } else {
      const pct = Math.round(extracted.confidence * 100);
      setStatus(crib
        ? 'Recovered using your first-characters hint.'
        : pct >= 85
          ? `Password recovered automatically (${pct}% clean text).`
          : `Recovered, but the text may be imperfect (${pct}% clean) — enter the document's first few characters below for an exact result.`);
    }
  };

  const download = () => {
    if (!preview) return;
    const blob = new Blob([preview], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName.replace(/\.wrd$/i, '') + '.txt';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  return (
    <div className="p-3 space-y-3">
      <p className="text-[11px] font-mono text-gray-500 leading-tight">
        Forgotten the password on an old Psion Series 3 Word file? Load the
        <span className="text-psion-charcoal"> .WRD</span> file here and its text is
        read straight out — the password is recovered automatically. For very
        short documents, type the first few characters you remember to pin the
        result exactly.
      </p>

      <div className="flex flex-wrap items-center gap-2">
        <input ref={fileRef} type="file" accept=".wrd,.WRD" className="hidden"
               onChange={e => { void onPick(e.target.files?.[0]); e.target.value = ''; }} />
        <button
          type="button"
          onClick={() => fileRef.current?.click()}
          className="px-3 py-1.5 rounded text-xs font-mono cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors">
          Choose .WRD file…
        </button>
        {fileName && (
          <span className="text-[11px] font-mono text-psion-charcoal truncate">
            {fileName}{encrypted && <span className="ml-2 text-amber-600">🔒 protected</span>}
          </span>
        )}
      </div>

      {bytes && (
        <>
          {encrypted && (
            <label className="block">
              <span className="text-[11px] font-mono text-gray-500">
                First characters of the document (optional, improves short files)
              </span>
              <input
                type="text"
                value={crib}
                onChange={e => setCrib(e.target.value)}
                placeholder="e.g. the first words you remember"
                className="mt-1 w-full px-2 py-1 rounded text-xs font-mono bg-white border border-psion-accent/60 text-psion-charcoal focus:border-psion-accent focus:outline-none" />
            </label>
          )}
          <div className="flex gap-2">
            <button
              type="button"
              onClick={recover}
              className="px-3 py-1.5 rounded text-xs font-mono cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors">
              Recover
            </button>
            {preview && (
              <button
                type="button"
                onClick={download}
                className="px-3 py-1.5 rounded text-xs font-mono cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors">
                Download .txt
              </button>
            )}
          </div>
        </>
      )}

      {status && <p className="text-[11px] font-mono text-psion-charcoal">{status}</p>}

      {preview && (
        <textarea
          readOnly
          value={preview}
          className="w-full h-40 px-2 py-1 rounded text-xs font-mono bg-white border border-psion-accent/40 text-psion-charcoal focus:outline-none whitespace-pre" />
      )}
    </div>
  );
}

export default function SettingsView({
  profiles,
  baseUrl,
  sortMode,
  onSortModeChange,
  nonFavouriteDevices,
  onNonFavouriteDevicesChange,
  showDebugging,
  experimentalFeatures,
  onShowDebuggingChange,
  onExperimentalFeaturesChange,
  lcdAccuracyMode,
  onLcdAccuracyModeChange,
  leaderboard,
  onClose,
}: Props) {
  // App-library delivery preference for EPOC32 devices — persisted
  // straight to localStorage; deliverApp reads it at try time.
  const [appDelivery, setAppDelivery] = useState<'link' | 'cf'>(getEpocDeliveryPref);

  const toggleFavourite = (id: string) => {
    const next = new Set(nonFavouriteDevices);
    if (next.has(id)) next.delete(id);
    else next.add(id);
    onNonFavouriteDevicesChange(next);
  };

  const favouriteCount = profiles.filter(p => !nonFavouriteDevices.has(p.id)).length;
  const loadsById = new Map<string, number>();
  for (const row of leaderboard ?? []) loadsById.set(row.deviceId, row.loadCount);
  const showLoads = sortMode === 'popularity-desc' || sortMode === 'popularity-asc';

  return (
    <div className="absolute inset-0 overflow-y-auto bg-psion-dark">
      <div className="max-w-3xl w-full mx-auto px-4 py-6">
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-lg font-mono font-semibold text-psion-charcoal">Settings</h1>
          <button
            onClick={onClose}
            className="text-xs font-mono text-gray-500 hover:text-psion-charcoal transition-colors"
            aria-label="Back to emulator"
          >
            ← Back
          </button>
        </div>

        {/* Options ---------------------------------------------------- */}
        <section className="mb-6 border border-psion-accent/30 rounded-lg overflow-hidden">
          <header className="px-4 py-2 bg-psion-mid border-b border-psion-accent/30">
            <h2 className="text-xs font-mono font-semibold uppercase tracking-wide text-psion-charcoal">
              Options
            </h2>
          </header>
          <div className="p-3 space-y-3">
            <label className="flex items-start gap-2 cursor-pointer select-none">
              <input
                type="checkbox"
                className="mt-0.5 accent-psion-highlight"
                checked={showDebugging}
                onChange={e => onShowDebuggingChange(e.target.checked)}
              />
              <span className="text-xs font-mono text-psion-charcoal leading-tight">
                Show debugging
                <span className="block text-[10px] text-gray-500 leading-tight">
                  Enable Screenshot, Logs, Ram download and Unique id
                </span>
              </span>
            </label>
            <label className="flex items-start gap-2 cursor-pointer select-none">
              <input
                type="checkbox"
                className="mt-0.5 accent-psion-highlight"
                checked={experimentalFeatures}
                onChange={e => onExperimentalFeaturesChange(e.target.checked)}
              />
              <span className="text-xs font-mono text-psion-charcoal leading-tight">
                Experimental features
                <span className="block text-[10px] text-gray-500 leading-tight">
                  Surfaces experimental features
                </span>
              </span>
            </label>
            <label className="flex items-start gap-2 cursor-pointer select-none">
                <input
                  type="checkbox"
                  className="mt-0.5 accent-psion-highlight"
                  checked={lcdAccuracyMode}
                  onChange={e => onLcdAccuracyModeChange(e.target.checked)}
                />
                <span className="text-xs font-mono text-psion-charcoal leading-tight">
                  LCD accuracy mode
                  <span className="block text-[10px] text-gray-500 leading-tight">
                    Renders LCD pixels as translucent dark grey in device mode (greyscale devices only)
                  </span>
                </span>
            </label>

            {/* App-library delivery on EPOC32 devices. Self-contained:
                read at delivery time straight from localStorage
                (lib/appLibrary.getEpocDeliveryPref). */}
            <div className="pt-1">
              <span className="text-xs font-mono text-psion-charcoal leading-tight">
                App delivery (EPOC devices)
                <span className="block text-[10px] text-gray-500 leading-tight">
                  How "Try it" puts an app's installer on Series 5/5mx/MC218/Revo/S7/netBook
                </span>
              </span>
              <div className="flex gap-4 mt-1.5">
                {([['link', 'Remote link (Documents folder)'], ['cf', 'CF card (drive D:)']] as const).map(([v, label]) => (
                  <label key={v} className="flex items-center gap-1.5 cursor-pointer select-none">
                    <input
                      type="radio"
                      name="app-delivery"
                      className="accent-psion-highlight"
                      checked={appDelivery === v}
                      onChange={() => { setAppDelivery(v); setEpocDeliveryPref(v); }}
                    />
                    <span className="text-[11px] font-mono text-psion-charcoal">{label}</span>
                  </label>
                ))}
              </div>
            </div>
          </div>
        </section>

        {/* Word password recovery -------------------------------------- */}
        <section className="mb-6 border border-psion-accent/30 rounded-lg overflow-hidden">
          <header className="px-4 py-2 bg-psion-mid border-b border-psion-accent/30">
            <h2 className="text-xs font-mono font-semibold uppercase tracking-wide text-psion-charcoal">
              Recover a Word file password
            </h2>
            <p className="text-xs font-mono text-gray-500 mt-0.5">
              Read the text out of a password-protected Psion Series 3 Word document.
            </p>
          </header>
          <WordRecoveryTool />
        </section>

        {/* Sort devices ------------------------------------------------- */}
        <section className="mb-6 border border-psion-accent/30 rounded-lg overflow-hidden">
          <header className="px-4 py-2 bg-psion-mid border-b border-psion-accent/30">
            <h2 className="text-xs font-mono font-semibold uppercase tracking-wide text-psion-charcoal">
              Sort devices
            </h2>
            <p className="text-xs font-mono text-gray-500 mt-0.5">
              Reorders the device list in the sidebar.
            </p>
          </header>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-2 p-3">
            {SORT_OPTIONS.map(opt => {
              const active = sortMode === opt.value;
              return (
                <button
                  key={opt.value}
                  type="button"
                  onClick={() => onSortModeChange(opt.value)}
                  aria-pressed={active}
                  className={[
                    'text-left rounded border px-3 py-2 transition-colors',
                    active
                      ? 'bg-psion-highlight border-psion-accent ring-2 ring-psion-accent'
                      : 'border-psion-accent/40 hover:border-psion-accent hover:bg-psion-mid',
                  ].join(' ')}
                >
                  <div className="flex items-center gap-2">
                    <span
                      aria-hidden
                      className={[
                        'inline-flex w-3 h-3 rounded-full border',
                        active
                          ? 'bg-psion-charcoal border-psion-charcoal'
                          : 'border-psion-accent/60',
                      ].join(' ')}
                    />
                    <span className={[
                      'text-xs font-mono font-semibold',
                      active ? 'text-psion-charcoal' : 'text-psion-charcoal',
                    ].join(' ')}>
                      {opt.label}
                    </span>
                    {active && (
                      <span className="ml-auto text-[10px] font-mono uppercase tracking-wide text-psion-charcoal/70">
                        Selected
                      </span>
                    )}
                  </div>
                  <p className="text-[11px] font-mono text-gray-500 mt-1 leading-tight">
                    {opt.sub}
                  </p>
                </button>
              );
            })}
          </div>
          {showLoads && leaderboard === null && (
            <p className="px-4 py-2 text-[11px] font-mono text-gray-500 border-t border-psion-accent/20">
              Loading popularity data from the usage leaderboard…
            </p>
          )}
          {showLoads && leaderboard !== null && leaderboard.length === 0 && (
            <p className="px-4 py-2 text-[11px] font-mono text-gray-500 border-t border-psion-accent/20">
              No popularity data available — devices stay in registry order.
            </p>
          )}
        </section>

        {/* Favourite devices ------------------------------------------- */}
        <section className="mb-6 border border-psion-accent/30 rounded-lg overflow-hidden">
          <header className="px-4 py-2 bg-psion-mid border-b border-psion-accent/30">
            <div className="flex items-center justify-between gap-2">
              <h2 className="text-xs font-mono font-semibold uppercase tracking-wide text-psion-charcoal">
                Favourite devices
              </h2>
              <span className="text-[11px] font-mono text-gray-500">
                {favouriteCount} of {profiles.length} favourited
              </span>
            </div>
            <p className="text-xs font-mono text-gray-500 mt-0.5">
              Unchecked devices move into a collapsed "Other devices" section in the sidebar.
            </p>
          </header>
          <div className="flex items-center justify-end gap-3 px-4 py-2 border-b border-psion-accent/20 bg-psion-mid/40">
            <button
              type="button"
              onClick={() => onNonFavouriteDevicesChange(new Set())}
              className="text-[11px] font-mono text-gray-600 hover:text-psion-charcoal transition-colors"
              title="Mark every device as a favourite"
            >
              Select all
            </button>
            <button
              type="button"
              onClick={() => onNonFavouriteDevicesChange(new Set(profiles.map(p => p.id)))}
              className="text-[11px] font-mono text-gray-600 hover:text-psion-charcoal transition-colors"
              title="Move every device into Other devices"
            >
              Clear all
            </button>
          </div>
          <div>
            {profiles.map(profile => {
              const checked = !nonFavouriteDevices.has(profile.id);
              const year    = DEVICE_RELEASE_YEARS[profile.id];
              const loads   = loadsById.get(profile.id);
              return (
                <label
                  key={profile.id}
                  className="flex items-center gap-3 px-4 py-2 border-b border-psion-accent/20 last:border-b-0 cursor-pointer hover:bg-psion-mid/40 select-none"
                >
                  <input
                    type="checkbox"
                    className="accent-psion-highlight"
                    checked={checked}
                    onChange={() => toggleFavourite(profile.id)}
                  />
                  <div className="relative w-10 h-7 flex-shrink-0 rounded overflow-hidden bg-gray-100 border border-psion-accent/20 flex items-center justify-center">
                    {DEVICE_LOGO_MAP[profile.id] && (
                      <img
                        src={`${baseUrl}device-logos/${DEVICE_LOGO_MAP[profile.id]}`}
                        alt=""
                        className="absolute inset-0 w-full h-full object-contain"
                      />
                    )}
                  </div>
                  <span className="text-xs font-mono text-psion-charcoal truncate flex-1">
                    {profile.displayName}
                  </span>
                  {year !== undefined && (
                    <span className="text-[11px] font-mono text-gray-500 tabular-nums">
                      {year}
                    </span>
                  )}
                  {showLoads && (
                    <span
                      className="text-[11px] font-mono text-gray-500 tabular-nums w-14 text-right"
                      title={`${loads ?? 0} total loads`}
                    >
                      {loads ?? 0} loads
                    </span>
                  )}
                </label>
              );
            })}
          </div>
        </section>
      </div>
    </div>
  );
}
