// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useRef, useState } from 'react';

interface Props {
  message: string;
  // Secondary phase line under the bar, e.g. "Downloading ROM… 2.1 of 8.4 MB".
  detail?: string | null;
  skinFilename?: string;
  skinFolder?: string;
  // 0..1 → determinate bar; null/undefined → indeterminate sweep.
  progress?: number | null;
}

// How long real progress may sit still before we surface the slow-connection
// hint. Generous enough that a normal mobile load never sees it.
const STALL_MS = 8000;

/**
 * Eases the displayed fraction toward the latest reported target so coarse
 * checkpoint updates (0.1 → 0.6 → 0.8…) render as continuous motion rather
 * than jumps. Between real updates the bar trickles slightly ahead (bounded
 * to +4% and never past 99%) so it never looks frozen, and `stalled` flips
 * true once no real progress has arrived for STALL_MS.
 */
function useSmoothProgress(target: number | null | undefined): { value: number; stalled: boolean } {
  const determinate = typeof target === 'number';
  const [shown, setShown] = useState(0);
  const [stalled, setStalled] = useState(false);
  const sRef = useRef({ shown: 0, target: 0, lastChange: performance.now() });

  // Record real target changes; every one resets the stall timer. A large
  // backwards jump is a genuine restart (e.g. a failed state-restore falling
  // through to cold boot), so snap down instead of refusing to move back.
  useEffect(() => {
    const st = sRef.current;
    st.lastChange = performance.now();
    setStalled(false);
    if (typeof target !== 'number') return;
    const t = Math.min(1, Math.max(0, target));
    if (t < st.target - 0.2) { st.shown = t; setShown(t); }
    st.target = t;
  }, [target]);

  useEffect(() => {
    if (!determinate) return;   // indeterminate: the CSS sweep provides motion
    let raf = 0;
    let last = performance.now();
    const tick = (now: number) => {
      const st = sRef.current;
      const dt = Math.min(0.1, (now - last) / 1000);
      last = now;
      let next = st.shown;
      if (next < st.target) {
        // Exponential approach with a linear floor so the bar both feels
        // responsive on big jumps and actually reaches the target.
        next = Math.min(st.target, next + Math.max((st.target - next) * 4, 0.08) * dt);
      } else {
        const cap = st.target >= 1 ? 1 : Math.min(0.99, st.target + 0.04);
        if (next < cap) next = Math.min(cap, next + (cap - next) * 0.15 * dt);
      }
      if (next !== st.shown) { st.shown = next; setShown(next); }
      // React bails out when the value is unchanged, so calling every frame is fine.
      setStalled(now - st.lastChange > STALL_MS && st.target < 1);
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [determinate]);

  return { value: shown, stalled };
}

function ProgressBar({ value, label, onDark }: { value: number | null; label: string; onDark: boolean }) {
  return (
    <div
      role="progressbar"
      aria-label={label}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={value != null ? Math.round(value * 100) : undefined}
      className={`relative h-2 w-64 max-w-[75vw] overflow-hidden rounded-full ${onDark ? 'bg-white/25' : 'bg-psion-charcoal/10'}`}
    >
      {value != null ? (
        <div
          className="h-full rounded-full bg-psion-highlight"
          style={{ width: `${(Math.min(1, Math.max(0, value)) * 100).toFixed(2)}%` }}
        />
      ) : (
        <div className="absolute inset-y-0 w-1/3 rounded-full bg-psion-highlight animate-progress-sweep motion-reduce:animate-pulse motion-reduce:w-full" />
      )}
    </div>
  );
}

export default function LoadingOverlay({ message, detail, skinFilename, skinFolder = 'skins', progress }: Props) {
  const { value, stalled } = useSmoothProgress(progress);
  const determinate = typeof progress === 'number';
  const barValue = determinate ? value : null;

  const content = (onDark: boolean) => (
    <div className="flex flex-col items-center gap-3 px-4 text-center" aria-busy="true">
      <p className={`font-mono text-sm ${onDark ? 'text-white drop-shadow' : 'text-psion-charcoal/80'}`}>
        {message}
        {determinate && (
          // Fixed width so the title doesn't jitter as the digits change.
          <span className="ml-2 inline-block w-[4ch] text-left tabular-nums">{Math.round(value * 100)}%</span>
        )}
      </p>
      <ProgressBar value={barValue} label={message} onDark={onDark} />
      <div className="min-h-[2rem]" aria-live="polite">
        {detail && (
          <p className={`font-mono text-xs ${onDark ? 'text-white/70' : 'text-gray-500'}`}>{detail}</p>
        )}
        {stalled && (
          <p className={`font-mono text-[11px] mt-1 ${onDark ? 'text-white/50' : 'text-gray-400'}`}>
            Still working — this can take a moment on slow connections.
          </p>
        )}
      </div>
    </div>
  );

  if (skinFilename) {
    return (
      <div className="flex items-start justify-center py-6 px-4">
        <div className="relative w-full" style={{ maxWidth: 'min(95vw, 900px)' }}>
          <img
            src={`${import.meta.env.BASE_URL}${skinFolder}/${skinFilename}`}
            alt="Device skin"
            className="w-full h-auto block rounded select-none pointer-events-none"
            draggable={false}
          />
          {/* Dark scrim: psion-dark is WHITE, so the old bg-psion-dark/60 put
              white text on a white wash — unreadable over light skin photos. */}
          <div className="absolute inset-0 flex flex-col items-center justify-center rounded bg-psion-charcoal/70 backdrop-blur-[2px]">
            {content(true)}
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="flex flex-col items-center justify-center py-20">
      {content(false)}
    </div>
  );
}
