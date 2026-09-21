// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useRef, useState } from 'react';
import EmulatorView from '../EmulatorView';
import type { DeviceProfile } from '../../types/emulator';
import type { SaveGeneration } from '../../lib/desktop/host';
import type { EmulatorControls } from '../../hooks/useEmulator';
import { useEmulator, applyStatesBundle, hasStoredState } from '../../hooks/useEmulator';
import { useEmulatorWorker } from '../../hooks/useEmulatorWorker';
import { useBacklight } from '../../hooks/useBacklight';
import { prefersSkinnedView } from '../../lib/deviceKeys';
import { familyFor } from '../../lib/extdrive/project';
import { requireHost } from '../../lib/desktop/host';
import AutoSaveController from './AutoSaveController';
import DrivesPanel from './DrivesPanel';
import ExternalDriveController, { type ExternalDriveStatus } from './ExternalDriveController';
import InternalMountController, { type InternalMountStatus } from './InternalMountController';
import DesktopChrome from './DesktopChrome';
import DeviceKeyStrip from './DeviceKeyStrip';
import DesktopDeviceSwitcher from './DesktopDeviceSwitcher';

// The desktop shell: a borderless window that is nothing but the machine.
//
// The emulator itself is the existing chromeless EmulatorView — the same
// recipe the #/embed/<id> route already uses — so no rendering code is
// duplicated here. What this adds is the things a window needs and a web page
// does not: an aspect lock tied to the machine on screen, geometry that
// persists per device, a switcher with no header to live in, and a home for
// the keys the missing bezel used to carry.
//
// Worker mode is required rather than preferred. The main-thread path gzips a
// 128 MB heap on the same thread as the render loop, which is the starvation
// the device-switch comment in useEmulator describes; in a long-lived desktop
// session that would be a recurring stall rather than a one-off.

const PREF_KEY_STRIP = 'ui.keyStripVisible';
const PREF_SKINNED = 'ui.skinnedView';

/** Electron's Chromium always satisfies this; the check is for honesty. */
function workerModeAvailable(): boolean {
  try {
    return typeof Worker !== 'undefined'
      && typeof OffscreenCanvas !== 'undefined'
      && typeof HTMLCanvasElement !== 'undefined'
      && typeof HTMLCanvasElement.prototype.transferControlToOffscreen === 'function';
  } catch { return false; }
}

export default function DesktopApp() {
  if (!workerModeAvailable()) return <UnsupportedRuntime />;
  return <DesktopShell />;
}

function UnsupportedRuntime() {
  return (
    <div style={{
      position: 'fixed', inset: 0, display: 'flex', alignItems: 'center',
      justifyContent: 'center', padding: 24, textAlign: 'center',
      font: '13px ui-monospace, monospace', color: '#3a3a36', background: '#c9c9bd',
    }}>
      This build needs off-main-thread emulation (Worker + OffscreenCanvas),
      which this runtime does not provide.
    </div>
  );
}

/** Where useEmulatorWorker looks for the machine to resume. */
const LAST_DEVICE_KEY = 'psion-last-device';

function DesktopShell() {
  // The emulator hook auto-loads the last device the moment it mounts, so any
  // restore-from-disk has to happen BEFORE that — hence a gate rather than an
  // effect inside the shell. Mounting Shell is what starts the emulator.
  const [ready, setReady] = useState(false);

  useEffect(() => {
    let alive = true;
    void (async () => {
      try {
        await recoverStateFromDisk();
      } catch (err) {
        // A failed recovery must not stop the app starting; the machine just
        // cold-boots, which is what would have happened anyway.
        console.error('[psion] restoring a saved session from disk failed:', err);
      }
      if (alive) setReady(true);
    })();
    return () => { alive = false; };
  }, []);

  if (!ready) {
    return (
      <div style={{
        position: 'fixed', inset: 0, display: 'flex', alignItems: 'center',
        justifyContent: 'center', font: '12px ui-monospace, monospace',
        color: '#3a3a36', background: '#c9c9bd',
      }}>
        Checking for a saved session…
      </div>
    );
  }
  // A separate component, not a hook call in this body: DesktopShell returns
  // early while the check runs, so calling useEmulatorWorker from here would
  // change the hook count between renders.
  return <WorkerShell />;
}

function WorkerShell() {
  const controls = useEmulatorWorker({});
  return <Shell controls={controls} />;
}

