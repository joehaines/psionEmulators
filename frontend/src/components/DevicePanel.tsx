// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useRef, useState } from 'react';
import type { DeviceProfile } from '../types/emulator';
import { DEVICE_LOGO_MAP } from '../lib/deviceMeta';

// Known brand prefixes, longest first so "Oregon Scientific" matches
// before any single-word prefix could.
const BRAND_PREFIXES = ['Oregon Scientific ', 'Acorn ', 'Ericsson ', 'Psion '];

function renderName(displayName: string) {
  const prefix = BRAND_PREFIXES.find(p => displayName.startsWith(p));
  if (prefix) {
    return (
      <>{prefix.trimEnd()} <span className="font-bold">{displayName.slice(prefix.length)}</span></>
    );
  }
  return <span className="font-bold">{displayName}</span>;
}

interface Props {
  profiles: DeviceProfile[];
  currentDeviceId: string | null;
  savedDevices: string[];
  isOpen: boolean;
  onClose(): void;
  onSelect(deviceId: string, romUrl: string): void;
  // The session-management handlers are optional: the app library hosts
  // this panel outside the emulator, where clear/export/import/settings
  // don't apply — their UI is hidden when a handler is absent.
  onClearSession?(deviceId: string): Promise<void>;
  onDownloadStates?(): Promise<void>;
  onImportStates?(file: File): Promise<{ imported: number; errors: string[] }>;
  baseUrl: string;
  // IDs the user has explicitly opted out of as favourites. Decided in
  // the main Settings view; we only read it here to bucket the list into
  // Favourites vs. Other devices.
  nonFavouriteDevices: Set<string>;
  // Opens the main-area Settings view, which now owns the favourites
  // picker plus the show-debugging / experimental / sort toggles.
  onOpenSettings?(): void;
  // Why the device list is empty, when it is (engine/worker failure). Shown
  // in place of the indefinite "Loading…" so the panel never sits silently
  // blank — the symptom this replaces was an empty menu with no explanation.
  listError?: string | null;
}

