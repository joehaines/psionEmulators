// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useEmulator, type EmulatorControls } from './hooks/useEmulator';
import { useEmulatorWorker } from './hooks/useEmulatorWorker';
import { loadPsionModule } from './lib/wasmBridge';
import { useBacklight } from './hooks/useBacklight';
import LoadingOverlay from './components/LoadingOverlay';
import EmulatorView, { getSkinFilename, getDeviceSkinPhotoFilename, type SizingMode, type DeviceScale } from './components/EmulatorView';
import MameFrame from './components/MameFrame';
import DevicePanel from './components/DevicePanel';
import Leaderboard from './components/Leaderboard';
import AppLibrary from './components/AppLibrary';
import Home from './components/Home';
import SettingsView, { type SortMode } from './components/SettingsView';
import {
  takePendingTry, deliverApp, fetchAppZip, isAppsRoute, parseAppsRoute,
  type AppEntry, type DeliveryResult,
} from './lib/appLibrary';
import type { DeviceProfile } from './types/emulator';
import psionLogoUrl from './assets/psion-logo.svg';
import { trackDeviceLoad, startSession, endSession, fetchLeaderboard, type LeaderboardRow } from './lib/analytics';
import { DEVICE_RELEASE_YEARS } from './lib/deviceMeta';

// Persisted device-mode preference. When on, photo-realistic device skins
// from the `device-skins/` public folder are shown instead of the minimal
// SVG/plain-LCD rendering. Default on — only an explicit '0' (user turned it
// off) disables it.
const DEVICE_MODE_KEY = 'psion-device-mode';
function loadDeviceMode(): boolean {
  if (typeof window === 'undefined') return true;
  return window.localStorage.getItem(DEVICE_MODE_KEY) !== '0';
}

// Persisted sizing-mode preference. Only 'device' / 'fill' round-trip
// through localStorage; 'fullscreen' is a one-shot action triggered by
// clicking the third icon (the browser won't enter fullscreen on reload
// without a fresh user gesture anyway).
const SIZING_MODE_KEY = 'psion-sizing-mode';
function loadSizingMode(): SizingMode {
  if (typeof window === 'undefined') return 'device';
  const v = window.localStorage.getItem(SIZING_MODE_KEY);
  if (v === 'fill') return 'fill';
  // 'device4x' migration handled in loadDeviceScale (caller writes back
  // a clean 'device' on next persist).
  return 'device';
}

// Persisted pixel-perfect zoom factor for 'device' mode. 2× is the
// default on first load — small enough to fit any portable device on
// most laptop / tablet viewports while still being clearly readable.
// Range 1..4 enforced by the type.
const DEVICE_SCALE_KEY = 'psion-device-scale';
function loadDeviceScale(): DeviceScale {
  if (typeof window === 'undefined') return 2;
  const v = window.localStorage.getItem(DEVICE_SCALE_KEY);
  if (v === '1' || v === '2' || v === '3' || v === '4') {
    return parseInt(v, 10) as DeviceScale;
  }
  // Migrate users from the previous build, which expressed the 4×
  // option as a separate SizingMode rather than a scale. Map it onto
  // the new scale state; the obsolete mode value gets overwritten by
  // the next loadSizingMode() / persist cycle.
  if (window.localStorage.getItem(SIZING_MODE_KEY) === 'device4x') return 4;
  return 2;
}

// Per-user "developer-y" toggles in the side panel's Settings section.
// Both default off — Show debugging gates the Logs / Download RAM
// affordances, and Experimental features currently gates the simulated
// Modem panel (the most rough-edged Internet path on the device).
const SHOW_DEBUGGING_KEY  = 'psion-show-debugging';
const EXPERIMENTAL_KEY    = 'psion-experimental-features';
const LCD_ACCURACY_KEY    = 'psion-lcd-accuracy-mode';
function loadBoolFlag(key: string): boolean {
  if (typeof window === 'undefined') return false;
  return window.localStorage.getItem(key) === '1';
}

// Stored as the set of device IDs the user has EXPLICITLY unchecked. New
// devices added in a future build default to visible (favourite) — they
// only get demoted to "Other devices" if the user opts them out.
const NON_FAVOURITE_DEVICES_KEY = 'psion-non-favourite-devices';
function loadNonFavourites(): Set<string> {
  if (typeof window === 'undefined') return new Set();
  try {
    const raw = window.localStorage.getItem(NON_FAVOURITE_DEVICES_KEY);
    if (!raw) return new Set();
    const arr = JSON.parse(raw);
    if (!Array.isArray(arr)) return new Set();
    return new Set(arr.filter((x): x is string => typeof x === 'string'));
  } catch { return new Set(); }
}

// Default = age-desc (newest first). Older sessions without the key fall
// back to this default; an unknown stored value also falls back.
const SORT_MODE_KEY = 'psion-device-sort-mode';
const VALID_SORT_MODES: SortMode[] = ['age-desc', 'age-asc', 'popularity-desc', 'popularity-asc'];
function loadSortMode(): SortMode {
  if (typeof window === 'undefined') return 'age-desc';
  const v = window.localStorage.getItem(SORT_MODE_KEY);
  return (VALID_SORT_MODES as string[]).includes(v ?? '') ? (v as SortMode) : 'age-desc';
}

// Sort device profiles for the side menu based on the user's preference.
// Stable: equal sort keys preserve the registry order so the list doesn't
// reshuffle between renders. When the leaderboard hasn't loaded yet (or
// failed), popularity sorts treat every device as having zero loads and
// the registry order shows through unchanged.
function sortProfiles(
  profiles: DeviceProfile[],
  mode: SortMode,
  leaderboard: LeaderboardRow[] | null,
): DeviceProfile[] {
  const indexed = profiles.map((profile, registryIndex) => ({ profile, registryIndex }));
  const loadsById = new Map<string, number>();
  for (const row of leaderboard ?? []) loadsById.set(row.deviceId, row.loadCount);

  const compare = (a: { profile: DeviceProfile; registryIndex: number },
                   b: { profile: DeviceProfile; registryIndex: number }): number => {
    if (mode === 'age-desc' || mode === 'age-asc') {
      // Devices with no known year sort to the end of either direction —
      // they're metadata gaps, not "year 0" entries.
      const ya = DEVICE_RELEASE_YEARS[a.profile.id];
      const yb = DEVICE_RELEASE_YEARS[b.profile.id];
      const ka = ya ?? (mode === 'age-desc' ? -Infinity : Infinity);
      const kb = yb ?? (mode === 'age-desc' ? -Infinity : Infinity);
      if (ka !== kb) return mode === 'age-desc' ? kb - ka : ka - kb;
    } else {
      const la = loadsById.get(a.profile.id) ?? 0;
      const lb = loadsById.get(b.profile.id) ?? 0;
      if (la !== lb) return mode === 'popularity-desc' ? lb - la : la - lb;
    }
    return a.registryIndex - b.registryIndex;
  };

  return indexed.sort(compare).map(({ profile }) => profile);
}

// Devices for which an embedded MAME wasm bundle is also available as a
// fallback. The native V30+ASIC9 port is now the default for these (the
// May 2026 mirror-write fix put the v3.40f / v1.30f M:-drive bring-up on
// the same code path MAME uses, so the "Media is corrupt" dialog no longer
// appears on cold boot). The MAME bundle stays compiled in and reachable
// from a low-prominence button in the header for side-by-side comparison.
const MAME_CAPABLE_DEVICE_IDS = new Set(['series3a', 'pocketbk2']);

// The MC400 ships in two ROM revisions. The picker lists only the default
// (v2.60F, id 'mc400'); a discreet header link — mirroring the MAME escape
// hatch above — swaps to the older v1.26F ('mc400v126', hidden from the
// picker) and back. Each entry names the device to load and the label/title
// for the link shown while the *other* ROM is running.
const MC400_ROM_SWITCH: Record<string, { to: string; label: string; title: string }> = {
  mc400:     { to: 'mc400v126', label: 'v1.26F', title: 'Load the older MC400 v1.26F boot ROM' },
  mc400v126: { to: 'mc400',     label: 'v2.60F', title: 'Load the default MC400 v2.60F boot ROM' },
};

