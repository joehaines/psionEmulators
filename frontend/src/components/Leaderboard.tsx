// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useState } from 'react';
import { fetchLeaderboard, fetchAppLeaderboard, type LeaderboardRow, type AppLeaderboardRow } from '../lib/analytics';
import { fetchAppManifest } from '../lib/appLibrary';
import psionLogoUrl from '../assets/psion-logo.svg';

interface Props {
  onClose(): void;
}

type Mode = 'loads' | 'time';

function formatDuration(ms: number): string {
  if (!ms || ms < 1000) return '—';
  const totalSeconds = Math.floor(ms / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  if (hours > 0) return `${hours}h ${minutes}m`;
  if (minutes > 0) return `${minutes}m`;
  return `${totalSeconds}s`;
}

interface BarChartProps {
  rows: LeaderboardRow[];
  names: Record<string, string>;
  mode: Mode;
}

function BarChart({ rows, names, mode }: BarChartProps) {
  // Horizontal bar chart, one row per device. Width is value / max for the
  // active metric; bars are drawn as plain SVG <rect>s so we don't pull in
  // a chart library.
  const ROW_HEIGHT = 24;
  const LABEL_WIDTH = 140;
  const VALUE_WIDTH = 60;
  const CHART_WIDTH = 640;
  const PADDING_X = 12;

  const max = rows.reduce((m, r) =>
    Math.max(m, mode === 'loads' ? r.loadCount : r.totalTimeMs), 0);
  const totalHeight = rows.length * ROW_HEIGHT + 8;
  const barAreaWidth = CHART_WIDTH - LABEL_WIDTH - VALUE_WIDTH - 2 * PADDING_X;

  return (
    <svg
      viewBox={`0 0 ${CHART_WIDTH} ${totalHeight}`}
      preserveAspectRatio="xMidYMid meet"
      className="w-full h-auto"
      role="img"
      aria-label={`Devices by ${mode === 'loads' ? 'load count' : 'time used'}`}
    >
      {rows.map((row, i) => {
        const value = mode === 'loads' ? row.loadCount : row.totalTimeMs;
        const barWidth = max > 0 ? (value / max) * barAreaWidth : 0;
        const y = i * ROW_HEIGHT + 4;
        const displayName = names[row.deviceId] ?? row.deviceId;
        const valueLabel = mode === 'loads'
          ? String(row.loadCount)
          : formatDuration(row.totalTimeMs);
        const trackX = PADDING_X + LABEL_WIDTH;
        return (
          <g key={row.deviceId}>
            <title>
              {`${displayName} — ${row.loadCount} loads, ${formatDuration(row.totalTimeMs)}, ${row.uniqueClients} users`}
            </title>
            <text
              x={PADDING_X + LABEL_WIDTH - 8}
              y={y + ROW_HEIGHT / 2}
              textAnchor="end"
              dominantBaseline="middle"
              fontSize="11"
              fontFamily="JetBrains Mono, monospace"
              className="fill-psion-charcoal"
            >
              {displayName}
            </text>
            <rect
              x={trackX}
              y={y + 4}
              width={barAreaWidth}
              height={ROW_HEIGHT - 8}
              className="fill-psion-mid"
            />
            <rect
              x={trackX}
              y={y + 4}
              width={barWidth}
              height={ROW_HEIGHT - 8}
              className="fill-psion-highlight"
            />
            <text
              x={trackX + barAreaWidth + 8}
              y={y + ROW_HEIGHT / 2}
              dominantBaseline="middle"
              fontSize="11"
              fontFamily="JetBrains Mono, monospace"
              className="fill-psion-charcoal"
            >
              {valueLabel}
            </text>
          </g>
        );
      })}
    </svg>
  );
}

export default function Leaderboard({ onClose }: Props) {
  const [rows, setRows] = useState<LeaderboardRow[] | null>(null);
  const [names, setNames] = useState<Record<string, string>>({});
  const [mode, setMode] = useState<Mode>('loads');
  // Devices is the default view; Apps shows the app-library try /
  // download counts (lazy-loaded on first switch).
  const [view, setView] = useState<'devices' | 'apps'>('devices');
  const [appRows, setAppRows] = useState<AppLeaderboardRow[] | null>(null);
  const [appNames, setAppNames] = useState<Record<string, string>>({});

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const [data, nameMap] = await Promise.all([
        fetchLeaderboard(),
        fetch(`${import.meta.env.BASE_URL}device-names.json`)
          .then(r => (r.ok ? r.json() : {}))
          .catch(() => ({})),
      ]);
      if (cancelled) return;
      setRows(data);
      setNames(nameMap ?? {});
    })();
    return () => { cancelled = true; };
  }, []);

  useEffect(() => {
    if (view !== 'apps' || appRows !== null) return;
    let cancelled = false;
    (async () => {
      const [data, manifest] = await Promise.all([
        fetchAppLeaderboard(),
        fetchAppManifest().catch(() => null),
      ]);
      if (cancelled) return;
      setAppRows(data);
      if (manifest) {
        const m: Record<string, string> = {};
        for (const a of manifest.apps) m[a.id] = a.name;
        setAppNames(m);
      }
    })();
    return () => { cancelled = true; };
  }, [view, appRows]);

  const sorted = (rows ?? []).slice().sort((a, b) => {
    if (mode === 'loads') return b.loadCount - a.loadCount;
    return b.totalTimeMs - a.totalTimeMs;
  });

  const btn = 'text-xs font-mono px-3 py-1 rounded border border-psion-accent text-gray-600 hover:bg-psion-accent hover:text-white transition-colors';
  const btnActive = 'text-xs font-mono px-3 py-1 rounded border border-psion-accent bg-psion-highlight text-psion-charcoal';

  return (
    <div className="min-h-screen bg-psion-dark font-mono flex flex-col">
      <header className="border-b border-psion-accent/40 px-4 py-2 flex items-center gap-3 flex-shrink-0 bg-psion-dark shadow-sm">
        <img src={psionLogoUrl} alt="Psion" className="h-8 w-auto select-none" draggable={false} />
        <span className="text-xs text-gray-400 ml-1 border-l border-psion-accent/30 pl-3">
          Usage leaderboard
        </span>
        <div className="flex-1" />
        <button
          onClick={onClose}
          className="text-xs font-mono text-gray-500 hover:text-psion-charcoal transition-colors"
          aria-label="Back to emulator"
        >
          ← Back
        </button>
      </header>

      <main className="flex-1 px-4 py-6 max-w-3xl w-full mx-auto">
        <div className="flex items-center gap-2 mb-5">
          <button onClick={() => setView('devices')}
                  className={view === 'devices' ? btnActive : btn}>
            Devices
          </button>
          <button onClick={() => setView('apps')}
                  className={view === 'apps' ? btnActive : btn}>
            Apps
          </button>
        </div>

        {view === 'apps' && (
          <AppsView rows={appRows} names={appNames} />
        )}

        {view === 'devices' && (<>
        <div className="flex items-center gap-2 mb-4">
          <span className="text-xs font-mono text-gray-500 mr-2">Rank by:</span>
          <button
            onClick={() => setMode('loads')}
            className={mode === 'loads' ? btnActive : btn}
          >
            Loads
          </button>
          <button
            onClick={() => setMode('time')}
            className={mode === 'time' ? btnActive : btn}
          >
            Time used
          </button>
        </div>

        {rows === null && (
          <p className="text-xs font-mono text-gray-400 py-8 text-center">Loading…</p>
        )}

        {rows !== null && sorted.length === 0 && (
          <p className="text-xs font-mono text-gray-400 py-8 text-center">
            No usage data yet.
          </p>
        )}

        {sorted.length > 0 && (
          <>
            <div className="border border-psion-accent/30 rounded p-3 mb-4 bg-psion-dark">
              <p className="text-xs font-mono text-gray-500 uppercase tracking-wide mb-2">
                All devices by {mode === 'loads' ? 'loads' : 'time used'}
              </p>
              <BarChart rows={sorted} names={names} mode={mode} />
            </div>

            <div className="border border-psion-accent/30 rounded overflow-hidden">
              <div className="grid grid-cols-[2.5rem_1fr_5rem_5rem_4rem] gap-2 px-3 py-2 bg-psion-mid border-b border-psion-accent/30 text-xs font-mono text-gray-500 uppercase tracking-wide">
                <span>#</span>
                <span>Device</span>
                <span className="text-right">Loads</span>
                <span className="text-right">Time</span>
                <span className="text-right">Users</span>
              </div>
              {sorted.map((row, i) => {
                const displayName = names[row.deviceId] ?? row.deviceId;
                return (
                  <div
                    key={row.deviceId}
                    className={[
                      'grid grid-cols-[2.5rem_1fr_5rem_5rem_4rem] gap-2 px-3 py-2 text-xs font-mono border-b border-psion-accent/20 last:border-b-0',
                      i % 2 === 0 ? 'bg-psion-dark' : 'bg-psion-mid/40',
                    ].join(' ')}
                  >
                    <span className="text-gray-400">{i + 1}</span>
                    <span className="text-psion-charcoal truncate">{displayName}</span>
                    <span className={[
                      'text-right',
                      mode === 'loads' ? 'text-psion-charcoal font-semibold' : 'text-gray-500',
                    ].join(' ')}>
                      {row.loadCount}
                    </span>
                    <span className={[
                      'text-right',
                      mode === 'time' ? 'text-psion-charcoal font-semibold' : 'text-gray-500',
                    ].join(' ')}>
                      {formatDuration(row.totalTimeMs)}
                    </span>
                    <span className="text-right text-gray-500">{row.uniqueClients}</span>
                  </div>
                );
              })}
            </div>
          </>
        )}
        </>)}

        <p className="text-xs font-mono text-gray-400 mt-6 text-center">
          Anonymous usage stats — clear browser storage to opt out.
        </p>
      </main>
    </div>
  );
}