export default function DevicePanel({
  profiles,
  currentDeviceId,
  savedDevices,
  isOpen,
  onClose,
  onSelect,
  onClearSession,
  onDownloadStates,
  onImportStates,
  baseUrl,
  nonFavouriteDevices,
  onOpenSettings,
  listError,
}: Props) {
  const [confirmClearId, setConfirmClearId] = useState<string | null>(null);
  const [importStatus, setImportStatus] = useState<string | null>(null);
  const [downloading, setDownloading] = useState(false);
  const [otherDevicesOpen, setOtherDevicesOpen] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  // Preserve the caller-supplied order (already sorted by App.tsx
  // according to the user's chosen sort mode) while splitting the list
  // into the two buckets the side menu renders.
  const favouriteProfiles = profiles.filter(p => !nonFavouriteDevices.has(p.id));
  const otherProfiles     = profiles.filter(p =>  nonFavouriteDevices.has(p.id));

  const handleSelect = (profile: DeviceProfile) => {
    if (profile.status !== 'supported') return;
    onSelect(profile.id, `${baseUrl}roms/${profile.romFilename}`);
  };

  const handleDownload = async () => {
    if (!onDownloadStates) return;
    setDownloading(true);
    try {
      await onDownloadStates();
    } finally {
      setDownloading(false);
    }
  };

  const handleFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file || !onImportStates) return;
    e.target.value = '';
    setImportStatus('Importing…');
    try {
      const { imported, errors } = await onImportStates(file);
      if (errors.length === 0) {
        setImportStatus(`Restored ${imported} device${imported === 1 ? '' : 's'}. Reload or switch device to apply.`);
      } else {
        setImportStatus(`Restored ${imported}, ${errors.length} error(s): ${errors.join('; ')}`);
      }
    } catch (err) {
      setImportStatus(`Import failed: ${String(err)}`);
    }
    setTimeout(() => setImportStatus(null), 6000);
  };

  return (
    <>
      {/* Backdrop */}
      {isOpen && (
        <div
          className="fixed inset-0 bg-black/50 z-40"
          onClick={onClose}
        />
      )}

      {/* Slide-in panel */}
      <div
        className={[
          'fixed top-0 left-0 h-full z-50 flex flex-col',
          'bg-psion-dark border-r border-psion-accent/40',
          'w-72 shadow-2xl',
          'transition-transform duration-200 ease-in-out',
          isOpen ? 'translate-x-0' : '-translate-x-full pointer-events-none',
        ].join(' ')}
      >
        {/* Header */}
        <div className="flex items-center justify-between px-4 py-3 border-b border-psion-accent/40 flex-shrink-0 bg-psion-mid">
          <span className="text-sm font-mono font-semibold text-psion-charcoal">
            Select Device
          </span>
          <button
            onClick={onClose}
            className="text-gray-400 hover:text-psion-charcoal transition-colors text-lg leading-none"
            aria-label="Close panel"
          >
            ✕
          </button>
        </div>

        {/* Device list */}
        <div className="flex-1 overflow-y-auto py-2">
          {/* App library — promoted above the device rows as the panel's
              one non-device destination. */}
          <div className="px-3 pt-1 pb-2 border-b border-psion-accent/20 mb-2">
            <a
              href="#/apps"
              className="block w-full text-center text-xs font-mono font-semibold px-3 py-1.5 rounded bg-psion-highlight border border-psion-accent text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors"
              title="Browse the software library and try apps on the emulated devices"
            >
              ▸ App library
            </a>
          </div>

          {profiles.length === 0 && (
            listError ? (
              <div className="px-4 py-3 flex flex-col gap-2">
                <p className="text-red-600 font-mono text-xs">Device list failed to load.</p>
                <p className="text-gray-500 font-mono text-[11px] break-words">{listError}</p>
                <button
                  onClick={() => window.location.reload()}
                  className="self-start px-3 py-1 rounded bg-psion-highlight text-psion-charcoal font-mono text-xs hover:brightness-95 transition"
                >
                  Reload
                </button>
              </div>
            ) : (
              <p className="text-gray-400 font-mono text-xs px-4 py-3">Loading…</p>
            )
          )}

          {favouriteProfiles.map(profile => renderDeviceRow({
            profile,
            currentDeviceId,
            savedDevices,
            baseUrl,
            confirmClearId,
            setConfirmClearId,
            onSelect: handleSelect,
            onClearSession,
          }))}

          {otherProfiles.length > 0 && (
            <div className="border-t border-psion-accent/30">
              <button
                type="button"
                onClick={() => setOtherDevicesOpen(o => !o)}
                className="w-full flex items-center justify-between px-4 py-2 text-xs font-mono text-gray-400 font-semibold uppercase tracking-wide hover:text-psion-charcoal transition-colors bg-psion-mid/40"
                aria-expanded={otherDevicesOpen}
              >
                <span>Other devices ({otherProfiles.length})</span>
                <span aria-hidden className="text-gray-400">{otherDevicesOpen ? '▾' : '▸'}</span>
              </button>
              {otherDevicesOpen && otherProfiles.map(profile => renderDeviceRow({
                profile,
                currentDeviceId,
                savedDevices,
                baseUrl,
                confirmClearId,
                setConfirmClearId,
                onSelect: handleSelect,
                onClearSession,
              }))}
            </div>
          )}
        </div>

        {/* State file management footer */}
        <div className="px-4 py-3 border-t border-psion-accent/20 flex-shrink-0 bg-psion-mid space-y-2">
          {onDownloadStates && onImportStates && (<>
          <p className="text-xs font-mono text-gray-400 font-semibold uppercase tracking-wide">
            State Files
          </p>
          <div className="flex gap-2">
            <button
              onClick={handleDownload}
              disabled={downloading || savedDevices.length === 0}
              className={[
                'flex-1 text-xs font-mono px-2 py-1 rounded border transition-colors',
                savedDevices.length === 0
                  ? 'border-psion-accent/30 text-gray-500 cursor-not-allowed'
                  : 'border-psion-accent text-gray-600 hover:bg-psion-accent hover:text-white',
              ].join(' ')}
              title={savedDevices.length === 0 ? 'No saved states to export' : `Export all ${savedDevices.length} saved state(s) as a bundle (saved locally, nothing leaves your browser)`}
            >
              {downloading ? 'Exporting…' : 'Export all'}
            </button>
            <button
              onClick={() => fileInputRef.current?.click()}
              className="flex-1 text-xs font-mono px-2 py-1 rounded border border-psion-accent text-gray-600 hover:bg-psion-accent hover:text-white transition-colors"
              title="Import a previously exported states bundle from your computer"
            >
              Import
            </button>
            <input
              ref={fileInputRef}
              type="file"
              // Don't filter strictly — mobile Safari and many Android
              // file pickers hide third-party storage providers (Drive,
              // Files, Dropbox) when `accept` doesn't enumerate a MIME
              // type they recognise, and a custom extension like
              // `.psionstate` is in neither the magic MIME map nor the
              // built-in app whitelists. Empty `accept` lets every
              // provider show its full file list; we sniff the magic
              // bytes ourselves in decodeBundle.
              className="hidden"
              onChange={handleFileChange}
            />
          </div>
          {importStatus && (
            <p className="text-xs font-mono text-psion-charcoal leading-tight">{importStatus}</p>
          )}
          <p className="text-xs font-mono text-gray-400">
            Use "Save State" to save progress
          </p>
          </>)}

          {/* Settings — opens the full-main-area Settings view, which
              hosts the favourites picker, sort options and the
              debugging / experimental toggles that used to live in this
              cramped sidebar collapsible. */}
          {onOpenSettings && <div className="pt-3 mt-1 border-t border-psion-accent/20">
            <button
              type="button"
              onClick={onOpenSettings}
              className="w-full text-left text-xs font-mono px-2 py-1 rounded border border-psion-accent text-gray-600 hover:bg-psion-accent hover:text-white transition-colors"
            >
              Settings…
            </button>
          </div>}

          <a
            href="#/usage"
            className="block text-xs font-mono text-gray-500 hover:text-psion-accent transition-colors pt-2"
          >
            → Usage leaderboard
          </a>
        </div>
      </div>
    </>
  );
}

