// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useMemo, useRef, useState, useCallback } from 'react';
import psionLogoUrl from '../assets/psion-logo.svg';
import DevicePanel from './DevicePanel';
import { loadPsionModule } from '../lib/wasmBridge';
import { listSavedDevices } from '../hooks/useEmulator';
import { DEVICE_RELEASE_YEARS } from '../lib/deviceMeta';
import type { DeviceProfile } from '../types/emulator';
import {
  fetchAppManifest, appAssetUrl, tryTargetsFor, setPendingTry,
  BOOTLOADER_TRY_DEVICES, appsRouteHash, parseAppsRoute,
  type AppManifest, type AppEntry,
} from '../lib/appLibrary';
import { unzipAll } from '../lib/zip';
import { trackAppEvent, fetchAppLeaderboard } from '../lib/analytics';

interface Props {
  onClose(): void;
  // Device id from `#/apps?device=<id>`: opens the library filtered to
  // that machine instead of restoring the user's last filters.
  initialDeviceFilter?: string | null;
  // App id from `#/apps?app=<category>/<slug>`: the app whose details
  // popup is open. The route owns that — see openDetail below.
  appId?: string | null;
}

// Standalone app-library route (#/apps): browse, search and sort the
// 3-Lib shareware catalogue, download any app, or "Try it" — which
// hands off to App.tsx to boot the right device and deliver the app
// onto it (CF card / SSD pack / Remote Link; see lib/appLibrary.ts).
//
// The manifest + bundles are deploy-time artefacts (dist/apps); when
// they're unreachable (offline, or a dev build without the generator
// run) the page degrades to a friendly notice instead of breaking —
// the rest of the emulator works fully offline, the library is the one
// part that needs the network on first visit.

type SortMode = 'name' | 'newest' | 'popular' | 'size';

const PAGE_SIZE = 48;

// Filter state survives leaving the page (e.g. "Try it" → back) via
// localStorage; Reset restores the defaults below.
const FILTERS_KEY = 'psion.applib.filters';

interface Filters {
  search: string;
  deviceFilter: string;
  categoryFilter: string;
  sort: SortMode;
  showVault: boolean;
  // Hide apps that can only be downloaded (no "Try it" target) — on by
  // default so the library leads with what runs in the emulator.
  hideDownloadOnly: boolean;
}

const DEFAULT_FILTERS: Filters = {
  search: '', deviceFilter: 'all', categoryFilter: 'all', sort: 'popular', showVault: false,
  hideDownloadOnly: true,
};

// A device named in the URL wins over the persisted filters entirely,
// rather than being ANDed with them: a link that promises "this
// machine's apps" has to show them, and a stale category or search from
// the user's last visit could otherwise leave the page empty.
function loadFilters(deviceFilter?: string | null): Filters {
  if (deviceFilter) return { ...DEFAULT_FILTERS, deviceFilter };
  try {
    const raw = localStorage.getItem(FILTERS_KEY);
    if (!raw) return DEFAULT_FILTERS;
    return { ...DEFAULT_FILTERS, ...JSON.parse(raw) as Partial<Filters> };
  } catch {
    return DEFAULT_FILTERS;
  }
}

function formatSize(bytes: number): string {
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
  if (bytes >= 1024) return `${Math.round(bytes / 1024)} KB`;
  return `${bytes} B`;
}

// Category labels are "device — genre" ("Series 3 — Games", "EPOC —
// Games"…). The filter dropdown is device-agnostic — the device filter
// next to it already covers platforms — so it groups categories by the
// genre half. Labels without the separator (Siena, GeoFox One) are
// whole-device catch-alls and keep their full label.
function categoryGenre(label: string): string {
  const i = label.indexOf(' — ');
  return i >= 0 ? label.slice(i + 3) : label;
}

// The route for opening (or closing) an app's details: whatever else is
// in the current one — the device filter — with the app swapped over.
function detailHash(app: string | null): string {
  return appsRouteHash({ ...parseAppsRoute(window.location.hash), app });
}