const USAGE_HASH = '#/usage';
// Standalone embed route: `#/embed/<deviceId>` renders the named device
// in a chromeless full-viewport layout, suitable for iframing into a
// third-party site. Currently used to host the MC218 on another site;
// adding more devices is just a matter of pointing the iframe at a
// different ID (the hash is parsed and looked up against the WASM
// device registry like any other ID).
const EMBED_HASH_PREFIX = '#/embed/';
// Standalone app-library route — like the leaderboard, it renders
// without the WASM bootstrap; "Try it" hands back into the emulator
// via a sessionStorage note + `#/<deviceId>` navigation. Its two
// optional parameters (`?device=`, `?app=`) are parsed by
// lib/appLibrary.ts, which the library itself writes back through —
// see APPS_ROUTE there.

export default function App() {
  const [route, setRoute] = useState(() =>
    typeof window !== 'undefined' ? window.location.hash : '');

  useEffect(() => {
    const onChange = () => setRoute(window.location.hash);
    window.addEventListener('hashchange', onChange);
    return () => window.removeEventListener('hashchange', onChange);
  }, []);

  // Render Leaderboard standalone (no WASM bootstrap) when the user lands
  // on or navigates to the usage route. Routing through a separate
  // component keeps the rules-of-hooks invariant clean — useEmulator only
  // ever runs in the emulator subtree.
  if (route === USAGE_HASH) {
    return <Leaderboard onClose={() => { window.location.hash = ''; }} />;
  }
  if (isAppsRoute(route)) {
    const { device, app } = parseAppsRoute(route);
    return (
      <AppLibrary
        initialDeviceFilter={device}
        appId={app}
        onClose={() => { window.location.hash = ''; }}
      />
    );
  }
  if (route.startsWith(EMBED_HASH_PREFIX) || route === '#/embed') {
    const deviceId = route.startsWith(EMBED_HASH_PREFIX)
      ? route.slice(EMBED_HASH_PREFIX.length)
      : '';
    // `#/embed` and `#/embed/` (no device ID) land on the embed index —
    // a reference page listing the supported device IDs and the iframe
    // URL pattern. The device routes look up the ID against the same
    // registry; an unknown ID still shows an inline error.
    if (!deviceId) return <EmbedIndex />;
    return <EmbedApp deviceId={deviceId} />;
  }
  const hashDeviceId = route.startsWith('#/') ? route.slice(2) : '';
  return WORKER_MODE
    ? <EmulatorAppWorker initialDeviceId={hashDeviceId || undefined} />
    : <EmulatorApp initialDeviceId={hashDeviceId || undefined} />;
}

// Off-main-thread emulation path (see docs/web-worker-emulation-scope.md). Now
// DEFAULT-ON wherever the browser supports it (Worker + OffscreenCanvas
// transfer); the emulation runs unbudgeted in the worker so the core advances at
// full speed while the UI thread stays free. Escape hatch: ?worker=0 forces the
// old main-thread path; unsupported browsers fall back to it automatically.
const WORKER_MODE = (() => {
  try {
    if (typeof window === 'undefined') return false;
    const flag = new URLSearchParams(window.location.search).get('worker');
    if (flag === '0') return false;                    // explicit opt-out
    const supported = typeof Worker !== 'undefined'
      && typeof OffscreenCanvas !== 'undefined'
      && typeof HTMLCanvasElement !== 'undefined'
      && typeof HTMLCanvasElement.prototype.transferControlToOffscreen === 'function';
    return supported;
  } catch { return false; }
})();

function EmulatorApp({ initialDeviceId }: { initialDeviceId?: string }) {
  const controls = useEmulator({ initialDeviceId });
  return <EmulatorAppBody controls={controls} />;
}

// Same UI, but the emulator runs in a Web Worker (Phase 1 — core experience).
function EmulatorAppWorker({ initialDeviceId }: { initialDeviceId?: string }) {
  const controls = useEmulatorWorker({ initialDeviceId });
  return <EmulatorAppBody controls={controls} />;
}

