// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Per-device usage analytics. Fail-silent end-to-end: every network call is
// guarded by navigator.onLine and wrapped in .catch(()=>{}), so a missing
// backend, a flaky connection, or a CSP block leaves the console clean and
// the app unchanged.
//
// Three categories of event are emitted to /psion/api/track.php:
//   - 'load'                       once per successful device boot
//   - 'session'                    30 s heartbeats + a final beacon on
//                                  page-hide / device-switch carrying the
//                                  accumulated foreground time
//   - 'cf_attach' | 'speaker_on' | 'mic_on' | 'printer_capture'
//                                  one-shot feature events
//
// A persistent clientId (UUID v4 in localStorage) and per-session sessionId
// give the server enough information to dedupe heartbeats and count unique
// visitors without authenticating anyone.

const CLIENT_ID_KEY = 'psion.clientId';
const HEARTBEAT_INTERVAL_MS = 30_000;
const API_BASE = `${import.meta.env.BASE_URL}api/`;

export type FeatureEvent = 'cf_attach' | 'cf_update_in_place' | 'speaker_on' | 'mic_on' | 'printer_capture' | 'printer_via_pc';

export interface LeaderboardRow {
  deviceId: string;
  loadCount: number;
  totalTimeMs: number;
  uniqueClients: number;
}

interface SessionState {
  sessionId: string;
  deviceId: string;
  accumulatedMs: number;
  segmentStart: number | null;
  heartbeatHandle: ReturnType<typeof setInterval> | null;
}

let currentSession: SessionState | null = null;

function uuidv4(): string {
  // Prefer the platform implementation when available (HTTPS / modern
  // browsers); fall back to a Math.random() RFC4122 v4 generator for
  // older / file:// contexts where crypto.randomUUID is undefined.
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return crypto.randomUUID();
  }
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, c => {
    const r = (Math.random() * 16) | 0;
    const v = c === 'x' ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}

function getClientId(): string {
  try {
    const existing = localStorage.getItem(CLIENT_ID_KEY);
    if (existing && /^[0-9a-f-]{36}$/i.test(existing)) return existing;
    const fresh = uuidv4();
    localStorage.setItem(CLIENT_ID_KEY, fresh);
    return fresh;
  } catch {
    return uuidv4();
  }
}

function canSend(): boolean {
  return typeof navigator !== 'undefined' && navigator.onLine !== false;
}

interface TrackPayload {
  clientId: string;
  deviceId: string;
  event: 'load' | 'session' | FeatureEvent;
  sessionId?: string;
  durationMs?: number;
}

function post(payload: TrackPayload): void {
  if (!canSend()) return;
  try {
    fetch(`${API_BASE}track.php`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
      keepalive: true,
    }).catch(() => {});
  } catch {
    // ignore
  }
}

function beacon(payload: TrackPayload): void {
  if (!canSend()) return;
  try {
    const body = new Blob([JSON.stringify(payload)], { type: 'application/json' });
    if (typeof navigator.sendBeacon === 'function') {
      navigator.sendBeacon(`${API_BASE}track.php`, body);
      return;
    }
    post(payload);
  } catch {
    // ignore
  }
}

function currentDurationMs(s: SessionState): number {
  const live = s.segmentStart !== null ? Date.now() - s.segmentStart : 0;
  return Math.max(0, s.accumulatedMs + live);
}

function pauseSegment(s: SessionState): void {
  if (s.segmentStart !== null) {
    s.accumulatedMs += Date.now() - s.segmentStart;
    s.segmentStart = null;
  }
}

function resumeSegment(s: SessionState): void {
  if (s.segmentStart === null) {
    s.segmentStart = Date.now();
  }
}

function sendHeartbeat(s: SessionState, viaBeacon = false): void {
  const payload: TrackPayload = {
    clientId: getClientId(),
    deviceId: s.deviceId,
    event: 'session',
    sessionId: s.sessionId,
    durationMs: currentDurationMs(s),
  };
  if (viaBeacon) beacon(payload);
  else post(payload);
}

export function trackDeviceLoad(deviceId: string): void {
  if (!deviceId) return;
  post({
    clientId: getClientId(),
    deviceId,
    event: 'load',
  });
}

export function trackFeature(deviceId: string | null | undefined, feature: FeatureEvent): void {
  if (!deviceId) return;
  post({
    clientId: getClientId(),
    deviceId,
    event: feature,
  });
}

