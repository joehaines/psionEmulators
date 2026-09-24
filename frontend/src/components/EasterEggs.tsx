// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useState, type ReactNode } from 'react';
import psionLogoUrl from '../assets/psion-logo.svg';
import { EASTER_EGGS, type EasterEgg, type EggKind } from '../lib/easterEggs';

// The Easter eggs page (#/eggs): every hidden credit screen, cheat and
// leftover test program found in the ROMs, with how to set each one off.
// Static, like the leaderboard route — it needs no WASM, only the device
// display names, which it fetches the same way the leaderboard does.

interface Props {
  onClose(): void;
}

const KIND_LABEL: Record<EggKind, string> = {
  credits: 'Hidden credits',
  cheat:   'Cheat',
  hidden:  'Hidden in ROM',
};

const SECTIONS: { kind: EggKind; title: string; blurb: string }[] = [
  { kind: 'credits', title: 'Hidden credits',
    blurb: 'Screens the developers left for themselves, reachable on the running machine.' },
  { kind: 'cheat', title: 'Cheats',
    blurb: 'Secret modes in the built-in games.' },
  { kind: 'hidden', title: 'Hidden in the ROM',
    blurb: 'Test and factory programs that ship in the ROM but that the emulator can’t start yet.' },
];

export default function EasterEggs({ onClose }: Props) {
  const [names, setNames] = useState<Record<string, string>>({});

  useEffect(() => {
    let cancelled = false;
    fetch(`${import.meta.env.BASE_URL}device-names.json`)
      .then(r => (r.ok ? r.json() : {}))
      .catch(() => ({}))
      .then(m => { if (!cancelled) setNames(m ?? {}); });
    return () => { cancelled = true; };
  }, []);

  const deviceName = (id: string) => names[id] ?? id;

  return (
    <div className="min-h-screen bg-psion-dark font-mono flex flex-col">
      <header className="border-b border-psion-accent/40 px-4 py-2 flex items-center gap-3 flex-shrink-0 bg-psion-dark shadow-sm">
        <img src={psionLogoUrl} alt="Psion" className="h-8 w-auto select-none" draggable={false} />
        <span className="text-xs text-gray-400 ml-1 border-l border-psion-accent/30 pl-3">
          Easter eggs
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
        <p className="text-sm text-psion-charcoal mb-2">
          Easter eggs, cheats and leftovers found by going through every ROM
          this emulator runs. Each one lists the machines you can try it on;
          pick one from the menu, and the same hint shows under its controls.
        </p>

        {SECTIONS.map(section => {
          const eggs = EASTER_EGGS.filter(e => e.kind === section.kind);
          if (eggs.length === 0) return null;
          return (
            <section key={section.kind} className="mt-8">
              <h2 className="text-xs font-semibold uppercase tracking-wide text-psion-accent border-b border-psion-accent/20 pb-2 mb-1">
                {section.title}
              </h2>
              <p className="text-xs text-gray-500 mb-4">{section.blurb}</p>
              <div className="space-y-4">
                {eggs.map(egg => (
                  <EggCard key={egg.id} egg={egg} deviceName={deviceName} />
                ))}
              </div>
            </section>
          );
        })}
      </main>
    </div>
  );
}

function EggCard({ egg, deviceName }: { egg: EasterEgg; deviceName(id: string): string }) {
  return (
    <article className="border border-psion-accent/30 rounded bg-psion-mid/40 p-4">
      <div className="flex flex-wrap items-baseline gap-2 mb-2">
        <h3 className="text-sm font-semibold text-psion-charcoal">{egg.title}</h3>
        <span className="text-[10px] font-semibold uppercase tracking-wide text-psion-charcoal bg-psion-highlight rounded px-1.5 py-0.5">
          {KIND_LABEL[egg.kind]}
        </span>
        {!egg.verified && (
          <span className="text-[10px] uppercase tracking-wide text-gray-500 border border-psion-accent/30 rounded px-1.5 py-0.5">
            Read from the ROM
          </span>
        )}
      </div>

      {egg.devices.length > 0 && (
        <p className="text-xs text-gray-500 mb-2">
          Try it on:{' '}
          {egg.devices.map((id, i) => (
            <span key={id}>
              {i > 0 && ', '}
              <a
                href={`#/${id}`}
                className="text-psion-charcoal underline decoration-psion-highlight decoration-2 underline-offset-2 hover:text-psion-highlight"
              >
                {deviceName(id)}
              </a>
            </span>
          ))}
        </p>
      )}

      <ol className={[
        'text-xs text-psion-charcoal space-y-1 mb-2',
        egg.kind === 'hidden' ? 'list-disc pl-5' : 'list-decimal pl-5',
      ].join(' ')}>
        {egg.steps.map((step, i) => <li key={i}>{withCode(step)}</li>)}
      </ol>

      <p className="text-xs text-gray-500">{egg.about}</p>

      {egg.dormantOn && (
        <p className="text-xs text-gray-500 mt-2">
          <span className="text-psion-charcoal">
            {egg.devices.length > 0 ? 'Also in the ROM of ' : 'In the ROM of '}
            {egg.dormantOn.devices.map(deviceName).join(', ')}
          </span>
          {' — '}{egg.dormantOn.why}
        </p>
      )}
    </article>
  );
}

// `Backticked` spans in a step are what to type or press — shown as code and
// kept on one line, so "!Mrs T Bogan!" can't wrap into something ambiguous.
function withCode(text: string): ReactNode[] {
  return text.split('`').map((part, i) => (i % 2 === 1
    ? <code key={i} className="whitespace-nowrap bg-psion-highlight/40 text-psion-charcoal rounded px-1">{part}</code>
    : <span key={i}>{part}</span>));
}
