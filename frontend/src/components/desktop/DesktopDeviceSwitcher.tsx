// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useMemo, useRef, useState } from 'react';
import type { DeviceProfile } from '../../types/emulator';
import type { SaveGeneration } from '../../lib/desktop/host';
import DeviceCard from '../DeviceCard';

// The device switcher, as an overlay rather than a page.
//
// A borderless window has nowhere to put a header, so switching machines
// happens here: Ctrl/Cmd+K, the ⋯ button in the top strip, or the native
// Device menu. All three land on the same handler, which drives the existing
// hash route — the desktop adds no second switching mechanism.
//
// DeviceCard is reused as-is, so the grid looks like the one on the web and
// stays in step with it.

interface Props {
  open: boolean;
  profiles: DeviceProfile[];
  currentDeviceId: string | null;
  /** Device ids with a saved session, shown with a marker. */
  savedDevices: string[];
  onSelect(profile: DeviceProfile): void;
  onClose(): void;
  /**
   * Saved generations for the running machine, newest first. Absent when the
   * host cannot keep them — the tab is then not offered at all rather than
   * shown empty.
   */
  generations?: SaveGeneration[] | null;
  onRestore?(file: string): Promise<void>;
}

type Tab = 'devices' | 'history';

export default function DesktopDeviceSwitcher({
  open, profiles, currentDeviceId, savedDevices, onSelect, onClose,
  generations, onRestore,
}: Props) {
  const [filter, setFilter] = useState('');
  const [tab, setTab] = useState<Tab>('devices');
  const [restoring, setRestoring] = useState<string | null>(null);
  const inputRef = useRef<HTMLInputElement>(null);
  const historyAvailable = !!onRestore && !!currentDeviceId;

  const usable = useMemo(
    () => profiles.filter((p) => p.status === 'supported' && !p.hiddenFromPicker),
    [profiles],
  );

  const shown = useMemo(() => {
    const q = filter.trim().toLowerCase();
    if (!q) return usable;
    return usable.filter((p) =>
      p.displayName.toLowerCase().includes(q) || p.id.toLowerCase().includes(q));
  }, [usable, filter]);

  // Reset the filter each time it opens: a stale query from last time reads
  // as "most of my devices have vanished".
  useEffect(() => {
    if (!open) return;
    setFilter('');
    setTab('devices');
    // Focus after paint so the first keystroke lands in the box.
    const t = window.setTimeout(() => inputRef.current?.focus(), 0);
    return () => clearTimeout(t);
  }, [open]);

  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') { e.preventDefault(); onClose(); return; }
      // Enter picks the only remaining match, so filtering to one device and
      // pressing Enter is the whole interaction.
      if (e.key === 'Enter' && tab === 'devices' && shown.length === 1) {
        e.preventDefault();
        onSelect(shown[0]);
      }
    };
    // Capture, so the emulator's own key handling does not see these first
    // and feed them to the guest.
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open, shown, tab, onSelect, onClose]);

  if (!open) return null;

  const saved = new Set(savedDevices);

  return (
    <div
      onClick={onClose}
      style={{
        position: 'fixed', inset: 0, zIndex: 550,
        background: 'rgba(20, 20, 18, 0.82)',
        display: 'flex', flexDirection: 'column', alignItems: 'center',
        padding: '28px 16px 16px',
        WebkitAppRegion: 'no-drag',
      } as React.CSSProperties}
    >
      <div
        onClick={(e) => e.stopPropagation()}
        style={{ width: '100%', maxWidth: 880, display: 'flex',
                 flexDirection: 'column', minHeight: 0 }}
      >
        {historyAvailable && (
          <div style={{ display: 'flex', gap: 6, marginBottom: 12 }}>
            <TabButton active={tab === 'devices'} onClick={() => setTab('devices')}>
              Devices
            </TabButton>
            <TabButton active={tab === 'history'} onClick={() => setTab('history')}>
              Earlier sessions{generations?.length ? ` (${generations.length})` : ''}
            </TabButton>
          </div>
        )}

        {tab === 'history' ? (
          <div style={{ overflowY: 'auto', minHeight: 0, paddingBottom: 8 }}>
            {!generations ? (
              <p style={paneNote}>Looking for earlier sessions…</p>
            ) : generations.length === 0 ? (
              <p style={paneNote}>
                No earlier sessions saved for this machine yet. One is written
                about every minute while you use it, and on quit.
              </p>
            ) : (
              <ul style={{ listStyle: 'none', margin: 0, padding: 0,
                           display: 'flex', flexDirection: 'column', gap: 6 }}>
                {generations.map((g, i) => (
                  <li key={g.file} style={{
                    display: 'flex', alignItems: 'center', gap: 12,
                    padding: '8px 10px', borderRadius: 6,
                    background: 'rgba(255,255,255,0.07)',
                    font: '12px ui-monospace, monospace', color: 'white',
                  }}>
                    <span style={{ flex: 1 }}>
                      {new Date(g.savedAt).toLocaleString()}
                      {i === 0 && (
                        <span style={{ marginLeft: 8, opacity: 0.55 }}>most recent</span>
                      )}
                    </span>
                    <span style={{ opacity: 0.55 }}>{formatBytes(g.bytes)}</span>
                    <button
                      type="button"
                      disabled={restoring !== null}
                      onClick={async () => {
                        setRestoring(g.file);
                        try { await onRestore?.(g.file); onClose(); }
                        finally { setRestoring(null); }
                      }}
                      style={{
                        padding: '3px 10px', borderRadius: 4,
                        border: '1px solid rgba(255,255,255,0.3)',
                        background: restoring === g.file
                          ? 'rgba(255,255,255,0.3)' : 'rgba(255,255,255,0.12)',
                        color: 'white', font: '11px ui-monospace, monospace',
                        cursor: restoring ? 'default' : 'pointer',
                      }}
                    >
                      {restoring === g.file ? 'Restoring…' : 'Restore'}
                    </button>
                  </li>
                ))}
              </ul>
            )}
            <p style={{ ...paneNote, marginTop: 12, fontSize: 11, opacity: 0.4 }}>
              Restoring replaces the machine&rsquo;s current state. The session you
              are in now is saved first, so it becomes one of these too.
            </p>
          </div>
        ) : (
        <>
        <input
          ref={inputRef}
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          placeholder="Filter devices…"
          aria-label="Filter devices"
          style={{
            width: '100%', padding: '9px 12px', borderRadius: 8,
            border: '1px solid rgba(255,255,255,0.25)',
            background: 'rgba(255,255,255,0.08)', color: 'white',
            font: '13px ui-monospace, monospace', outline: 'none',
            marginBottom: 14,
          }}
        />

        <div style={{ overflowY: 'auto', minHeight: 0, paddingBottom: 8 }}>
          {shown.length === 0 ? (
            <p style={{ color: 'rgba(255,255,255,0.7)',
                        font: '13px ui-monospace, monospace' }}>
              No device matches “{filter}”.
            </p>
          ) : (
            <div style={{ display: 'grid', gap: 12,
                          gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))' }}>
              {shown.map((p) => (
                <div key={p.id} style={{ position: 'relative' }}>
                  <DeviceCard profile={p} onSelect={onSelect} />
                  {/* Two markers worth having at a glance: which machine is
                      running, and which ones have a session to come back to. */}
                  {p.id === currentDeviceId && (
                    <span style={badge('rgba(90, 140, 90, 0.95)')}>running</span>
                  )}
                  {p.id !== currentDeviceId && saved.has(p.id) && (
                    <span style={badge('rgba(70, 90, 130, 0.95)')}>saved</span>
                  )}
                </div>
              ))}
            </div>
          )}
        </div>

        <p style={{ marginTop: 10, color: 'rgba(255,255,255,0.45)',
                    font: '11px ui-monospace, monospace', userSelect: 'none' }}>
          Esc to close · Enter picks a single match
        </p>
        </>
        )}
      </div>
    </div>
  );
}

const paneNote: React.CSSProperties = {
  color: 'rgba(255,255,255,0.7)',
  font: '12px ui-monospace, monospace',
  margin: 0,
};

function TabButton({ active, onClick, children }: {
  active: boolean; onClick(): void; children: React.ReactNode;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      style={{
        padding: '5px 12px', borderRadius: 6,
        border: '1px solid rgba(255,255,255,0.22)',
        background: active ? 'rgba(255,255,255,0.18)' : 'transparent',
        color: active ? 'white' : 'rgba(255,255,255,0.65)',
        font: '11px ui-monospace, monospace', cursor: 'pointer',
      }}
    >
      {children}
    </button>
  );
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(0)} KB`;
  return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

function badge(background: string): React.CSSProperties {
  return {
    position: 'absolute', top: 6, right: 6,
    padding: '1px 6px', borderRadius: 999,
    background, color: 'white',
    font: '10px ui-monospace, monospace',
    pointerEvents: 'none', userSelect: 'none',
  };
}
