// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useState } from 'react';
import type { MountInfo } from '../../lib/desktop/host';
import { requireHost } from '../../lib/desktop/host';
import { familyLabel, familySupportsProjection, type CardFamily } from '../../lib/extdrive/project';
import type { ExternalDriveStatus } from './ExternalDriveController';
import type { InternalMountStatus } from './InternalMountController';

// The shared card folder, and what the machine currently makes of it.
//
// A borderless window has no menu bar to hang this off on Windows, so it is an
// overlay reached from the Drives menu, Ctrl/Cmd+D, or the top strip.
//
// The panel's job beyond the folder picker is to be honest about the things
// that are easy to hide: which files did not fit, and which machines cannot
// take a shared card at all. A file silently missing from a card is the failure
// mode this feature would otherwise have.

interface Props {
  open: boolean;
  onClose(): void;
  family: CardFamily;
  status: ExternalDriveStatus | null;
  syncStatus: InternalMountStatus | null;
  /** Whether this machine's ROM speaks a link protocol the emulator can drive. */
  canLink: boolean;
  deviceName: string | null;
}

export default function DrivesPanel({
  open, onClose, family, status, syncStatus, canLink, deviceName,
}: Props) {
  const host = requireHost();
  const [mount, setMount] = useState<MountInfo | null>(null);
  const [internalMount, setInternalMount] = useState<MountInfo | null>(null);
  const [working, setWorking] = useState(false);

  const refresh = useCallback(async () => {
    const mounts = host.mounts;
    if (!mounts) return;
    const all = await mounts.get();
    setMount(all.external);
    setInternalMount(all.internal);
  }, [host]);

  useEffect(() => { if (open) void refresh(); }, [open, refresh]);

  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') { e.preventDefault(); onClose(); }
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [open, onClose]);

  if (!open) return null;
  const mounts = host.mounts;

  const choose = async () => {
    if (!mounts) return;
    setWorking(true);
    try { setMount(await mounts.pick('external')); } finally { setWorking(false); }
  };
  const chooseInternal = async () => {
    if (!mounts) return;
    setWorking(true);
    try { setInternalMount(await mounts.pick('internal')); } finally { setWorking(false); }
  };

  const supported = familySupportsProjection(family);

  return (
    <div
      onClick={onClose}
      style={{
        position: 'fixed', inset: 0, zIndex: 550,
        background: 'rgba(20, 20, 18, 0.82)',
        display: 'flex', alignItems: 'flex-start', justifyContent: 'center',
        padding: '40px 16px 16px',
        WebkitAppRegion: 'no-drag',
      } as React.CSSProperties}
    >
      <div
        onClick={(e) => e.stopPropagation()}
        style={{
          width: '100%', maxWidth: 620, color: 'white',
          font: '12px ui-monospace, monospace',
          display: 'flex', flexDirection: 'column', gap: 14,
        }}
      >
        <h2 style={{ margin: 0, fontSize: 15, fontWeight: 600 }}>Shared card folder</h2>
        <p style={{ margin: 0, opacity: 0.7, lineHeight: 1.5 }}>
          One folder that follows you between machines. It is projected into
          whatever card the machine in front of you takes &mdash; a CompactFlash
          image for the ARM machines, an SSD pack for the Series&nbsp;3 family
          &mdash; and read back when you switch away, so a file dropped in here
          turns up on whichever Psion you open next.
        </p>

        {!mounts ? (
          <Row>This build cannot mount host folders.</Row>
        ) : (
          <>
            <Row>
              <span style={{ opacity: 0.6, marginRight: 8 }}>Folder</span>
              <span style={{ flex: 1, wordBreak: 'break-all' }}>
                {mount?.path ?? <em style={{ opacity: 0.6 }}>none chosen</em>}
              </span>
            </Row>

            <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
              <Button onClick={choose} disabled={working}>
                {mount?.path ? 'Choose a different folder…' : 'Choose a folder…'}
              </Button>
              {mount?.path && (
                <>
                  <Button onClick={() => void mounts.reveal('external')}>
                    Reveal in {host.info.platform === 'darwin' ? 'Finder' : 'Explorer'}
                  </Button>
                  <Button
                    onClick={async () => {
                      await mounts.setEnabled('external', !(mount?.enabled ?? false));
                      await refresh();
                    }}
                  >
                    {mount.enabled ? 'Stop using it' : 'Start using it'}
                  </Button>
                </>
              )}
            </div>

            <Row>
              <span style={{ opacity: 0.6, marginRight: 8 }}>This machine</span>
              <span style={{ flex: 1 }}>
                {supported
                  ? `takes ${familyLabel(family)}`
                  : familyLabel(family) === 'Datapak'
                    ? 'takes Datapaks, which need a pack filesystem this build does not have yet'
                    : 'has no removable slot, so the shared card cannot appear on it'}
              </span>
            </Row>

            {status && supported && (
              <Row>
                <span style={{ opacity: 0.6, marginRight: 8 }}>Status</span>
                <span style={{ flex: 1 }}>
                  {status.busy ? 'working…' : status.error ?? status.summary}
                </span>
              </Row>
            )}

            {/* The important one. A file that silently did not make it onto the
                card is the failure this feature would otherwise hide. */}
            {status && status.skipped.length > 0 && (
              <div style={{
                padding: '10px 12px', borderRadius: 6,
                background: 'rgba(180, 120, 40, 0.22)',
                border: '1px solid rgba(220, 160, 60, 0.4)',
              }}>
                <p style={{ margin: '0 0 6px', fontWeight: 600 }}>
                  {status.skipped.length} file(s) did not fit
                </p>
                <ul style={{ margin: 0, paddingLeft: 18, lineHeight: 1.6, opacity: 0.85 }}>
                  {status.skipped.slice(0, 8).map((s) => (
                    <li key={s.rel}>
                      {s.rel} &mdash; {s.reason === 'too-big'
                        ? 'larger than the whole card'
                        : s.reason === 'no-space' ? 'no room left'
                        : s.reason === 'unsupported-family' ? 'this machine cannot take one'
                        : s.detail ?? 'rejected'}
                    </li>
                  ))}
                  {status.skipped.length > 8 && (
                    <li style={{ opacity: 0.7 }}>…and {status.skipped.length - 8} more</li>
                  )}
                </ul>
              </div>
            )}
          </>
        )}

        <p style={{ margin: 0, opacity: 0.45, fontSize: 11 }}>
          Names are shortened to 8.3 on the card, as those filesystems require,
          and restored when they come back.
        </p>

        <hr style={{ border: 0, borderTop: '1px solid rgba(255,255,255,0.15)', margin: 0 }} />

        <h2 style={{ margin: 0, fontSize: 15, fontWeight: 600 }}>
          This machine&rsquo;s own drive
        </h2>
        {/* Said plainly, because the honest description is not the flattering
            one: measured at 0.81 KB/s on a real 5mx, this is a folder that gets
            synced, not a drive that gets mounted, and a user who expects the
            latter will think it is broken. */}
        <p style={{ margin: 0, opacity: 0.7, lineHeight: 1.5 }}>
          A folder kept in step with the machine&rsquo;s own drive over the
          emulated Remote Link cable, so a document you drop in turns up in
          Word. The cable is slow &mdash; under a kilobyte a second &mdash; so
          this suits documents, not large files, and changes take a few seconds
          to cross. It is a sync, not a mounted drive.
        </p>

        {!mounts ? null : !canLink ? (
          <Row>
            <span style={{ flex: 1 }}>
              {deviceName ?? 'This machine'} has no cable link this emulator can
              drive, so use the shared card above instead.
            </span>
          </Row>
        ) : (
          <>
            <Row>
              <span style={{ opacity: 0.6, marginRight: 8 }}>Folder</span>
              <span style={{ flex: 1, wordBreak: 'break-all' }}>
                {internalMount?.path ?? <em style={{ opacity: 0.6 }}>none chosen</em>}
              </span>
            </Row>
            <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
              <Button onClick={chooseInternal} disabled={working}>
                {internalMount?.path ? 'Choose a different folder…' : 'Choose a folder…'}
              </Button>
              {internalMount?.path && (
                <>
                  <Button onClick={() => void mounts.reveal('internal')}>
                    Reveal in {host.info.platform === 'darwin' ? 'Finder' : 'Explorer'}
                  </Button>
                  <Button
                    onClick={async () => {
                      await mounts.setEnabled('internal', !(internalMount?.enabled ?? false));
                      await refresh();
                    }}
                  >
                    {internalMount.enabled ? 'Stop syncing' : 'Start syncing'}
                  </Button>
                </>
              )}
            </div>
            {syncStatus && (
              <Row>
                <span style={{ opacity: 0.6, marginRight: 8 }}>Status</span>
                <span style={{
                  flex: 1,
                  color: syncStatus.actionable ? '#ffd28a' : undefined,
                }}>
                  {syncStatus.summary}
                </span>
              </Row>
            )}
            {/* Files the machine would not hand over — a document it has open,
                usually. Visible rather than silently missing. */}
            {syncStatus && syncStatus.warnings.length > 0 && (
              <ul style={{ margin: 0, paddingLeft: 18, lineHeight: 1.6,
                           opacity: 0.75, fontSize: 11 }}>
                {syncStatus.warnings.slice(0, 5).map((w) => <li key={w}>{w}</li>)}
              </ul>
            )}
          </>
        )}

        <p style={{ margin: 0, opacity: 0.45, fontSize: 11 }}>
          Esc to close.
        </p>
      </div>
    </div>
  );
}

function Row({ children }: { children: React.ReactNode }) {
  return (
    <div style={{
      display: 'flex', alignItems: 'baseline',
      padding: '8px 10px', borderRadius: 6,
      background: 'rgba(255,255,255,0.07)',
    }}>
      {children}
    </div>
  );
}

function Button({ onClick, disabled, children }: {
  onClick(): void | Promise<void>; disabled?: boolean; children: React.ReactNode;
}) {
  return (
    <button
      type="button"
      disabled={disabled}
      onClick={() => void onClick()}
      style={{
        padding: '5px 12px', borderRadius: 5,
        border: '1px solid rgba(255,255,255,0.28)',
        background: disabled ? 'rgba(255,255,255,0.06)' : 'rgba(255,255,255,0.14)',
        color: 'white', font: '11px ui-monospace, monospace',
        cursor: disabled ? 'default' : 'pointer',
      }}
    >
      {children}
    </button>
  );
}