interface DeviceRowParams {
  profile: DeviceProfile;
  currentDeviceId: string | null;
  savedDevices: string[];
  baseUrl: string;
  confirmClearId: string | null;
  setConfirmClearId(id: string | null): void;
  onSelect(profile: DeviceProfile): void;
  onClearSession?(deviceId: string): Promise<void>;
}

function renderDeviceRow({
  profile,
  currentDeviceId,
  savedDevices,
  baseUrl,
  confirmClearId,
  setConfirmClearId,
  onSelect,
  onClearSession,
}: DeviceRowParams) {
  const supported = profile.status === 'supported';
  const active    = profile.id === currentDeviceId;
  const hasSaved  = savedDevices.includes(profile.id);

  return (
    <div
      key={profile.id}
      className={[
        'flex items-center gap-3 px-4 py-3 border-b border-psion-accent/20',
        supported ? 'group' : 'opacity-40',
      ].join(' ')}
    >
      {/* Thumbnail */}
      <div className="relative w-14 h-10 flex-shrink-0 rounded overflow-hidden bg-gray-100 border border-psion-accent/20 flex items-center justify-center">
        {DEVICE_LOGO_MAP[profile.id] ? (
          <img
            src={`${baseUrl}device-logos/${DEVICE_LOGO_MAP[profile.id]}`}
            alt=""
            className="absolute inset-0 w-full h-full object-contain"
          />
        ) : (
          <span className="text-gray-400 text-xs font-mono">
            {profile.displayName.split(' ').map(w => w[0]).join('').slice(0, 3)}
          </span>
        )}
      </div>

      {/* Name + action */}
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-1.5">
          {active && (
            <span className="w-1.5 h-1.5 rounded-full bg-psion-highlight flex-shrink-0" />
          )}
          <p className={[
            'text-xs font-mono truncate',
            active ? 'text-psion-charcoal font-semibold' : supported ? 'text-psion-charcoal' : 'text-gray-400',
          ].join(' ')}>
            {renderName(profile.displayName)}
          </p>
          {hasSaved && (
            <span
              className="text-green-500 text-xs leading-none flex-shrink-0"
              title="Saved state in browser storage"
            >
              ●
            </span>
          )}
        </div>

        {supported ? (
          <div className="flex items-center gap-2 mt-1 flex-wrap">
            <button
              onClick={() => onSelect(profile)}
              className={[
                'text-xs font-mono px-2 py-0.5 rounded border transition-colors',
                active
                  ? 'bg-psion-highlight text-psion-charcoal border-psion-accent hover:bg-psion-accent hover:text-white'
                  : 'border-psion-accent text-gray-600 hover:bg-psion-accent hover:text-white',
              ].join(' ')}>
              {active ? 'Running' : 'Load'}
            </button>

            {onClearSession && (confirmClearId === profile.id ? (
              <span className="flex items-center gap-1.5">
                <span className="text-xs font-mono text-gray-500">Sure?</span>
                <button
                  onClick={async () => {
                    await onClearSession(profile.id);
                    setConfirmClearId(null);
                  }}
                  className="text-xs font-mono text-red-600 hover:text-red-800 transition-colors"
                >
                  Yes
                </button>
                <button
                  onClick={() => setConfirmClearId(null)}
                  className="text-xs font-mono text-gray-400 hover:text-gray-600 transition-colors"
                >
                  No
                </button>
              </span>
            ) : (
              <button
                onClick={() => setConfirmClearId(profile.id)}
                className="text-xs font-mono text-gray-400 hover:text-amber-700 transition-colors"
                title="Delete saved state for this device"
              >
                Clear state
              </button>
            ))}
          </div>
        ) : (
          <p className="text-xs font-mono text-gray-400 mt-0.5">Coming soon</p>
        )}
      </div>
    </div>
  );
}