/**
 * If IndexedDB has no session for the machine we are about to resume but disk
 * does, import the newest generation into IDB so the normal restore path
 * picks it up.
 *
 * This is the case the disk mirror exists for: a wiped browser profile, an
 * evicted origin, or a fresh install alongside an older one. IDB stays
 * authoritative whenever it has anything — it is the live store, and a
 * generation file is by definition no newer than it.
 */
async function recoverStateFromDisk(): Promise<void> {
  const host = requireHost();
  const saves = host.saves;
  if (!saves) return;

  let deviceId: string | null = null;
  try { deviceId = localStorage.getItem(LAST_DEVICE_KEY); } catch { return; }
  if (!deviceId) return;

  if (await hasStoredState(deviceId)) return;

  const generations = await saves.list(deviceId);
  if (!generations.length) return;

  const newest = generations[0];
  const bytes = await saves.read(deviceId, newest.file);
  // applyStatesBundle takes a File because the web path comes from a picker;
  // wrapping the bytes is cheaper than duplicating the decode.
  const result = await applyStatesBundle(
    new File([bytes as BlobPart], newest.file, { type: 'application/octet-stream' }));
  if (result.errors.length) {
    console.warn('[psion] generation imported with problems:', result.errors);
  } else {
    console.log(`[psion] restored ${deviceId} from ${newest.file} (disk)`);
  }
}

// Kept so the main-thread hook stays referenced and typechecked. The desktop
// never selects it, but letting it rot silently would mean discovering it is
// broken only when something forces a fallback.
export function DesktopAppMainThread() {
  const controls = useEmulator({});
  return <Shell controls={controls} />;
}