function EmulatorAppBody({ controls }: { controls: EmulatorControls }) {
  const { state, loadProgress, loadStatus, error, currentDeviceId, profiles, loadDevice, clearSession, savedDevices, exportAllStates, importStates } = controls;

  const [panelOpen, setPanelOpen] = useState(false);
  const [loadingDeviceId, setLoadingDeviceId] = useState<string | null>(null);
  const lastAttemptRef = useRef<{ deviceId: string; romUrl: string } | null>(null);
  const [mameDevice, setMameDevice] = useState<{ id: string; romUrl: string } | null>(null);
  const mameDeviceId = mameDevice?.id ?? null;

  const [deviceMode, setDeviceMode] = useState<boolean>(loadDeviceMode);
  const [sizingMode, setSizingMode] = useState<SizingMode>(loadSizingMode);
  const [deviceScale, setDeviceScale] = useState<DeviceScale>(loadDeviceScale);
  // Largest actual-size scale the window can show for the device currently
  // on screen — reported by EmulatorView, which owns the skin layout and
  // panel size it depends on, and recomputed on resize / rotate / device
  // change. The zoom flyout greys out anything above it: a 640-wide panel
  // at 4× is 2560 px of device, and rather than quietly drawing something
  // else the control now says it can't. 4 until a device reports.
  const [maxDeviceScale, setMaxDeviceScale] = useState(4);
  // Bump count — EmulatorView watches this and re-enters fullscreen each
  // time it changes, even if the user has already exited via Escape (the
  // value would otherwise stay the same and the effect wouldn't re-fire).
  const [fullscreenRequest, setFullscreenRequest] = useState(0);
  const [showDebugging, setShowDebugging] = useState<boolean>(() => loadBoolFlag(SHOW_DEBUGGING_KEY));
  const [experimentalFeatures, setExperimentalFeatures] = useState<boolean>(() => loadBoolFlag(EXPERIMENTAL_KEY));
  const [lcdAccuracyMode, setLcdAccuracyMode] = useState<boolean>(() => {
    if (typeof window === 'undefined') return true;
    const v = window.localStorage.getItem(LCD_ACCURACY_KEY);
    return v === null || v === '1';
  });
  const [nonFavouriteDevices, setNonFavouriteDevices] = useState<Set<string>>(() => loadNonFavourites());
  const [sortMode, setSortMode] = useState<SortMode>(() => loadSortMode());
  const [settingsViewOpen, setSettingsViewOpen] = useState(false);
  // Home/landing view. Shown by default when no device is loaded (the
  // "blank screen behind the menu on load"), and re-asserted whenever the
  // user clicks the Psion logo in the header. It's a pure UI overlay — a
  // device that's already running keeps running in the background and
  // re-appears when the user picks it again, so nothing is lost.
  const [homeViewOpen, setHomeViewOpen] = useState(false);

  // "Try it" handoff from the app library (#/apps): once the requested
  // device reaches 'running', fetch the app bundle and deliver it onto
  // the device (CF card / SSD pack / Remote Link — lib/appLibrary.ts),
  // narrating progress and then the install steps in a corner overlay.
  const [tryNotice, setTryNotice] = useState<
    { app: AppEntry; phase: string; progress?: { bytes: number; total: number };
      result?: DeliveryResult; error?: string } | null>(null);
  const tryBusyRef = useRef(false);
  // Aborts the in-flight delivery (Close button / device switch) so the
  // Remote Link tears down with a proper Disc_Pdu — abandoning it mid
  // transfer leaves the device holding a dead session that blocks every
  // reconnect until its Remote Link is toggled off and on.
  const tryAbortRef = useRef<AbortController | null>(null);
  // The notice belongs to the device it was delivered to — dismiss it
  // the moment a different device starts loading.
  useEffect(() => {
    if (state === 'loading-rom') {
      tryAbortRef.current?.abort();
      setTryNotice(null);
    }
  }, [state]);
  // Abort on unmount too (navigating back to the library mid-delivery).
  useEffect(() => () => tryAbortRef.current?.abort(), []);
  useEffect(() => {
    if (state !== 'running' || !currentDeviceId || tryBusyRef.current) return;
    const entry = takePendingTry(currentDeviceId);
    if (!entry) return;
    tryBusyRef.current = true;
    const abort = new AbortController();
    tryAbortRef.current = abort;
    setTryNotice({ app: entry, phase: 'Downloading the app…' });
    void (async () => {
      try {
        const zip = await fetchAppZip(entry);
        const profile = profiles.find(p => p.id === currentDeviceId);
        if (!profile) throw new Error('device profile unavailable');
        const result = await deliverApp(entry, zip, controls, profile,
          phase => setTryNotice(n => (n ? { ...n, phase } : n)),
          // The Remote Link upload reports every chunk — only re-render
          // when the displayed whole percentage would move.
          (bytes, total) => setTryNotice(n => {
            if (!n) return n;
            const p = n.progress;
            if (p && p.total === total && bytes < total
                && Math.floor((bytes * 100) / total) === Math.floor((p.bytes * 100) / p.total)) {
              return n;
            }
            return { ...n, progress: { bytes, total } };
          }),
          abort.signal);
        if (!abort.signal.aborted) setTryNotice({ app: entry, phase: 'done', result });
      } catch (e) {
        if (!abort.signal.aborted) {
          setTryNotice({
            app: entry, phase: 'error',
            error: e instanceof Error ? e.message : String(e),
          });
        }
      } finally {
        tryBusyRef.current = false;
        if (tryAbortRef.current === abort) tryAbortRef.current = null;
      }
    })();
  }, [state, currentDeviceId, profiles, controls]);
  // Cached leaderboard rows, fetched once when the app mounts. Both the
  // popularity-sort options and the Settings view's "loads" stats use this
  // map; the leaderboard route still does its own fetch so it never sees
  // a stale snapshot. `null` while in flight; `[]` if the request failed
  // or returned empty (popularity sort then falls back to age order).
  const [leaderboard, setLeaderboard] = useState<LeaderboardRow[] | null>(null);
  useEffect(() => {
    let cancelled = false;
    fetchLeaderboard()
      .then(rows => { if (!cancelled) setLeaderboard(rows); })
      .catch(() => { if (!cancelled) setLeaderboard([]); });
    return () => { cancelled = true; };
  }, []);
  // deviceId -> total loads, derived from the cached leaderboard. Drives
  // the popularity sort and, via the Home view, the order of the device
  // image bar. Empty until the leaderboard lands (or if it failed).
  const deviceLoads = useMemo(() => {
    const m = new Map<string, number>();
    for (const row of leaderboard ?? []) m.set(row.deviceId, row.loadCount);
    return m;
  }, [leaderboard]);

  // Backlight simulation lives behind the Experimental features flag —
  // the 3mx path is a known partial (no auto-off because the port pin
  // isn't modelled) and the feature is purely cosmetic, so it ships in
  // the same opt-in bucket as the simulated Modem.
  const backlight = useBacklight(controls, experimentalFeatures);

  useEffect(() => {
    try { window.localStorage.setItem(DEVICE_MODE_KEY, deviceMode ? '1' : '0'); } catch {}
  }, [deviceMode]);
  useEffect(() => {
    try { window.localStorage.setItem(SIZING_MODE_KEY, sizingMode); } catch {}
  }, [sizingMode]);
  useEffect(() => {
    try { window.localStorage.setItem(DEVICE_SCALE_KEY, String(deviceScale)); } catch {}
  }, [deviceScale]);
  useEffect(() => {
    try { window.localStorage.setItem(SHOW_DEBUGGING_KEY, showDebugging ? '1' : '0'); } catch {}
  }, [showDebugging]);
  useEffect(() => {
    try { window.localStorage.setItem(EXPERIMENTAL_KEY, experimentalFeatures ? '1' : '0'); } catch {}
  }, [experimentalFeatures]);
  useEffect(() => {
    try { window.localStorage.setItem(LCD_ACCURACY_KEY, lcdAccuracyMode ? '1' : '0'); } catch {}
  }, [lcdAccuracyMode]);
  useEffect(() => {
    try {
      window.localStorage.setItem(
        NON_FAVOURITE_DEVICES_KEY,
        JSON.stringify(Array.from(nonFavouriteDevices)),
      );
    } catch {}
  }, [nonFavouriteDevices]);
  useEffect(() => {
    try { window.localStorage.setItem(SORT_MODE_KEY, sortMode); } catch {}
  }, [sortMode]);

  useEffect(() => {
    if (currentDeviceId && window.location.hash !== `#/${currentDeviceId}`) {
      history.replaceState(null, '', `#/${currentDeviceId}`);
    }
  }, [currentDeviceId]);

  const handlePickSizingMode = (mode: SizingMode) => setSizingMode(mode);
  // Picking a scale always implies device (pixel-perfect) mode — the chip
  // appears inside the actual-size button's flyout, so a click there is a
  // direct expression of "I want pixel-perfect at this zoom".
  const handlePickDeviceScale = (scale: DeviceScale) => {
    setDeviceScale(scale);
    setSizingMode('device');
  };
  const handleEnterFullscreen = () => setFullscreenRequest(n => n + 1);

  // Use profiles exactly as the WASM registry reports them. All SIBO
  // devices (Series 3 / 3a / 3c / 3mx / PB2 / Siena) are backed by the
  // native V30 + ASIC1/2/9 core; 3a and PB2 additionally have an opt-in
  // MAME bundle reachable from the header (see MAME_CAPABLE_DEVICE_IDS).
  const mergedProfiles: DeviceProfile[] = profiles;
  const effectiveDeviceId = mameDeviceId ?? currentDeviceId;

  // Sort the side-menu device list according to the user's chosen mode.
  // The original registry order is preserved as the stable tiebreaker so
  // devices with the same year (or the same zero load count when the
  // leaderboard hasn't loaded yet) don't shuffle between renders.
  // Hidden profiles (alternate-ROM variants like the older MC400 v1.26F)
  // are dropped from the picker — they're still in mergedProfiles for
  // label/ROM lookups and reachable via the header ROM-switch link.
  const sortedProfiles = useMemo(
    () => sortProfiles(mergedProfiles.filter(p => !p.hiddenFromPicker), sortMode, leaderboard),
    [mergedProfiles, sortMode, leaderboard],
  );

  // Ids the Home view's per-device Launch buttons can boot. Empty until
  // the WASM registry lands, which is what keeps those buttons disabled
  // while there's no ROM filename to build a URL from.
  const launchableIds = useMemo(
    () => new Set(mergedProfiles.map(p => p.id)),
    [mergedProfiles],
  );

  // Once the user closes the auto-opened picker we must not pop it back
  // open at them — without this latch any later re-run of the effect
  // below (a "Try it" load that drops back to 'ready', an ended session)
  // re-asserts panelOpen, so closing it feels like the menu "keeps coming
  // up". The latch is cleared whenever a device starts loading/running,
  // i.e. a genuinely new context that may warrant the picker again.
  const pickerDismissedRef = useRef(false);

  // Open panel automatically when WASM is ready but no device is loading/loaded.
  // Mirrors the auto-load rule in useEmulator(-Worker): when the route hash or
  // the remembered last device names a supported machine, a load is about to
  // start (e.g. the app library's "Try it" handoff) — keep the panel shut so
  // it doesn't sit over the loading screen and then vanish.
  useEffect(() => {
    if (state !== 'ready' || mameDevice || pickerDismissedRef.current) return;
    let pendingId: string | null = null;
    try {
      const hashId = window.location.hash.startsWith('#/') ? window.location.hash.slice(2) : '';
      pendingId = hashId || localStorage.getItem('psion-last-device');
    } catch { /* storage blocked — fall through to opening the panel */ }
    if (pendingId && profiles.some(p => p.id === pendingId && p.status === 'supported')) return;
    setPanelOpen(true);
  }, [state, mameDevice, profiles]);

  // Close panel once a device starts running, and forget any prior
  // dismissal so the picker can auto-open again in a later idle state.
  useEffect(() => {
    if (state === 'loading-rom' || state === 'running') pickerDismissedRef.current = false;
    if (state === 'running') setPanelOpen(false);
  }, [state]);

  // Closing the panel records the user's intent (see pickerDismissedRef);
  // opening it clears the latch. Used by both the header toggle and the
  // panel's own close affordances.
  const setPanelOpenIntent = useCallback((open: boolean) => {
    pickerDismissedRef.current = !open;
    setPanelOpen(open);
  }, []);

  const handleSelectDevice = async (deviceId: string, romUrl: string) => {
    setPanelOpen(false);
    setHomeViewOpen(false);
    setMameDevice(null);
    setLoadingDeviceId(deviceId);
    // Remembered so the error screen can offer "Try again" for the device
    // that actually failed (loadingDeviceId is cleared once loadDevice
    // settles, even on error).
    lastAttemptRef.current = { deviceId, romUrl };
    // NB: the URL is deliberately NOT changed here. It's updated by the
    // currentDeviceId effect below once the device has actually loaded, so
    // the address bar can never get ahead of reality — previously the hash
    // flipped to the new device immediately and, if the switch then stalled
    // (e.g. picking a device while the current one was busy), the URL said
    // one thing while the old device stayed on screen.
    await loadDevice(deviceId, romUrl);
    // Only clear the loading overlay if this load is still the active one;
    // a newer selection that superseded us owns the overlay now.
    setLoadingDeviceId(prev => (prev === deviceId ? null : prev));
  };

  // Boot a device straight from its card on the Home page. Same path as
  // picking it in the side menu — resolve the profile for its ROM
  // filename and hand off to handleSelectDevice, which closes Home.
  const handleLaunchFromHome = (deviceId: string) => {
    const profile = mergedProfiles.find(p => p.id === deviceId);
    if (!profile) return;
    void handleSelectDevice(deviceId, `${import.meta.env.BASE_URL}roms/${profile.romFilename}`);
  };

  // Swap the running MC400 between its two boot ROMs (v2.60F default ↔
  // older v1.26F). Each ROM is a distinct device id, so this is just a
  // normal device load — save states stay keyed per ROM revision.
  const handleSwitchMc400Rom = () => {
    if (!currentDeviceId) return;
    const target = MC400_ROM_SWITCH[currentDeviceId];
    if (!target) return;
    const profile = mergedProfiles.find(p => p.id === target.to);
    if (!profile) return;
    void handleSelectDevice(target.to, `${import.meta.env.BASE_URL}roms/${profile.romFilename}`);
  };

  const handleSwitchToMame = () => {
    if (!currentDeviceId || !MAME_CAPABLE_DEVICE_IDS.has(currentDeviceId)) return;
    const profile = mergedProfiles.find(p => p.id === currentDeviceId);
    if (!profile) return;
    const romUrl = `${import.meta.env.BASE_URL}roms/${profile.romFilename}`;
    endSession('switch');
    setMameDevice({ id: currentDeviceId, romUrl });
    trackDeviceLoad(currentDeviceId);
    startSession(currentDeviceId);
  };

  const closeMame = () => {
    endSession('switch');
    setMameDevice(null);
    setPanelOpenIntent(true);
  };

  const handleSwitchToNative = async () => {
    if (!mameDevice) return;
    const { id, romUrl } = mameDevice;
    endSession('switch');
    setMameDevice(null);
    await loadDevice(id, romUrl);
  };

  // Return to the home/landing page. Clicking the Psion logo lands here
  // from anywhere — close Settings, drop out of the MAME iframe, and show
  // the Home view over whatever device is (still) running in the
  // background.
  const goHome = () => {
    setSettingsViewOpen(false);
    setMameDevice(null);
    setHomeViewOpen(true);
    // Reset the URL back to the base path, dropping any `#/<deviceId>`
    // route so the home page has a clean, shareable URL. Clearing the hash
    // fires hashchange (keeping App's route state in sync); replaceState
    // then strips the leftover bare '#'. The running device is unaffected
    // — initialDeviceId is only read once at mount.
    if (window.location.hash) {
      window.location.hash = '';
      window.history.replaceState(null, '', window.location.pathname + window.location.search);
    }
  };

  // A device the app is about to auto-load (named in the URL hash, or the
  // remembered last device) — mirrors the auto-load rule in
  // useEmulator(-Worker). When one is pending we keep the landing Home
  // suppressed so a returning visitor goes straight to their device
  // instead of flashing the home page first.
  const pendingAutoloadId = (() => {
    try {
      const hashId = window.location.hash.startsWith('#/') ? window.location.hash.slice(2) : '';
      return hashId || localStorage.getItem('psion-last-device');
    } catch { return null; }
  })();

  // Show the Home view when explicitly requested (logo click) or as the
  // default landing state — WASM ready with no device picked and none
  // pending an auto-load. While a device is loading, running, erroring, or
  // showing in MAME we defer to those views unless the user has explicitly
  // gone home.
  const showHome = !settingsViewOpen && !mameDeviceId &&
    (homeViewOpen || (!currentDeviceId && !pendingAutoloadId && state === 'ready'));

  return (
    <div className="min-h-screen bg-psion-dark font-mono flex flex-col">
      {/* Header */}
      <header className="border-b border-psion-accent/40 px-4 py-2 flex items-center gap-3 flex-shrink-0 bg-psion-dark shadow-sm">
        {/* Panel toggle */}
        <button
          onClick={() => setPanelOpenIntent(!panelOpen)}
          className="flex flex-col gap-1 p-1.5 rounded hover:bg-psion-mid transition-colors group"
          aria-label="Toggle device panel"
        >
          <span className="block w-5 h-0.5 bg-psion-charcoal/50 group-hover:bg-psion-charcoal transition-colors" />
          <span className="block w-5 h-0.5 bg-psion-charcoal/50 group-hover:bg-psion-charcoal transition-colors" />
          <span className="block w-5 h-0.5 bg-psion-charcoal/50 group-hover:bg-psion-charcoal transition-colors" />
        </button>

        <button
          type="button"
          onClick={goHome}
          className="flex items-center rounded p-0.5 hover:bg-psion-mid transition-colors"
          aria-label="Psion — back to home page"
          title="Back to home page"
        >
          <img src={psionLogoUrl} alt="Psion" className="h-8 w-auto select-none" draggable={false} />
        </button>

        {!showHome && effectiveDeviceId && (
          // Shortened device name (strips redundant "Psion ", "Series ",
          // "Ericsson ", "Oregon Scientific " prefixes — the audio
          // buttons below need to fit on mobile screens). Hidden
          // entirely below the sm: breakpoint so the buttons get all
          // the horizontal space they need on phones.
          <span className="hidden sm:inline-block text-xs text-gray-400 ml-1 border-l border-psion-accent/30 pl-3 whitespace-nowrap">
            {shortenDeviceName(mergedProfiles.find(p => p.id === effectiveDeviceId)?.displayName ?? effectiveDeviceId)}
          </span>
        )}

        {/* Global Speaker / Mic toggles. Moved up here from the
            in-view control bar so the audio path is reachable from a
            consistent location regardless of whether the user is
            inside fullscreen / fill mode / debugging panels. Only
            rendered when the running device actually has audio
            (deviceInfo.hasAudio — false on SIBO devices that pre-
            date the SIBO speaker support). The vertical divider goes
            BEFORE the buttons so the line sits between the device
            title and the controls. */}
        {!showHome && !mameDeviceId && state === 'running' && controls.deviceInfo?.hasAudio && (
          <AudioControls
            speakerEnabled={controls.speakerEnabled}
            micEnabled={controls.micEnabled}
            hasMic={controls.deviceInfo?.hasMic ?? true}
            onToggleSpeaker={() => void controls.toggleSpeaker()}
            onToggleMic={() => void controls.toggleMic()}
          />
        )}

        {/* Backlight toggle — only shown on the four devices that
            actually had an EL panel (5 / 5mx / 5mx Pro / 3mx). Lives in
            the header so it's reachable from any sizing mode, including
            on mobile when the soft keyboard would otherwise hide the
            in-view control bar. */}
        {!showHome && !mameDeviceId && state === 'running' && backlight.color && (
          <BacklightControl
            on={backlight.on}
            color={backlight.color}
            onToggle={backlight.toggle}
          />
        )}

        {/* Discreet escape-hatch to the MAME bundle for SIBO 3a/PB2.
            Native is the default; this lets a curious user side-by-side
            compare. Deliberately tiny + low-opacity so it doesn't compete
            with the device label. */}
        {!showHome && currentDeviceId && !mameDeviceId && MAME_CAPABLE_DEVICE_IDS.has(currentDeviceId) && (
          <button
            type="button"
            onClick={handleSwitchToMame}
            className="text-[10px] text-gray-500/40 hover:text-gray-400 transition-colors px-1"
            title="Switch this device to the MAME 0.253 reference build"
          >
            mame
          </button>
        )}

        {/* MC400 ROM-revision switch. The default device boots v2.60F;
            this discreet link loads the older v1.26F boot ROM (and back),
            without adding a second near-duplicate entry to the picker. */}
        {!showHome && currentDeviceId && !mameDeviceId && MC400_ROM_SWITCH[currentDeviceId] && (
          <button
            type="button"
            onClick={handleSwitchMc400Rom}
            className="text-[10px] text-gray-500/40 hover:text-gray-400 transition-colors px-1"
            title={MC400_ROM_SWITCH[currentDeviceId].title}
          >
            {MC400_ROM_SWITCH[currentDeviceId].label}
          </button>
        )}

        {/* Sizing-mode selector — only meaningful while a device is on
            screen (i.e. not in the MAME bundle, which is its own iframe). */}
        {!showHome && !mameDeviceId && state === 'running' && (
          <SizingControls
            mode={sizingMode}
            scale={deviceScale}
            maxScale={maxDeviceScale}
            deviceMode={deviceMode}
            onToggleDeviceMode={() => setDeviceMode(v => !v)}
            onPickMode={handlePickSizingMode}
            onPickScale={handlePickDeviceScale}
            onEnterFullscreen={handleEnterFullscreen}
          />
        )}
      </header>

      {/* Device panel */}
      <DevicePanel
        profiles={sortedProfiles}
        currentDeviceId={effectiveDeviceId}
        savedDevices={savedDevices}
        isOpen={panelOpen}
        onClose={() => setPanelOpenIntent(false)}
        onSelect={handleSelectDevice}
        onClearSession={clearSession}
        onDownloadStates={exportAllStates}
        onImportStates={importStates}
        baseUrl={import.meta.env.BASE_URL}
        nonFavouriteDevices={nonFavouriteDevices}
        onOpenSettings={() => { setSettingsViewOpen(true); setPanelOpen(false); }}
        listError={error}
      />

      {/* Main content */}
      <main className="flex-1 relative">
        {showHome ? (
          <Home
            baseUrl={import.meta.env.BASE_URL}
            onChooseDevice={() => setPanelOpenIntent(true)}
            onLaunchDevice={handleLaunchFromHome}
            launchableIds={launchableIds}
            loadsById={deviceLoads}
          />
        ) : settingsViewOpen ? (
          <SettingsView
            profiles={sortedProfiles}
            baseUrl={import.meta.env.BASE_URL}
            sortMode={sortMode}
            onSortModeChange={setSortMode}
            nonFavouriteDevices={nonFavouriteDevices}
            onNonFavouriteDevicesChange={setNonFavouriteDevices}
            showDebugging={showDebugging}
            experimentalFeatures={experimentalFeatures}
            onShowDebuggingChange={setShowDebugging}
            onExperimentalFeaturesChange={setExperimentalFeatures}
            lcdAccuracyMode={lcdAccuracyMode}
            onLcdAccuracyModeChange={setLcdAccuracyMode}
            leaderboard={leaderboard}
            onClose={() => {
              setSettingsViewOpen(false);
              // No device loaded → main area would land on an empty
              // state. Reopen the device picker so Back from Settings
              // always lands somewhere useful.
              if (!currentDeviceId && !mameDeviceId) setPanelOpenIntent(true);
            }}
          />
        ) : mameDeviceId ? (
          <MameFrame
            baseUrl={import.meta.env.BASE_URL}
            deviceId={mameDeviceId}
            deviceName={mergedProfiles.find(p => p.id === mameDeviceId)?.displayName ?? mameDeviceId}
            onClose={closeMame}
            onSwitchToNative={handleSwitchToNative}
          />
        ) : (
          <>
            {state === 'loading-wasm' && <LoadingOverlay message="Loading emulator…" />}

            {state === 'error' && (
              <div className="flex flex-col items-center justify-center py-20 gap-4">
                <p className="text-red-600 font-mono text-sm">
                  {lastAttemptRef.current ? 'Couldn’t load the device' : 'Something went wrong'}
                </p>
                <p className="text-gray-500 text-xs max-w-md text-center">{error}</p>
                {lastAttemptRef.current && (
                  <button
                    onClick={() => {
                      const a = lastAttemptRef.current;
                      if (a) void handleSelectDevice(a.deviceId, a.romUrl);
                    }}
                    className="px-4 py-1.5 rounded bg-psion-highlight text-psion-charcoal font-mono text-xs hover:brightness-95 transition"
                  >
                    Try again
                  </button>
                )}
                <button
                  onClick={() => window.location.reload()}
                  className="text-xs text-gray-400 hover:text-psion-charcoal transition-colors"
                >
                  Reload page
                </button>
              </div>
            )}

            {state === 'loading-rom' && (() => {
              const lid = loadingDeviceId ?? currentDeviceId;
              const photoFile = deviceMode ? getDeviceSkinPhotoFilename(lid) : null;
              return (
                <LoadingOverlay
                  key={lid ?? 'device'}
                  message="Loading device…"
                  detail={loadStatus}
                  skinFilename={photoFile ?? getSkinFilename('', lid) ?? undefined}
                  skinFolder={photoFile ? 'device-skins' : 'skins'}
                  progress={loadProgress}
                />
              );
            })()}

            {state === 'running' && (
              <EmulatorView
                controls={controls}
                sizingMode={sizingMode}
                deviceScale={deviceScale}
                fullscreenRequest={fullscreenRequest}
                showDebugging={showDebugging}
                experimentalFeatures={experimentalFeatures}
                lcdAccuracyMode={lcdAccuracyMode}
                backlight={backlight}
                deviceMode={deviceMode}
                onMaxDeviceScale={setMaxDeviceScale}
              />
            )}
          </>
        )}
      </main>

      {tryNotice && (
        <TryAppNotice
          notice={tryNotice}
          onClose={() => {
            // Closing mid-delivery cancels it (and tears the Remote Link
            // down cleanly) rather than letting it run on unobserved.
            tryAbortRef.current?.abort();
            setTryNotice(null);
          }}
        />
      )}
    </div>
  );
}