// ── App-library events ──────────────────────────────────────────────
// Same fail-silent posture as the device events; app events carry an
// appId (manifest slug, e.g. "epocgames/atomic14") instead of a
// deviceId and land in the separate app_events table.

export type AppEventKind = 'app_try' | 'app_download';

export function trackAppEvent(appId: string, event: AppEventKind): void {
  if (!appId || !canSend()) return;
  try {
    fetch(`${API_BASE}track.php`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ clientId: getClientId(), appId, event }),
      keepalive: true,
    }).catch(() => {});
  } catch {
    // ignore
  }
}

export interface AppLeaderboardRow {
  appId: string;
  tryCount: number;
  downloadCount: number;
  uniqueClients: number;
}

export async function fetchAppLeaderboard(): Promise<AppLeaderboardRow[]> {
  if (!canSend()) return [];
  try {
    const resp = await fetch(`${API_BASE}leaderboard.php?view=apps`);
    if (!resp.ok) return [];
    const rows = await resp.json();
    return Array.isArray(rows) ? rows as AppLeaderboardRow[] : [];
  } catch {
    return [];
  }
}

export function startSession(deviceId: string): void {
  if (!deviceId) return;
  // Flush any prior session before swapping. Don't recurse through endSession's
  // 'switch' beacon path here because we're inside the new device's user
  // gesture and want minimal latency; sendHeartbeat is fire-and-forget.
  if (currentSession) {
    pauseSegment(currentSession);
    sendHeartbeat(currentSession);
    if (currentSession.heartbeatHandle) clearInterval(currentSession.heartbeatHandle);
  }
  const startedHidden = typeof document !== 'undefined' && document.visibilityState === 'hidden';
  const s: SessionState = {
    sessionId: uuidv4(),
    deviceId,
    accumulatedMs: 0,
    segmentStart: startedHidden ? null : Date.now(),
    heartbeatHandle: null,
  };
  s.heartbeatHandle = setInterval(() => sendHeartbeat(s), HEARTBEAT_INTERVAL_MS);
  currentSession = s;
}

export function endSession(reason: 'switch' | 'unload' = 'switch'): void {
  const s = currentSession;
  if (!s) return;
  pauseSegment(s);
  if (s.heartbeatHandle) {
    clearInterval(s.heartbeatHandle);
    s.heartbeatHandle = null;
  }
  // Unload path must use sendBeacon; tabs are about to die.
  sendHeartbeat(s, reason === 'unload');
  currentSession = null;
}

export async function fetchLeaderboard(): Promise<LeaderboardRow[]> {
  if (!canSend()) return [];
  try {
    const resp = await fetch(`${API_BASE}leaderboard.php`, {
      method: 'GET',
      headers: { Accept: 'application/json' },
    });
    if (!resp.ok) return [];
    const data = await resp.json();
    if (!Array.isArray(data)) return [];
    return data
      .filter((r): r is LeaderboardRow =>
        r && typeof r.deviceId === 'string'
          && Number.isFinite(Number(r.loadCount))
          && Number.isFinite(Number(r.totalTimeMs))
          && Number.isFinite(Number(r.uniqueClients)))
      .map(r => ({
        deviceId: r.deviceId,
        loadCount: Number(r.loadCount),
        totalTimeMs: Number(r.totalTimeMs),
        uniqueClients: Number(r.uniqueClients),
      }));
  } catch {
    return [];
  }
}

// ── Page lifecycle wiring (installed once at module import) ─────────────────
// visibility:hidden pauses the session timer so a tab left open overnight
// doesn't inflate totalTimeMs. pagehide/beforeunload guarantee a final
// beacon — sendBeacon is the only reliable way to deliver an HTTP request
// during a navigation away.
if (typeof document !== 'undefined') {
  document.addEventListener('visibilitychange', () => {
    const s = currentSession;
    if (!s) return;
    if (document.visibilityState === 'hidden') {
      pauseSegment(s);
      sendHeartbeat(s, true);
    } else if (document.visibilityState === 'visible') {
      resumeSegment(s);
    }
  });
}

if (typeof window !== 'undefined') {
  const flush = () => {
    const s = currentSession;
    if (!s) return;
    pauseSegment(s);
    sendHeartbeat(s, true);
  };
  window.addEventListener('pagehide', flush);
  window.addEventListener('beforeunload', flush);
}