// App-library popularity table: tries, downloads, unique users per app.
function AppsView({ rows, names }: {
  rows: AppLeaderboardRow[] | null;
  names: Record<string, string>;
}) {
  if (rows === null) {
    return <p className="text-xs font-mono text-gray-400 py-8 text-center">Loading…</p>;
  }
  if (rows.length === 0) {
    return (
      <p className="text-xs font-mono text-gray-400 py-8 text-center">
        No app activity yet — be the first: <a href="#/apps" className="underline">browse the app library</a>.
      </p>
    );
  }
  return (
    <div className="border border-psion-accent/30 rounded overflow-hidden">
      <div className="grid grid-cols-[2.5rem_1fr_4.5rem_6rem_4rem] gap-2 px-3 py-2 bg-psion-mid border-b border-psion-accent/30 text-xs font-mono text-gray-500 uppercase tracking-wide">
        <span>#</span>
        <span>App</span>
        <span className="text-right">Tries</span>
        <span className="text-right">Downloads</span>
        <span className="text-right">Users</span>
      </div>
      {rows.map((row, i) => (
        <div
          key={row.appId}
          className={[
            'grid grid-cols-[2.5rem_1fr_4.5rem_6rem_4rem] gap-2 px-3 py-2 text-xs font-mono border-b border-psion-accent/20 last:border-b-0',
            i % 2 === 0 ? 'bg-psion-dark' : 'bg-psion-mid/40',
          ].join(' ')}
        >
          <span className="text-gray-400">{i + 1}</span>
          <span className="text-psion-charcoal truncate" title={row.appId}>
            {names[row.appId] ?? row.appId}
          </span>
          <span className="text-right text-psion-charcoal font-semibold">{row.tryCount}</span>
          <span className="text-right text-gray-500">{row.downloadCount}</span>
          <span className="text-right text-gray-500">{row.uniqueClients}</span>
        </div>
      ))}
    </div>
  );
}