function formatBytes(n: number): string {
  if (n < 1024)        return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

// Corner card narrating an app-library "Try it" delivery: progress
// phases while the bundle lands on the device — a determinate bar with
// percentage once the Remote Link upload starts reporting bytes — then
// the on-device install steps (which stay up until dismissed — the
// user needs them while driving the device).
function TryAppNotice({ notice, onClose }: {
  notice: { app: AppEntry; phase: string; progress?: { bytes: number; total: number };
    result?: DeliveryResult; error?: string };
  onClose(): void;
}) {
  const { app, phase, progress, result, error } = notice;
  const pct = progress
    ? (progress.total > 0 ? Math.floor((progress.bytes * 100) / progress.total) : 100)
    : 0;
  return (
    <div className="fixed bottom-4 right-4 z-50 max-w-sm w-[calc(100%-2rem)] bg-psion-dark border border-psion-accent/60 rounded-lg shadow-lg">
      <div className="flex items-center justify-between px-3 py-2 border-b border-psion-accent/40 bg-psion-mid rounded-t-lg">
        <span className="text-xs font-mono font-semibold text-psion-charcoal truncate">
          {app.name}
        </span>
        <button
          onClick={onClose}
          className="text-xs font-mono text-gray-400 hover:text-amber-700 transition-colors cursor-pointer ml-2"
        >
          Close
        </button>
      </div>
      <div className="px-3 py-2.5 space-y-1.5">
        {error ? (
          <>
            <p className="text-[11px] font-mono text-red-700">Couldn't deliver the app: {error}</p>
            <p className="text-[11px] font-mono text-gray-500">
              You can still <a href="#/apps" className="underline">download it from the library</a> and
              transfer it via the CF Card / SSD / Remote Link dialogs.
            </p>
          </>
        ) : result ? (
          <>
            <p className="text-[11px] font-mono text-green-700">✓ {result.summary}</p>
            <ol className="list-decimal pl-4 space-y-0.5 text-[11px] font-mono text-psion-charcoal">
              {result.steps.map((s, i) => <li key={i}>{s}</li>)}
            </ol>
          </>
        ) : (
          <>
            <p className="text-[11px] font-mono text-amber-700">⏳ {phase}</p>
            {progress && (
              <div className="w-full">
                <div
                  role="progressbar"
                  aria-label={phase}
                  aria-valuemin={0}
                  aria-valuemax={100}
                  aria-valuenow={pct}
                  className="h-2 bg-psion-charcoal/10 rounded-full overflow-hidden"
                >
                  <div
                    className="h-full rounded-full bg-psion-highlight transition-all"
                    style={{ width: `${pct}%` }}
                  />
                </div>
                <p className="text-[11px] font-mono text-psion-charcoal/70 mt-1 tabular-nums">
                  {pct}% · {formatBytes(progress.bytes)} / {formatBytes(progress.total)}
                </p>
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );
}

// ── Embed index (#/embed or #/embed/) ───────────────────────────────────
//
// Reference page for whoever is setting up an iframe: lists every
// supported device ID alongside its display name and the exact iframe
// URL to use. Loads the WASM module just to read the device registry
// (no ROM is fetched here) — cheap enough on the rare visit, and means
// the list always matches what the iframe routes will actually serve.
function EmbedIndex() {
  const [profiles, setProfiles] = useState<DeviceProfile[] | null>(null);
  const [loadErr, setLoadErr] = useState<string | null>(null);
  useEffect(() => {
    let cancelled = false;
    loadPsionModule()
      .then(mod => {
        if (cancelled) return;
        setProfiles(JSON.parse(mod.getAllDeviceProfilesJSON()) as DeviceProfile[]);
      })
      .catch(err => { if (!cancelled) setLoadErr(String(err)); });
    return () => { cancelled = true; };
  }, []);

  // Absolute base URL the embedder will paste into their iframe `src`,
  // computed from the current location so it picks up the deployment's
  // origin and base path (e.g. https://example.com/psion/) without
  // having to hardcode anything. Strips the existing hash so the
  // example URL is clean.
  const baseUrl = typeof window !== 'undefined'
    ? `${window.location.origin}${window.location.pathname}`
    : '';

  return (
    <div className="min-h-screen bg-psion-dark font-mono text-psion-charcoal px-6 py-8">
      <div className="max-w-3xl mx-auto flex flex-col gap-6">
        <header className="flex flex-col gap-1">
          <h1 className="text-xl font-semibold">Psion emulator — embed devices</h1>
          <p className="text-sm text-gray-600">
            Each device below can be embedded as an iframe in any site.
            The iframe renders just the device — no app header, no
            device picker, no control bar — and scales to fit the
            iframe's width and height.
          </p>
        </header>

        <section className="flex flex-col gap-2">
          <h2 className="text-sm font-semibold uppercase tracking-wide text-gray-500">
            How to embed
          </h2>
          <p className="text-sm">
            Point an iframe at <code className="px-1 py-0.5 bg-psion-mid rounded">{baseUrl}#/embed/&lt;deviceId&gt;</code>{' '}
            using one of the IDs listed below.
          </p>
          <pre className="bg-psion-mid border border-psion-accent/30 rounded p-3 text-xs overflow-x-auto whitespace-pre">
{`<iframe
  src="${baseUrl}#/embed/mc218"
  width="900" height="450"
  style="border: 0;"
  allow="clipboard-read; clipboard-write"
></iframe>`}
          </pre>
          <p className="text-xs text-gray-500">
            Sizing is responsive — the device scales to fit the iframe.
            Pick a width/height matching the device's aspect ratio for
            the cleanest layout (the MC218 is roughly 2.2:1).
          </p>
        </section>

        <section className="flex flex-col gap-2">
          <h2 className="text-sm font-semibold uppercase tracking-wide text-gray-500">
            Supported device IDs
          </h2>
          {loadErr && (
            <p className="text-sm text-red-600">
              Failed to load device registry: {loadErr}
            </p>
          )}
          {!loadErr && !profiles && (
            <p className="text-sm text-gray-500 italic">Loading device list…</p>
          )}
          {profiles && (
            <ul className="flex flex-col divide-y divide-psion-accent/20 border border-psion-accent/30 rounded overflow-hidden">
              {profiles
                .filter(p => p.status === 'supported')
                .map(p => {
                  const href = `${baseUrl}#/embed/${p.id}`;
                  return (
                    <li key={p.id} className="flex items-center justify-between gap-3 px-3 py-2 bg-psion-mid/50 hover:bg-psion-mid transition-colors">
                      <span className="flex flex-col">
                        <code className="text-sm font-semibold">{p.id}</code>
                        <span className="text-xs text-gray-600">{p.displayName}</span>
                      </span>
                      <a
                        href={href}
                        className="text-xs text-psion-accent hover:text-psion-charcoal underline whitespace-nowrap"
                        target="_blank"
                        rel="noopener noreferrer"
                      >
                        Open ↗
                      </a>
                    </li>
                  );
                })}
            </ul>
          )}
        </section>
      </div>
    </div>
  );
}

// ── Standalone embed route (#/embed/<deviceId>) ─────────────────────────
//
// Renders a single device with NO page chrome — no header, no device
// panel, no control bar — so the page can be embedded as an iframe in a
// third-party site (currently used to host the MC218). The emulator
// fills the iframe's viewport; the embedder controls the iframe's
// width/height and the device scales to fit. Failure modes (unknown
// device ID, ROM fetch error, WASM init error) render a minimal
// centred error message rather than the main app's full error chrome.
function EmbedApp({ deviceId }: { deviceId: string }) {
  // embedMode tells useEmulator to skip LAST_DEVICE_KEY read/write so
  // visiting the iframe doesn't change which device the main app loads
  // (they share localStorage at the same origin).
  const controls = useEmulator({ embedMode: true });
  const { state, loadProgress, loadStatus, error, profiles, loadDevice } = controls;

  // Trigger the device load exactly once, after WASM init completes. We
  // re-fire if the deviceId in the URL changes (e.g. the embedder
  // updates the iframe src to switch device). The profile lookup
  // doubles as validation — unknown IDs land on a friendly error rather
  // than a WASM trap.
  const [loadStarted, setLoadStarted] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  useEffect(() => {
    if (state !== 'ready' || loadStarted) return;
    const profile = profiles.find(p => p.id === deviceId && p.status === 'supported');
    if (!profile) {
      setLoadError(`Unknown device "${deviceId}"`);
      return;
    }
    setLoadStarted(true);
    const romUrl = `${import.meta.env.BASE_URL}roms/${profile.romFilename}`;
    void loadDevice(deviceId, romUrl);
  }, [state, deviceId, profiles, loadStarted, loadDevice]);

  // Backlight stays off in embed mode — the simulated EL panel is a
  // cosmetic feature (and its 3mx fallback is a known partial); without
  // the header/control-bar toggles to opt in or out, leaving it on
  // would surprise embedders who aren't expecting their page to dim.
  const dummyBacklight = { color: undefined, on: false, toggle: () => {} };

  if (loadError || (state === 'error' && error)) {
    return (
      <div className="fixed inset-0 flex items-center justify-center bg-psion-mid font-mono text-sm text-psion-charcoal px-4">
        {loadError ?? error}
      </div>
    );
  }
  if (state === 'running') {
    return (
      <EmulatorView
        controls={controls}
        sizingMode="fill"
        deviceScale={2}
        fullscreenRequest={0}
        showDebugging={false}
        experimentalFeatures={false}
        backlight={dummyBacklight}
        chromeless
      />
    );
  }
  // loading-wasm / loading-rom / ready (before our effect kicks in) all
  // show the same minimal centred spinner. The page is supposed to be
  // the device, not a busy multi-step loader.
  return (
    <div className="fixed inset-0 flex items-center justify-center bg-psion-mid">
      <LoadingOverlay
        message={state === 'loading-wasm' ? 'Loading emulator…' : 'Loading device…'}
        detail={state === 'loading-rom' ? loadStatus : undefined}
        skinFilename={state === 'loading-rom' ? (getSkinFilename('', deviceId) ?? undefined) : undefined}
        progress={state === 'loading-rom' ? loadProgress : undefined}
      />
    </div>
  );
}

// Strip redundant manufacturer / "Series" prefixes from a device's
// `displayName` so the header label stays short enough to share the row
// with the audio + sizing buttons on mobile widths. Removes (in order,
// case-insensitive at word boundaries) "Psion ", "Series ", "Ericsson ",
// "Oregon Scientific ". The Acorn Pocket Book devices aren't stripped —
// "Pocket Book" alone is too ambiguous on its own.
//
// Examples:
//   "Psion Series 5mx Pro"     → "5mx Pro"
//   "Psion Revo"               → "Revo"
//   "Ericsson MC218"           → "MC218"
//   "Oregon Scientific Osaris" → "Osaris"
//   "Psion Series 7"           → "7"
//   "Acorn Pocket Book II"     → "Acorn Pocket Book II"   (unchanged)
function shortenDeviceName(name: string): string {
  // Apply prefixes in this specific order — "Oregon Scientific" before
  // a hypothetical lone "Oregon", "Psion " before "Series " so the
  // common "Psion Series ..." case collapses cleanly.
  const prefixes = ['Oregon Scientific ', 'Ericsson ', 'Psion ', 'Series '];
  let out = name;
  for (const p of prefixes) {
    // Match at start of string OR right after a previous strip — re-run
    // each prefix until it no longer matches so "Psion Series X" reduces
    // to "X" in one pass.
    while (out.toLowerCase().startsWith(p.toLowerCase())) {
      out = out.slice(p.length);
    }
  }
  return out;
}

// ── Header audio controls ──────────────────────────────────────────────
//
// Speaker + Mic toggles in the global header. Same icon-button style as
// the SizingControls on the right: w-7 h-7 rounded, idle text-only
// hover, active = psion-highlight (yellow) ring + dark text. The
// vertical divider on the LEFT goes between the device title and these
// controls per the spec.
function AudioControls({
  speakerEnabled,
  micEnabled,
  hasMic,
  onToggleSpeaker,
  onToggleMic,
}: {
  speakerEnabled: boolean;
  micEnabled: boolean;
  // False on speaker-only hardware (buzzer-era SIBO devices, Siena,
  // Workabout) — the Mic button is hidden entirely so users aren't
  // offered an input the machine never had.
  hasMic: boolean;
  onToggleSpeaker: () => void;
  onToggleMic: () => void;
}) {
  const iconClass = 'block';
  const baseBtn = 'inline-flex items-center justify-center w-7 h-7 rounded transition-colors';
  const activeBtn = `${baseBtn} bg-psion-highlight text-psion-charcoal ring-1 ring-psion-accent`;
  const idleBtn   = `${baseBtn} text-psion-charcoal/60 hover:text-psion-charcoal hover:bg-psion-mid`;

  return (
    <div
      className="flex items-center gap-1 border-l border-psion-accent/30 pl-2 ml-1"
      role="group"
      aria-label="Audio"
    >
      {/* Speaker icon — cone + sound waves. Active = sound is routed to
          the browser output. */}
      <button
        type="button"
        onClick={onToggleSpeaker}
        className={speakerEnabled ? activeBtn : idleBtn}
        title={speakerEnabled
          ? 'Speaker on — emulator audio is routed to the browser'
          : 'Speaker off — click to route emulator audio to the browser'}
        aria-pressed={speakerEnabled}
        // Accessible name starts with "Speaker" so the existing
        // Playwright audio harnesses (test-audio-browser.mjs,
        // test-revo-audio.mjs) can still match via /^speaker/i.
        aria-label={speakerEnabled ? 'Speaker on' : 'Speaker off'}
      >
        <svg className={iconClass} width="16" height="16" viewBox="0 0 24 24" fill="none"
             stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
          {/* Speaker body: a trapezoid cone aimed right. */}
          <path d="M4 9v6h4l5 4V5L8 9H4z" fill="currentColor" stroke="none" />
          {/* Two sound-wave arcs when enabled, drawn but visually
              muted by currentColor when idle.  */}
          <path d="M16 8.5a5 5 0 0 1 0 7" />
          <path d="M19 6a8 8 0 0 1 0 12" />
        </svg>
      </button>

      {/* Mic icon — capsule with a stand. Active = browser mic is
          captured and routed into the emulator. Hidden when the
          hardware has no microphone. */}
      {hasMic && <button
        type="button"
        onClick={onToggleMic}
        className={micEnabled ? activeBtn : idleBtn}
        title={micEnabled
          ? 'Microphone on — browser mic is captured for voice memos'
          : 'Microphone off — click to grant mic access for voice memos'}
        aria-pressed={micEnabled}
        // Accessible name starts with "Microphone" so the existing
        // Playwright audio harnesses can still match via /^mic/i.
        aria-label={micEnabled ? 'Microphone on' : 'Microphone off'}
      >
        <svg className={iconClass} width="16" height="16" viewBox="0 0 24 24" fill="none"
             stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
          {/* Mic capsule. */}
          <rect x="9" y="3" width="6" height="11" rx="3" fill="currentColor" stroke="none" />
          {/* Stand: U under the capsule + the vertical post + base. */}
          <path d="M5 11a7 7 0 0 0 14 0" />
          <path d="M12 18v3" />
          <path d="M9 21h6" />
        </svg>
      </button>}
    </div>
  );
}

// ── Header backlight toggle ────────────────────────────────────────────
//
// Lightbulb icon mirroring the speaker/mic style. When the device's
// backlight is on, the button picks up a tinted background derived from
// the live backlight colour so the same affordance reads as "lit". On
// mobile this is the most reliable trigger — Cmd+Space / Super+Space are
// usually swallowed by the host OS, and the in-view control bar can be
// pushed off-screen by the soft keyboard.
function BacklightControl({
  on,
  color,
  onToggle,
}: {
  on: boolean;
  color: string;
  onToggle: () => void;
}) {
  // Red border marks this as an Experimental-features-gated control,
  // matching the in-view Light / Modem buttons in EmulatorView. The
  // border stays the same colour whether the backlight is on or off
  // so the signal doesn't disappear when the user lights the panel.
  const baseBtn = 'inline-flex items-center justify-center w-7 h-7 rounded border border-red-600 transition-colors';
  const idleBtn = `${baseBtn} text-psion-charcoal/60 hover:text-psion-charcoal hover:bg-psion-mid`;
  // When the backlight is on, paint the button with the actual EL
  // colour so the icon itself reads as glowing. Charcoal foreground
  // keeps the bulb legible against the pale tint.
  const litBtn = `${baseBtn} text-psion-charcoal`;

  return (
    <div
      className="flex items-center gap-1 border-l border-psion-accent/30 pl-2 ml-1"
      role="group"
      aria-label="Backlight"
    >
      <button
        type="button"
        onClick={onToggle}
        className={on ? litBtn : idleBtn}
        style={on ? { backgroundColor: color } : undefined}
        title={on
          ? 'Backlight on — tap to switch off (also: Fn+Space)'
          : 'Backlight off — tap to switch on (also: Fn+Space)'}
        aria-pressed={on}
        aria-label={on ? 'Backlight on' : 'Backlight off'}
      >
        {/* Lightbulb glyph: bulb body + filament + screw base. */}
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none"
             stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
          <path d="M9 18h6" />
          <path d="M10 21h4" />
          <path d="M12 3a6 6 0 0 0-4 10.5c.8.7 1.5 1.6 1.8 2.5h4.4c.3-.9 1-1.8 1.8-2.5A6 6 0 0 0 12 3z"
                fill={on ? 'currentColor' : 'none'} fillOpacity={on ? 0.25 : 0} />
        </svg>
      </button>
    </div>
  );
}

// ── Header sizing-mode controls ────────────────────────────────────────
//
// Three icon buttons that select how the device occupies the page:
//   • Device sized — fixed ~hand-held proportions (the default and the
//     persisted fall-back when an old saved "fill" mode would be cramped
//     on a small viewport)
//   • Fill window — responsive, fills the browser width
//   • Fullscreen  — triggers the same enter-fullscreen path the old
//     in-control-bar "Fullscreen" button used. Not persisted: a reload
//     can't re-enter fullscreen without a fresh user gesture, so this
//     icon stays "unselected" even after clicking it. The corresponding
//     Exit-fullscreen affordance lives inside the fullscreen overlay
//     itself (Esc for native FS, the corner X for the CSS overlay, and
//     the in-control-bar Exit button kept around for the case where
//     neither of those is reachable).
// Single zoom-icon glyph used both for the always-visible trigger (drawn
// with the current `scale`) and the 4 chips inside the hover flyout.
// The "N×" label sits inside the screen rectangle of a stylised device.
function ScaleIcon({ n }: { n: DeviceScale }) {
  return (
    <svg className="block" width="16" height="16" viewBox="0 0 24 24" fill="none"
         stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
      <rect x="3" y="5" width="18" height="14" rx="1.5" />
      <text x="12" y="16" fontSize="10" fontWeight="700" textAnchor="middle"
            fill="currentColor" stroke="none">{n}×</text>
    </svg>
  );
}

function SizingControls({
  mode,
  scale,
  maxScale,
  deviceMode,
  onToggleDeviceMode,
  onPickMode,
  onPickScale,
  onEnterFullscreen,
}: {
  mode: SizingMode;
  scale: DeviceScale;
  // Largest scale this window can actually draw (see maxDeviceScale above).
  // Anything past it is offered greyed out rather than silently substituted.
  maxScale: number;
  deviceMode: boolean;
  onToggleDeviceMode: () => void;
  onPickMode: (mode: SizingMode) => void;
  onPickScale: (scale: DeviceScale) => void;
  onEnterFullscreen: () => void;
}) {
  // Stroke-based SVGs so the icons inherit currentColor and stay legible
  // against both the active (yellow) and inactive (transparent) backgrounds.
  const iconClass = 'block';
  const baseBtn = 'inline-flex items-center justify-center w-7 h-7 rounded transition-colors';
  const activeBtn  = `${baseBtn} bg-psion-highlight text-psion-charcoal ring-1 ring-psion-accent`;
  const idleBtn    = `${baseBtn} text-psion-charcoal/60 hover:text-psion-charcoal hover:bg-psion-mid`;
  // A scale too big for this window. Shown rather than hidden so the row
  // doesn't reshuffle as the window resizes, and so it's visible that 3×
  // and 4× exist and are simply out of reach here.
  const outOfReachBtn = `${baseBtn} text-psion-charcoal/25 cursor-not-allowed`;
  // Past this the picture stops changing: the device is already as wide as
  // the window and a higher scale has nothing left to give. maxScale 0 means
  // even 1× overflows (a handheld on a phone), so the device is fitted to
  // the window and 1× is the honest label for what you get.
  const topScale = Math.min(4, Math.max(1, maxScale)) as DeviceScale;

  // The flyout opens on hover *or* keyboard focus inside the wrapper, and
  // closes when the pointer leaves the wrapper entirely (so moving between
  // the trigger button and the chip row keeps it open). Driving this from
  // React state — rather than purely CSS — makes the focus behaviour
  // reliable across browsers and lets us close cleanly on chip click.
  const [flyoutOpen, setFlyoutOpen] = useState(false);

  return (
    <div className="ml-auto flex items-center gap-1" role="group" aria-label="Device sizing">
      {/* Device mode — toggles photo-realistic device skins from the
          device-skins/ folder. Sits left of the sizing controls with a
          thin vertical divider separating them. */}
      <button
        type="button"
        onClick={onToggleDeviceMode}
        className={deviceMode ? activeBtn : idleBtn}
        title={deviceMode ? 'Device mode on — click to hide device skin' : 'Device mode off — click to show device skin'}
        aria-pressed={deviceMode}
      >
        <svg className={iconClass} width="16" height="16" viewBox="0 0 24 24" fill="none"
             stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
          {/* Clamshell palmtop body — landscape, mirrors the 5mx wireframe:
              a wide LCD up top and a keyboard row along the bottom. */}
          <rect x="2" y="4" width="20" height="16" rx="2" />
          {/* LCD screen */}
          <rect x="4.5" y="6" width="15" height="7" rx="0.5" fill="currentColor" fillOpacity="0.25" stroke="currentColor" strokeWidth="1.2" />
          {/* Keyboard — two rows of keys plus a spacebar */}
          <line x1="5" y1="15.2" x2="19" y2="15.2" strokeWidth="1.3" strokeDasharray="1.3 1.1" />
          <line x1="5" y1="17.4" x2="19" y2="17.4" strokeWidth="1.3" strokeDasharray="1.3 1.1" />
        </svg>
      </button>

      {/* Thin vertical divider between device-mode and sizing controls */}
      <span className="w-px h-4 bg-psion-accent/30 self-center mx-0.5" aria-hidden="true" />
      {/* Actual size — pixel-perfect zoom. The button itself shows the
          currently selected scale (1×/2×/3×/4×); hovering reveals a chip
          row to switch between them. Clicking the trigger reasserts the
          current scale and ensures device-mode is active. */}
      <div
        className="relative"
        onMouseEnter={() => setFlyoutOpen(true)}
        onMouseLeave={() => setFlyoutOpen(false)}
        onFocus={() => setFlyoutOpen(true)}
        onBlur={(e) => {
          if (!e.currentTarget.contains(e.relatedTarget as Node | null)) setFlyoutOpen(false);
        }}
      >
        <button
          type="button"
          onClick={() => onPickScale(scale)}
          className={mode === 'device' ? activeBtn : idleBtn}
          // The icon shows the level being drawn, not the one stored: a 4×
          // preference carried over from a bigger window fills this one, and
          // a button reading 4× over a window-width picture is the thing
          // that made this control feel broken. The preference is kept, and
          // the icon goes back up when the window does.
          title={scale > topScale
            ? `Actual size (${topScale}× fills this window — ${scale}× needs a bigger one)`
            : `Actual size (${scale}×) — hover for other zoom levels`}
          aria-pressed={mode === 'device'}
          aria-haspopup="menu"
          aria-expanded={flyoutOpen}
        >
          <ScaleIcon n={scale > topScale ? topScale : scale} />
        </button>
        {flyoutOpen && (
          // Sits flush with the trigger (top-full, no margin) so hovering
          // between button and flyout never crosses dead pixels. Anchored
          // to the right of the wrapper to keep it on-screen even when
          // the controls cluster sits at the far edge of the header.
          <div
            className="absolute right-0 top-full flex items-center gap-1 p-1 rounded
                       bg-psion-light ring-1 ring-psion-mid shadow-lg z-20"
            role="menu"
            aria-label="Actual-size zoom level"
          >
            {([1, 2, 3, 4] as DeviceScale[]).map(n => {
              const isActive = mode === 'device' && scale === n;
              // Out of reach in this window: the device already fills it at
              // topScale, so picking a higher one draws the identical
              // picture and the chip looks inert. Say so instead — the level
              // comes back the moment the window is big enough (or the
              // machine is turned).
              const outOfReach = n > topScale;
              return (
                <button
                  key={n}
                  type="button"
                  disabled={outOfReach}
                  onClick={() => { onPickScale(n); setFlyoutOpen(false); }}
                  className={outOfReach ? outOfReachBtn : isActive ? activeBtn : idleBtn}
                  title={outOfReach
                    ? `${n}× actual size — this window is already full at ${topScale}×`
                    : `${n}× actual size`}
                  role="menuitemradio"
                  aria-checked={isActive}
                >
                  <ScaleIcon n={n} />
                </button>
              );
            })}
          </div>
        )}
      </div>

      {/* Fill window — a wide inner rectangle that fills the viewport
          width, suggesting the device stretching across the browser. */}
      <button
        type="button"
        onClick={() => onPickMode('fill')}
        className={mode === 'fill' ? activeBtn : idleBtn}
        title="Fill window — device grows to fit the browser width"
        aria-pressed={mode === 'fill'}
      >
        <svg className={iconClass} width="16" height="16" viewBox="0 0 24 24" fill="none"
             stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
          <rect x="3" y="5" width="18" height="14" rx="1.5" />
          <rect x="4" y="9" width="16" height="6" rx="0.5" fill="currentColor" />
        </svg>
      </button>

      {/* Fullscreen — classic 4-corner expand glyph. */}
      <button
        type="button"
        onClick={onEnterFullscreen}
        className={idleBtn}
        title="Fullscreen"
        aria-label="Enter fullscreen"
      >
        <svg className={iconClass} width="16" height="16" viewBox="0 0 24 24" fill="none"
             stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
          <path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5" />
        </svg>
      </button>
    </div>
  );
}