function Shell({ controls }: { controls: EmulatorControls }) {
  const host = requireHost();
  const {
    state, loadProgress, loadStatus, error, currentDeviceId, deviceInfo,
    profiles, loadDevice, savedDevices, resetDevice, revertToSaved,
  } = controls;

  // Off, as in the embed route: the simulated EL panel is an experimental
  // cosmetic feature and the desktop chrome has no toggle to opt into it.
  const backlight = useBacklight(controls, false);

  const [switcherOpen, setSwitcherOpen] = useState(false);
  const [keysVisible, setKeysVisible] = useState(false);
  const [skinned, setSkinned] = useState(false);
  const [busy, setBusy] = useState<string | null>(null);
  const [keyStripHeight, setKeyStripHeight] = useState(0);
  const [generations, setGenerations] = useState<SaveGeneration[] | null>(null);
  const [drivesOpen, setDrivesOpen] = useState(false);
  const [driveStatus, setDriveStatus] = useState<ExternalDriveStatus | null>(null);
  const [syncStatus, setSyncStatus] = useState<InternalMountStatus | null>(null);

  // The composed aspect of the machine, and the chrome that must not scale
  // with it. Held in refs as well as state because the window has to be told
  // whenever EITHER changes, and doing that from one effect avoids two
  // resizes racing each other on a device switch.
  const aspectRef = useRef(0);
  const [aspect, setAspect] = useState(0);

  // ── Preferences ────────────────────────────────────────────────────────
  useEffect(() => {
    let alive = true;
    void (async () => {
      const [strip, skin] = await Promise.all([
        host.state.get<boolean>(PREF_KEY_STRIP),
        host.state.get<boolean>(PREF_SKINNED),
      ]);
      if (!alive) return;
      if (strip !== null) setKeysVisible(strip);
      if (skin !== null) setSkinned(skin);
    })();
    return () => { alive = false; };
  }, [host]);

  // The Organiser II is a two-line display over a 36-key pad — the pad IS the
  // machine, so screen-only is the wrong default there. Applied on arrival at
  // the device, and only when the user has expressed no preference of their
  // own, so an explicit choice is never overridden.
  const skinDecidedFor = useRef<string | null>(null);
  useEffect(() => {
    if (!currentDeviceId || !deviceInfo) return;
    if (skinDecidedFor.current === currentDeviceId) return;
    skinDecidedFor.current = currentDeviceId;
    void (async () => {
      const explicit = await host.state.get<boolean>(PREF_SKINNED);
      if (explicit !== null) return;
      setSkinned(prefersSkinnedView({
        deviceId: currentDeviceId,
        deviceName: deviceInfo.deviceName ?? null,
      }));
    })();
  }, [currentDeviceId, deviceInfo, host]);

  // ── Window shape ───────────────────────────────────────────────────────
  const onContentAspect = useCallback((ratio: number) => {
    aspectRef.current = ratio;
    setAspect(ratio);
  }, []);

  useEffect(() => {
    if (!aspect) return;
    void host.window.setAspectRatio(aspect, { width: 0, height: keyStripHeight });
  }, [aspect, keyStripHeight, host]);

  // Geometry is remembered per device, because every machine has its own
  // aspect and a size chosen for one looks wrong on another.
  const previousDeviceId = useRef<string | null>(null);
  useEffect(() => {
    const previous = previousDeviceId.current;
    if (previous && previous !== currentDeviceId) {
      void host.window.rememberGeometry(previous);
    }
    previousDeviceId.current = currentDeviceId;
    host.devices.setCurrent(currentDeviceId);
    if (currentDeviceId) {
      // After the new machine's aspect has been applied, so the restore is
      // not immediately re-solved by the lock.
      const t = window.setTimeout(() => {
        void host.window.restoreGeometry(currentDeviceId);
      }, 80);
      return () => clearTimeout(t);
    }
  }, [currentDeviceId, host]);

  // ── The native menu's device list ──────────────────────────────────────
  useEffect(() => {
    if (!profiles.length) return;
    host.devices.publish(profiles.map((p) => ({
      id: p.id,
      name: p.displayName,
      supported: p.status === 'supported',
      hidden: p.hiddenFromPicker === true,
    })));
  }, [profiles, host]);

  // ── Earlier sessions ───────────────────────────────────────────────────
  // Refreshed each time the switcher opens rather than kept live: a save lands
  // about once a minute and nothing is watching this list in between.
  useEffect(() => {
    const saves = host.saves;
    if (!switcherOpen || !saves || !currentDeviceId) return;
    let alive = true;
    setGenerations(null);
    void saves.list(currentDeviceId)
      .then((list) => { if (alive) setGenerations(list); })
      .catch(() => { if (alive) setGenerations([]); });
    return () => { alive = false; };
  }, [switcherOpen, currentDeviceId, host]);

  const restoreGeneration = useCallback(async (file: string) => {
    const saves = host.saves;
    if (!saves || !currentDeviceId) return;
    setBusy('Restoring…');
    try {
      // Save what is on screen first, so choosing to go back does not throw
      // away the session being left — it becomes another generation.
      await controls.saveState();
      const bytes = await saves.read(currentDeviceId, file);
      const result = await applyStatesBundle(
        new File([bytes as BlobPart], file, { type: 'application/octet-stream' }));
      if (result.errors.length) {
        console.warn('[psion] generation imported with problems:', result.errors);
      }
      // The bundle has replaced this device's entry in IDB; revertToSaved
      // reloads the machine from it.
      await revertToSaved();
    } catch (err) {
      console.error('[psion] restoring an earlier session failed:', err);
    } finally {
      setBusy(null);
    }
  }, [host, currentDeviceId, controls, revertToSaved]);

  // ── Switching devices ──────────────────────────────────────────────────
  const selectDevice = useCallback((profile: DeviceProfile) => {
    setSwitcherOpen(false);
    if (profile.id === currentDeviceId) return;
    // The hash route is the existing switching path; going through it keeps
    // the desktop from growing a second one.
    window.location.hash = `#/${profile.id}`;
    void loadDevice(profile.id, `${import.meta.env.BASE_URL}roms/${profile.romFilename}`);
  }, [currentDeviceId, loadDevice]);

  const toggleKeys = useCallback(() => {
    setKeysVisible((v) => {
      const next = !v;
      void host.state.set(PREF_KEY_STRIP, next);
      return next;
    });
  }, [host]);

  // ── Commands from the menu, tray and accelerators ──────────────────────
  useEffect(() => host.devices.onCommand((cmd) => {
    switch (cmd.kind) {
      case 'open-switcher': setSwitcherOpen(true); break;
      case 'open-drives': setDrivesOpen((v) => !v); break;
      case 'toggle-keys': toggleKeys(); break;
      case 'reset-device': resetDevice(); break;
      case 'load-device': {
        const profile = profiles.find((p) => p.id === cmd.deviceId);
        if (profile) selectDevice(profile);
        break;
      }
      case 'revert-to-saved': void revertToSaved(); break;
      case 'sync-now': setDrivesOpen(true); break;
      default: break;
    }
  }), [host, profiles, selectDevice, toggleKeys, resetDevice, revertToSaved]);

  // Ctrl/Cmd+K in the renderer as well as on the menu: on Windows the menu
  // bar is hidden, and a user who has clicked into the device should still be
  // able to reach the switcher without hunting for it.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && !e.altKey && e.key.toLowerCase() === 'k') {
        e.preventDefault();
        e.stopPropagation();
        setSwitcherOpen((v) => !v);
      }
      if ((e.ctrlKey || e.metaKey) && !e.altKey && e.key.toLowerCase() === 'd') {
        e.preventDefault();
        e.stopPropagation();
        setDrivesOpen((v) => !v);
      }
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, []);

  // ── The quit handshake ─────────────────────────────────────────────────
  // AutoSaveController registers the save itself; all this does is put an
  // overlay up so a deliberately-waiting quit does not look like a hang.
  // Several subscribers can register — the preload aggregates them and only
  // reports ready once every one has settled.
  useEffect(() => host.lifecycle.onPrepareQuit(async () => {
    setBusy('Saving…');
    host.lifecycle.setBusy('Saving…');
  }), [host]);

  const currentProfile = profiles.find((p) => p.id === currentDeviceId) ?? null;
  const cardFamily = familyFor(currentProfile);

  const deviceName = currentProfile?.displayName ?? deviceInfo?.deviceName ?? null;

  if (error && state === 'error') {
    return (
      <div style={{
        position: 'fixed', inset: 0, display: 'flex', alignItems: 'center',
        justifyContent: 'center', padding: 24, textAlign: 'center',
        font: '13px ui-monospace, monospace', color: '#8a2a2a', background: '#c9c9bd',
      }}>
        {error}
      </div>
    );
  }

  return (
    <>
      {state === 'running' ? (
        <EmulatorView
          controls={controls}
          sizingMode="fill"
          deviceScale={2}
          fullscreenRequest={0}
          showDebugging={false}
          experimentalFeatures={false}
          backlight={backlight}
          chromeless
          deviceMode={skinned}
          onContentAspect={onContentAspect}
        />
      ) : (
        <div style={{
          position: 'fixed', inset: 0, display: 'flex', flexDirection: 'column',
          alignItems: 'center', justifyContent: 'center', gap: 10,
          font: '12px ui-monospace, monospace', color: '#3a3a36', background: '#c9c9bd',
        }}>
          <span>{loadStatus ?? 'Starting…'}</span>
          {loadProgress !== null && (
            <div style={{ width: 180, height: 3, background: 'rgba(0,0,0,0.15)' }}>
              <div style={{
                width: `${Math.round(loadProgress * 100)}%`, height: '100%',
                background: '#3a3a36', transition: 'width 120ms linear',
              }} />
            </div>
          )}
        </div>
      )}

      <AutoSaveController
        controls={controls}
        syncInFlight={(driveStatus?.busy ?? false) || syncStatus?.phase === 'syncing'}
      />

      {host.mounts && (
        <>
          <ExternalDriveController
            controls={controls}
            profile={currentProfile}
            onStatus={setDriveStatus}
          />
          <InternalMountController
            controls={controls}
            profile={currentProfile}
            onStatus={setSyncStatus}
          />
        </>
      )}

      <DesktopChrome
        deviceName={deviceName}
        onOpenSwitcher={() => setSwitcherOpen(true)}
        onOpenDrives={host.mounts ? () => setDrivesOpen(true) : undefined}
        driveSummary={driveStatus && driveStatus.skipped.length > 0
          ? `${driveStatus.skipped.length} didn't fit`
          : driveStatus?.busy ? 'card…' : null}
        syncSummary={syncStatus && syncStatus.phase !== 'off' ? syncStatus.summary : null}
        syncActionable={syncStatus?.actionable ?? false}
        onToggleKeys={toggleKeys}
        keysVisible={keysVisible}
        busy={busy}
      />

      <DeviceKeyStrip
        controls={controls}
        visible={keysVisible && state === 'running'}
        onHeightChange={setKeyStripHeight}
      />

      <DesktopDeviceSwitcher
        open={switcherOpen}
        profiles={profiles}
        currentDeviceId={currentDeviceId}
        savedDevices={savedDevices}
        onSelect={selectDevice}
        onClose={() => setSwitcherOpen(false)}
        generations={generations}
        onRestore={host.saves ? restoreGeneration : undefined}
      />

      <DrivesPanel
        open={drivesOpen}
        onClose={() => setDrivesOpen(false)}
        family={cardFamily}
        status={driveStatus}
        syncStatus={syncStatus}
        canLink={(currentProfile?.remoteLinkUart ?? -1) >= 0
                 && (currentProfile?.linkProtocol ?? 0) > 0}
        deviceName={deviceName}
      />
    </>
  );
}