// Typographic placeholder for apps without an extractable icon.
function IconPlaceholder({ name }: { name: string }) {
  const initials = name.replace(/[^A-Za-z0-9]/g, '').slice(0, 2).toUpperCase() || '?';
  return (
    <div className="w-12 h-12 rounded bg-psion-mid border border-psion-accent/40 flex items-center justify-center flex-shrink-0">
      <span className="text-sm font-mono font-semibold text-psion-charcoal/60">{initials}</span>
    </div>
  );
}

function AppIcon({ app }: { app: AppEntry }) {
  const [failed, setFailed] = useState(false);
  if (!app.icon || failed) return <IconPlaceholder name={app.name} />;
  return (
    <img
      src={appAssetUrl(app.icon)}
      alt=""
      loading="lazy"
      onError={() => setFailed(true)}
      className="w-12 h-12 rounded bg-white border border-psion-accent/40 object-contain flex-shrink-0"
      style={{ imageRendering: 'pixelated' }}
    />
  );
}

export default function AppLibrary({ onClose, initialDeviceFilter, appId }: Props) {
  const [manifest, setManifest] = useState<AppManifest | null>(null);
  const [loadError, setLoadError] = useState<'offline' | 'missing' | string | null>(null);
  const [deviceNames, setDeviceNames] = useState<Record<string, string>>({});
  const [popularity, setPopularity] = useState<Map<string, number>>(new Map());

  const [initialFilters] = useState(() => loadFilters(initialDeviceFilter));
  const [search, setSearch] = useState(initialFilters.search);
  const [deviceFilter, setDeviceFilter] = useState(initialFilters.deviceFilter);
  const [categoryFilter, setCategoryFilter] = useState(initialFilters.categoryFilter);
  const [sort, setSort] = useState<SortMode>(initialFilters.sort);
  const [showVault, setShowVault] = useState(initialFilters.showVault);
  const [hideDownloadOnly, setHideDownloadOnly] = useState(initialFilters.hideDownloadOnly);
  const [visible, setVisible] = useState(PAGE_SIZE);

  // ── The details popup lives in the URL ──────────────────────────────
  // Which app is open is read back out of the route rather than held in
  // state, so the address bar always names what's on screen and that URL
  // is the shareable link to the app. Opening pushes a history entry;
  // Back therefore closes the popup, and Close undoes its own push so
  // the two agree. Someone arriving on a shared link pushed nothing, so
  // Close navigates to the plain library instead of leaving the site.
  const selected = useMemo(
    () => (appId ? manifest?.apps.find(a => a.id === appId) ?? null : null),
    [manifest, appId]);
  const pushedDetail = useRef(false);
  useEffect(() => { if (!selected) pushedDetail.current = false; }, [selected]);

  const openDetail = useCallback((app: AppEntry) => {
    pushedDetail.current = true;
    window.location.hash = detailHash(app.id);
  }, []);

  const closeDetail = useCallback(() => {
    if (pushedDetail.current) {
      pushedDetail.current = false;
      history.back();
    } else {
      window.location.hash = detailHash(null);
    }
  }, []);

  // ── Device side menu, hosted right here so opening it doesn't leave
  // the page. Profiles come from the WASM device registry (cached
  // module — same trick as the landing DeviceSelector); the
  // session-management affordances stay hidden (no handlers passed).
  const [menuOpen, setMenuOpen] = useState(false);
  const [panelProfiles, setPanelProfiles] = useState<DeviceProfile[]>([]);
  const [panelError, setPanelError] = useState<string | null>(null);
  const [savedDevices, setSavedDevices] = useState<string[]>([]);
  useEffect(() => { void listSavedDevices().then(setSavedDevices).catch(() => {}); }, []);
  const [nonFavourites, setNonFavourites] = useState<Set<string>>(new Set());
  const openMenu = useCallback(() => {
    setMenuOpen(true);
    if (panelProfiles.length === 0) {
      setPanelError(null);
      void loadPsionModule().then(mod => {
        const all = JSON.parse(mod.getAllDeviceProfilesJSON()) as DeviceProfile[];
        // Newest-first, the app's default device ordering.
        all.sort((a, b) =>
          (DEVICE_RELEASE_YEARS[b.id] ?? 0) - (DEVICE_RELEASE_YEARS[a.id] ?? 0) ||
          a.displayName.localeCompare(b.displayName));
        setPanelProfiles(all);
      }).catch(err => {
        // Surface the failure in the panel instead of an indefinite
        // "Loading…" (WASM instantiation can fail under memory pressure —
        // notably right after leaving the emulator, whose worker instance
        // is kept alive a few seconds for its exit save). Re-opening the
        // menu retries because panelProfiles is still empty.
        setPanelError(String(err));
      });
      void listSavedDevices().then(setSavedDevices).catch(() => {});
      try {
        const raw = localStorage.getItem('psion-non-favourite-devices');
        if (raw) setNonFavourites(new Set(JSON.parse(raw) as string[]));
      } catch { /* ignore */ }
    }
  }, [panelProfiles.length]);

  const filtersDirty = search !== '' || deviceFilter !== 'all' ||
    categoryFilter !== 'all' || sort !== 'popular' || showVault || !hideDownloadOnly;

  // Persist the filters whenever they change.
  useEffect(() => {
    try {
      localStorage.setItem(FILTERS_KEY, JSON.stringify(
        { search, deviceFilter, categoryFilter, sort, showVault, hideDownloadOnly } satisfies Filters));
    } catch { /* storage full/blocked — filters just won't persist */ }
  }, [search, deviceFilter, categoryFilter, sort, showVault, hideDownloadOnly]);

  const resetFilters = useCallback(() => {
    setSearch(DEFAULT_FILTERS.search);
    setDeviceFilter(DEFAULT_FILTERS.deviceFilter);
    setCategoryFilter(DEFAULT_FILTERS.categoryFilter);
    setSort(DEFAULT_FILTERS.sort);
    setShowVault(DEFAULT_FILTERS.showVault);
    setHideDownloadOnly(DEFAULT_FILTERS.hideDownloadOnly);
  }, []);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      try {
        const m = await fetchAppManifest();
        if (!cancelled) setManifest(m);
      } catch (e) {
        if (!cancelled) {
          const msg = e instanceof Error ? e.message : String(e);
          setLoadError(msg === 'offline' || msg === 'missing' ? msg : msg);
        }
      }
      const [names, rows] = await Promise.all([
        fetch(`${import.meta.env.BASE_URL}device-names.json`)
          .then(r => (r.ok ? r.json() : {})).catch(() => ({})),
        fetchAppLeaderboard(),
      ]);
      if (cancelled) return;
      setDeviceNames(names ?? {});
      setPopularity(new Map(rows.map(r => [r.appId, r.tryCount + r.downloadCount])));
    })();
    return () => { cancelled = true; };
  }, []);

  // Reset paging whenever the query changes.
  useEffect(() => { setVisible(PAGE_SIZE); },
    [search, deviceFilter, categoryFilter, sort, showVault, hideDownloadOnly]);

  const deviceOptions = useMemo(() => {
    const ids = new Set<string>();
    for (const a of manifest?.apps ?? []) for (const d of a.devices) ids.add(d);
    return [...ids].sort();
  }, [manifest]);

  const categoryLabel = useCallback((id: string) =>
    manifest?.categories.find(c => c.id === id)?.label ?? id, [manifest]);

  // Category id → genre, for the device-agnostic category filter.
  const categoryGenreById = useMemo(() => {
    const m = new Map<string, string>();
    for (const c of manifest?.categories ?? []) m.set(c.id, categoryGenre(c.label));
    return m;
  }, [manifest]);

  const categoryOptions = useMemo(() => {
    const names = new Set<string>();
    for (const c of manifest?.categories ?? []) {
      if (showVault || !c.vault) names.add(categoryGenre(c.label));
    }
    return [...names].sort((a, b) => a.localeCompare(b));
  }, [manifest, showVault]);

  // Migrate a persisted pre-grouping filter (old values were category
  // ids like 'epocgames') to its genre; anything unrecognised resets
  // to 'all' rather than silently matching nothing.
  useEffect(() => {
    if (!manifest) return;
    setCategoryFilter(f => {
      if (f === 'all') return f;
      const byId = manifest.categories.find(c => c.id === f);
      if (byId) return categoryGenre(byId.label);
      return manifest.categories.some(c => categoryGenre(c.label) === f) ? f : 'all';
    });
  }, [manifest]);

  // Bootloader devices (5mx Pro / netBook) are only usable targets once
  // the user has booted them before — a fresh load needs the CF slot for
  // the OS card.
  const deviceEnabled = useCallback((d: string) =>
    !BOOTLOADER_TRY_DEVICES.has(d) || savedDevices.includes(d), [savedDevices]);
  const enabledTargetsFor = useCallback((app: AppEntry) =>
    tryTargetsFor(app).filter(deviceEnabled), [deviceEnabled]);

  const filtered = useMemo(() => {
    if (!manifest) return [];
    const q = search.trim().toLowerCase();
    let apps = manifest.apps.filter(a =>
      (showVault || !a.vault) &&
      // "Download only" ≡ the card would show no Try button.
      (!hideDownloadOnly || enabledTargetsFor(a).length > 0) &&
      (categoryFilter === 'all' || categoryGenreById.get(a.category) === categoryFilter) &&
      (deviceFilter === 'all' || a.devices.includes(deviceFilter)) &&
      (q === '' || a.name.toLowerCase().includes(q) ||
       a.description.toLowerCase().includes(q) ||
       (a.section ?? '').toLowerCase().includes(q)));
    apps = apps.slice();
    switch (sort) {
      case 'name':    apps.sort((a, b) => a.name.localeCompare(b.name)); break;
      case 'newest':  apps.sort((a, b) => (b.date ?? '').localeCompare(a.date ?? '')); break;
      case 'size':    apps.sort((a, b) => b.sizeBytes - a.sizeBytes); break;
      case 'popular': apps.sort((a, b) =>
        (popularity.get(b.id) ?? 0) - (popularity.get(a.id) ?? 0) ||
        a.name.localeCompare(b.name));
        break;
    }
    return apps;
  }, [manifest, search, deviceFilter, categoryFilter, sort, showVault, popularity,
      categoryGenreById, hideDownloadOnly, enabledTargetsFor]);

  const handleDownload = useCallback((app: AppEntry) => {
    trackAppEvent(app.id, 'app_download');
    const a = document.createElement('a');
    a.href = appAssetUrl(app.zip);
    a.download = `${app.id.split('/').pop()}.zip`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  }, []);

  const handleTry = useCallback((app: AppEntry, deviceId: string) => {
    trackAppEvent(app.id, 'app_try');
    setPendingTry(app, deviceId);
    window.location.hash = `#/${deviceId}`;
  }, []);

  // The device a card's "Try it" boots: the active device filter wins
  // when that device can run the app, then the manifest's default.
  const tryDeviceFor = useCallback((app: AppEntry): string => {
    const targets = enabledTargetsFor(app);
    if (deviceFilter !== 'all' && targets.includes(deviceFilter)) return deviceFilter;
    if (app.tryDevice && targets.includes(app.tryDevice)) return app.tryDevice;
    return targets[0];
  }, [deviceFilter, enabledTargetsFor]);

  const input = 'text-xs font-mono px-2 py-1.5 rounded border border-psion-accent/50 bg-white text-psion-charcoal focus:outline-none focus:border-psion-accent';

  return (
    <div className="min-h-screen bg-psion-dark font-mono flex flex-col">
      <header className="border-b border-psion-accent/40 px-4 py-2 flex items-center gap-3 flex-shrink-0 bg-psion-dark shadow-sm">
        {/* Devices menu — the same slide-over panel as the emulator,
            hosted locally so it overlays the library instead of
            navigating away. Picking a device navigates to its route. */}
        <button
          onClick={openMenu}
          className="flex flex-col gap-1 p-1.5 rounded hover:bg-psion-mid transition-colors group"
          aria-label="Open device menu"
          title="Devices"
        >
          <span className="block w-5 h-0.5 bg-psion-charcoal/50 group-hover:bg-psion-charcoal transition-colors" />
          <span className="block w-5 h-0.5 bg-psion-charcoal/50 group-hover:bg-psion-charcoal transition-colors" />
          <span className="block w-5 h-0.5 bg-psion-charcoal/50 group-hover:bg-psion-charcoal transition-colors" />
        </button>
        <img src={psionLogoUrl} alt="Psion" className="h-8 w-auto select-none" draggable={false} />
        <span className="text-xs text-gray-400 ml-1 border-l border-psion-accent/30 pl-3">
          App library
        </span>
        <div className="flex-1" />
        <a href="#/usage" className="text-xs font-mono text-gray-500 hover:text-psion-charcoal transition-colors">
          Usage
        </a>
        <button
          onClick={onClose}
          className="text-xs font-mono text-gray-500 hover:text-psion-charcoal transition-colors"
          aria-label="Back to emulator"
        >
          ← Back
        </button>
      </header>

      <main className="flex-1 px-4 py-5 max-w-5xl w-full mx-auto">
        <p className="text-xs font-mono text-gray-500 mb-4">
          Classic Psion software, ready to run. Pick an app to <b>try it on an emulated device</b>, or download.
        </p>

        {loadError && (
          <div className="border border-psion-accent/40 rounded p-6 text-center bg-psion-mid/30 max-w-xl mx-auto my-12">
            <p className="text-sm font-mono text-psion-charcoal mb-2">
              {loadError === 'offline'
                ? 'The app library needs a network connection.'
                : 'The app library isn’t available in this build.'}
            </p>
            <p className="text-xs font-mono text-gray-500">
              {loadError === 'offline'
                ? 'The emulator itself works offline, but the app catalogue and bundles are fetched on demand. Reconnect and reload this page.'
                : 'Run scripts/build-app-library.mts to generate it locally, or use the deployed site.'}
            </p>
          </div>
        )}

        {!manifest && !loadError && (
          <p className="text-xs font-mono text-gray-400 py-8 text-center">Loading the catalogue…</p>
        )}

        {manifest && (
          <>
            {/* Toolbar */}
            <div className="flex flex-wrap gap-2 items-center mb-4">
              <input
                type="search"
                value={search}
                onChange={e => setSearch(e.target.value)}
                placeholder={`Search ${manifest.apps.length} apps…`}
                className={`${input} flex-grow min-w-40 max-w-xs`}
                aria-label="Search apps"
              />
              <select value={deviceFilter} onChange={e => setDeviceFilter(e.target.value)}
                      className={input} aria-label="Filter by device">
                <option value="all">All devices</option>
                {deviceOptions.map(d => (
                  <option key={d} value={d}>{deviceNames[d] ?? d}</option>
                ))}
              </select>
              <select value={categoryFilter} onChange={e => setCategoryFilter(e.target.value)}
                      className={input} aria-label="Filter by category">
                <option value="all">All categories</option>
                {categoryOptions.map(name => (
                  <option key={name} value={name}>{name}</option>
                ))}
              </select>
              <select value={sort} onChange={e => setSort(e.target.value as SortMode)}
                      className={input} aria-label="Sort">
                <option value="name">A → Z</option>
                <option value="popular">Most popular</option>
                <option value="newest">Newest</option>
                <option value="size">Largest</option>
              </select>
              <label className="flex items-center gap-1.5 text-xs font-mono text-gray-500 cursor-pointer select-none"
                     title="Hide apps that can only be downloaded (no emulated device can run them)">
                <input type="checkbox" checked={hideDownloadOnly}
                       onChange={e => setHideDownloadOnly(e.target.checked)} />
                Hide download-only
              </label>
              <label className="flex items-center gap-1.5 text-xs font-mono text-gray-500 cursor-pointer select-none">
                <input type="checkbox" checked={showVault}
                       onChange={e => setShowVault(e.target.checked)} />
                Include vault
              </label>
              {filtersDirty && (
                <button
                  onClick={resetFilters}
                  className="text-xs font-mono px-3 py-1.5 rounded border border-psion-accent bg-psion-mid text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors cursor-pointer"
                  title="Clear the search, filters and sort back to defaults"
                >
                  ✕ Reset filters
                </button>
              )}
            </div>

            <p className="text-[11px] font-mono text-gray-500 mb-3">
              {filtered.length} app{filtered.length === 1 ? '' : 's'}
              {deviceFilter !== 'all' ? ` for ${deviceNames[deviceFilter] ?? deviceFilter}` : ''}
            </p>

            {/* Card grid */}
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
              {filtered.slice(0, visible).map(app => {
                const targets = enabledTargetsFor(app);
                return (
                  <div key={app.id}
                       className="border border-psion-accent/30 rounded-lg bg-psion-mid/20 p-3 flex flex-col gap-2 hover:border-psion-accent/60 transition-colors">
                    <button className="flex items-start gap-3 text-left cursor-pointer"
                            onClick={() => openDetail(app)}>
                      <AppIcon app={app} />
                      <div className="min-w-0">
                        <div className="text-sm font-mono font-semibold text-psion-charcoal truncate">
                          {app.name}
                        </div>
                        <div className="text-[10px] font-mono text-gray-500 truncate">
                          {categoryLabel(app.category)}
                          {app.date ? ` · ${app.date.slice(0, 4)}` : ''} · {formatSize(app.sizeBytes)}
                          {(popularity.get(app.id) ?? 0) > 0 ? ` · ★ ${popularity.get(app.id)}` : ''}
                        </div>
                      </div>
                    </button>
                    <p className="text-[11px] font-mono text-gray-600 leading-snug flex-grow"
                       style={{ display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical', overflow: 'hidden' }}>
                      {app.description}
                    </p>
                    <div className="flex gap-2">
                      {targets.length > 0 && (
                        <button
                          onClick={() => handleTry(app, tryDeviceFor(app))}
                          className="text-xs font-mono px-2.5 py-1 rounded bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors cursor-pointer"
                          title="Boot an emulated device with this app delivered onto it"
                        >
                          ▸ Try it
                        </button>
                      )}
                      <button
                        onClick={() => handleDownload(app)}
                        className="text-xs font-mono px-2.5 py-1 rounded border border-psion-accent/50 text-gray-600 hover:bg-psion-accent hover:text-white transition-colors cursor-pointer"
                        title="Download the original files as a zip"
                      >
                        Download
                      </button>
                      <button
                        onClick={() => openDetail(app)}
                        className="text-xs font-mono px-2 py-1 text-gray-400 hover:text-psion-charcoal transition-colors cursor-pointer ml-auto"
                      >
                        Details…
                      </button>
                    </div>
                  </div>
                );
              })}
            </div>

            {filtered.length > visible && (
              <div className="text-center mt-4">
                <button
                  onClick={() => setVisible(v => v + PAGE_SIZE * 2)}
                  className="text-xs font-mono px-4 py-2 rounded border border-psion-accent/50 text-gray-600 hover:bg-psion-accent hover:text-white transition-colors cursor-pointer"
                >
                  Show more ({filtered.length - visible} remaining)
                </button>
              </div>
            )}

            {filtered.length === 0 && (
              <p className="text-xs font-mono text-gray-400 py-12 text-center">
                No apps match — try clearing the filters{showVault ? '' : ' or including the vault'}.
              </p>
            )}
          </>
        )}
      </main>

      <DevicePanel
        profiles={panelProfiles}
        currentDeviceId={null}
        savedDevices={savedDevices}
        isOpen={menuOpen}
        onClose={() => setMenuOpen(false)}
        onSelect={deviceId => { window.location.hash = `#/${deviceId}`; }}
        baseUrl={import.meta.env.BASE_URL}
        nonFavouriteDevices={nonFavourites}
        listError={panelError}
      />

      {selected && manifest && (
        // Keyed by app: the popup carries per-app state (unpacked
        // readmes, the chosen try device), and the route can move
        // straight from one app to another without closing.
        <AppDetail
          key={selected.id}
          app={selected}
          categoryLabel={categoryLabel(selected.category)}
          deviceNames={deviceNames}
          defaultDevice={tryDeviceFor(selected)}
          deviceEnabled={deviceEnabled}
          onTry={handleTry}
          onDownload={handleDownload}
          onClose={closeDetail}
        />
      )}

      <footer className="px-4 py-3 text-center">
        <p className="text-[10px] font-mono text-gray-500/70 max-w-2xl mx-auto">
          Apps are listed as assumed abandonware, preserved for historical
          interest. If you own the copyright to an app and wish to have it
          removed, or would like attribution added, please contact us.
        </p>
      </footer>
    </div>
  );
}

// ── Detail overlay ──────────────────────────────────────────────────

interface DetailProps {
  app: AppEntry;
  categoryLabel: string;
  deviceNames: Record<string, string>;
  defaultDevice?: string;
  // Bootloader devices stay listed but unselectable until booted once.
  deviceEnabled(d: string): boolean;
  onTry(app: AppEntry, deviceId: string): void;
  onDownload(app: AppEntry): void;
  onClose(): void;
}

// Just the filename, for tab labels — the manifest stores zip-relative
// paths (e.g. "docs/README.TXT").
function baseName(rel: string): string {
  return rel.split('/').pop() ?? rel;
}

function AppDetail({ app, categoryLabel, deviceNames, defaultDevice, deviceEnabled, onTry, onDownload, onClose }: DetailProps) {
  const targets = tryTargetsFor(app);
  const enabled = targets.filter(deviceEnabled);
  const [tryDevice, setTryDevice] = useState(
    defaultDevice && enabled.includes(defaultDevice) ? defaultDevice
      : app.tryDevice && enabled.includes(app.tryDevice) ? app.tryDevice : (enabled[0] ?? ''));

  // Readme-like docs from the bundle each become a tab next to Details
  // (tab -1). Their text is unpacked from the app zip the first time any
  // doc tab is opened — one fetch covers them all — and cached here.
  const [tab, setTab] = useState(-1);
  const [docs, setDocs] = useState<Record<string, string> | null>(null);
  const [docsBusy, setDocsBusy] = useState(false);
  const [docsError, setDocsError] = useState<string | null>(null);

  const loadDocs = useCallback(async () => {
    if (docs || docsBusy || app.readmes.length === 0) return;
    setDocsBusy(true);
    setDocsError(null);
    try {
      const resp = await fetch(appAssetUrl(app.zip));
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
      const files = await unzipAll(new Uint8Array(await resp.arrayBuffer()));
      const out: Record<string, string> = {};
      for (const rel of app.readmes) {
        const bytes = files.get(rel);
        out[rel] = bytes
          ? new TextDecoder('iso-8859-1').decode(bytes)
          : '(file missing from bundle)';
      }
      setDocs(out);
    } catch (e) {
      setDocsError(e instanceof Error ? e.message : String(e));
    } finally {
      setDocsBusy(false);
    }
  }, [app, docs, docsBusy]);

  const openTab = useCallback((t: number) => {
    setTab(t);
    if (t >= 0) void loadDocs();
  }, [loadDocs]);

  return (
    <div className="fixed inset-0 z-50 bg-black/40 flex items-center justify-center p-4"
         onClick={onClose}>
      <div className="bg-psion-dark border border-psion-accent/50 rounded-lg shadow-lg max-w-xl w-full max-h-[85vh] overflow-auto"
           onClick={e => e.stopPropagation()}>
        <div className="sticky top-0 bg-psion-mid border-b border-psion-accent/40">
          <div className="flex items-center justify-between px-4 py-2.5">
            <span className="text-sm font-mono font-semibold text-psion-charcoal">{app.name}</span>
            <button onClick={onClose}
                    className="text-xs font-mono text-gray-400 hover:text-amber-700 transition-colors cursor-pointer">
              Close
            </button>
          </div>
          {app.readmes.length > 0 && (
            <div className="flex flex-wrap gap-1 px-3 pb-1.5" role="tablist" aria-label="App details">
              <button
                role="tab"
                aria-selected={tab === -1}
                onClick={() => openTab(-1)}
                className={`text-[11px] font-mono px-2.5 py-1 rounded-t border-b-2 transition-colors cursor-pointer ${
                  tab === -1
                    ? 'border-psion-accent text-psion-charcoal font-semibold'
                    : 'border-transparent text-gray-500 hover:text-psion-charcoal'}`}
              >
                Details
              </button>
              {app.readmes.map((rel, i) => (
                <button
                  key={rel}
                  role="tab"
                  aria-selected={tab === i}
                  onClick={() => openTab(i)}
                  title={rel}
                  className={`text-[11px] font-mono px-2.5 py-1 rounded-t border-b-2 transition-colors cursor-pointer ${
                    tab === i
                      ? 'border-psion-accent text-psion-charcoal font-semibold'
                      : 'border-transparent text-gray-500 hover:text-psion-charcoal'}`}
                >
                  {baseName(rel)}
                </button>
              ))}
            </div>
          )}
        </div>

        {tab === -1 ? (
          <div className="p-4 space-y-3">
            <div className="flex items-start gap-3">
              <AppIcon app={app} />
              <div className="text-[11px] font-mono text-gray-600 space-y-0.5">
                <div>{categoryLabel}{app.section ? ` — ${app.section}` : ''}</div>
                <div>
                  {app.date ? `Catalogued ${app.date} · ` : ''}{formatSize(app.sizeBytes)} ·{' '}
                  {app.fileCount} file{app.fileCount === 1 ? '' : 's'}
                  {app.vault ? ' · vault' : ''}
                </div>
                <div>
                  Runs on: {app.devices.length > 0
                    ? app.devices.map(d => deviceNames[d] ?? d).join(', ')
                    : 'a device this emulator doesn’t cover yet'}
                </div>
              </div>
            </div>

            <p className="text-xs font-mono text-psion-charcoal leading-relaxed">{app.description}</p>

            <div className="flex flex-wrap gap-2 items-center pt-1">
              {targets.length > 0 && (
                <>
                  <button
                    onClick={() => tryDevice && onTry(app, tryDevice)}
                    disabled={!tryDevice}
                    className="text-xs font-mono px-3 py-1.5 rounded bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors cursor-pointer disabled:opacity-50 disabled:cursor-not-allowed"
                  >
                    ▸ Try it on
                  </button>
                  <select
                    value={tryDevice}
                    onChange={e => setTryDevice(e.target.value)}
                    className="text-xs font-mono px-2 py-1.5 rounded border border-psion-accent/50 bg-white text-psion-charcoal"
                    aria-label="Device to try this app on"
                  >
                    {targets.map(d => (
                      <option key={d} value={d} disabled={!deviceEnabled(d)}>
                        {(deviceNames[d] ?? d) + (deviceEnabled(d) ? '' : ' — boot it once first')}
                      </option>
                    ))}
                  </select>
                </>
              )}
              <button
                onClick={() => onDownload(app)}
                className="text-xs font-mono px-3 py-1.5 rounded border border-psion-accent/50 text-gray-600 hover:bg-psion-accent hover:text-white transition-colors cursor-pointer"
              >
                Download zip
              </button>
            </div>

            {targets.length === 0 && app.installKind !== 'none' && (
              <p className="text-[11px] font-mono text-gray-500">
                This app can't be auto-installed on an emulated device — download
                it and transfer the files yourself (Remote Link or CF card).
              </p>
            )}
            {app.installKind === 'none' && (
              <p className="text-[11px] font-mono text-gray-500">
                No one-click installer in this bundle — download it and copy the
                files to a device manually (the readme usually explains where).
              </p>
            )}
          </div>
        ) : (
          <div className="p-4">
            {docsBusy && (
              <p className="text-xs font-mono text-gray-400 py-6 text-center">Loading…</p>
            )}
            {docsError && (
              <p className="text-xs font-mono text-amber-700 py-6 text-center">
                Could not load the documents: {docsError}
              </p>
            )}
            {docs && (
              <pre className="text-[11px] font-mono bg-white text-psion-charcoal p-3 rounded shadow-inner whitespace-pre-wrap max-h-[60vh] overflow-auto">
                {docs[app.readmes[tab]]}
              </pre>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
