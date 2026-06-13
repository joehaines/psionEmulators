// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useMemo, type ReactNode } from 'react';
import psionLogoUrl from '../assets/psion-logo.svg';

// Landing page shown in the main area when no device is loaded (the
// "blank screen behind the menu on load"), and again whenever the user
// clicks the Psion logo in the header. Pure static content — it needs no
// WASM, so it can paint immediately. The device picker still lives in the
// side menu; the CTA here just opens it.

// Device-mode screenshots that ship in public/intro/ (see the asset move
// in frontend/public/intro). The scroll bar is ordered dynamically by
// real popularity — total loads from the usage leaderboard (see Home's
// loadsById prop). This array is just the fallback order shown before the
// leaderboard arrives (or if it fails): a rough most-iconic-first guess.
const GALLERY: { id: string; name: string; year: string }[] = [
  { id: 'series5',     name: 'Series 5',             year: '1997' },
  { id: 'series3a',    name: 'Series 3a',            year: '1993' },
  { id: '5mx',         name: 'Series 5mx',           year: '1999' },
  { id: 'revo',        name: 'Revo',                 year: '1999' },
  { id: 'series3c',    name: 'Series 3c',            year: '1996' },
  { id: 'organiser2',  name: 'Organiser II',         year: '1986' },
  { id: 'series3',     name: 'Series 3',             year: '1991' },
  { id: 'series3mx',   name: 'Series 3mx',           year: '1998' },
  { id: 'series7',     name: 'Series 7',             year: '2000' },
  { id: 'netbook',     name: 'netBook',              year: '1999' },
  { id: 'siena',       name: 'Siena',                year: '1996' },
  { id: 'mc218',       name: 'Ericsson MC218',       year: '1999' },
  { id: 'pocketbk',    name: 'Acorn Pocket Book',    year: '1992' },
  { id: 'pocketbk2',   name: 'Acorn Pocket Book II', year: '1996' },
  { id: 'osaris',      name: 'Osaris',               year: '1998' },
  { id: '5mxpro',      name: 'Series 5mx Pro',       year: '2000' },
  { id: 'workabout',   name: 'Workabout',            year: '1995' },
  { id: 'workaboutmx', name: 'WorkaboutMX',          year: '2000' },
  { id: 'mc400',       name: 'MC400',                year: '1989' },
];

// Succinct capability list (kept broad on purpose — see home-page copy
// review). Rendered as chips with a small yellow brand tab. Save states,
// the App Library and device skins each get their own callout sentence
// below the chips rather than a pill.
const CAPABILITIES = [
  'CompactFlash',
  'SSD & Datapak packs',
  'Infrared',
  'Speaker',
  'Microphone',
  'Remote Link (PsiWin)',
  'Printing',
];

const FAMILIES: { tag: string; title: string; models: string }[] = [
  { tag: 'EPOC32 · ARM',  title: 'EPOC32 machines',
    models: 'Series 5 · 5mx · 5mx Pro · MC218 · Revo · Osaris · Series 7 · netBook' },
  { tag: 'SIBO · 16-bit', title: 'Series 3 family',
    models: 'Series 3 · 3a · 3c · 3mx · Siena · Workabout · WorkaboutMX · Pocket Book I & II · MC400' },
  { tag: '8-bit',         title: 'Organiser',
    models: 'Organiser II (LZ / LZ64)' },
];

export default function Home({
  baseUrl,
  onChooseDevice,
  loadsById,
}: {
  baseUrl: string;
  // Opens the side device-picker menu (the CTA + "pick a device" hints).
  onChooseDevice: () => void;
  // deviceId -> total loads from the usage leaderboard, used to order the
  // device bar by real popularity. Empty until the leaderboard arrives.
  loadsById: Map<string, number>;
}) {
  const img = (id: string) => `${baseUrl}intro/${id}.jpg`;

  // Most-loaded device first. Array.prototype.sort is stable, so devices
  // with equal (or zero) loads keep GALLERY's fallback order — which is
  // also what shows in full before any leaderboard data lands.
  const devices = useMemo(() => {
    if (loadsById.size === 0) return GALLERY;
    return [...GALLERY].sort(
      (a, b) => (loadsById.get(b.id) ?? 0) - (loadsById.get(a.id) ?? 0),
    );
  }, [loadsById]);

  return (
    <div className="absolute inset-0 overflow-y-auto bg-psion-dark font-mono text-psion-charcoal">
      <div className="max-w-4xl mx-auto px-5 sm:px-6 py-10 sm:py-14">
        {/* Hero */}
        <div className="text-center mb-10">
          <img
            src={psionLogoUrl}
            alt="Psion"
            className="h-14 sm:h-16 w-auto mx-auto mb-6 select-none"
            draggable={false}
          />
          <h1 className="text-xl sm:text-2xl font-semibold mb-2">
            Classic Psion palmtops, natively emulated in your browser
          </h1>
          <p className="text-sm sm:text-base text-psion-accent max-w-2xl mx-auto">
            Every Psion handheld, from the 1986 Organiser&nbsp;II to the
            ARM-powered Series&nbsp;7 — running its real ROM, directly in
            WebAssembly. No installs, no plug-ins.
          </p>
          <p className="text-sm sm:text-base text-psion-accent max-w-2xl mx-auto mt-3">
            Everything runs client-side in your browser — only the
            leaderboards talk to a server. It works on mobile too, runs
            offline, and installs to your home screen as a web app.
          </p>
        </div>

        {/* Device image bar — all of them, horizontally scrollable */}
        <div className="-mx-5 sm:-mx-6 mb-12">
          <div className="flex gap-3 overflow-x-auto px-5 sm:px-6 pb-3 snap-x">
            {devices.map(d => (
              <figure
                key={d.id}
                className="snap-start shrink-0 w-44 sm:w-52 bg-psion-mid border border-psion-accent/15 rounded-lg p-2.5"
              >
                {/* Fixed image area so the row stays level despite the mix
                    of landscape clamshells and tall bricks (Organiser II,
                    Workabout) — portrait shots letterbox onto the card. */}
                <div className="h-40 sm:h-44 flex items-center justify-center bg-psion-dark/40 rounded-md overflow-hidden">
                  <img
                    src={img(d.id)}
                    alt={d.name}
                    loading="lazy"
                    className="max-h-full max-w-full object-contain select-none"
                    draggable={false}
                  />
                </div>
                <figcaption className="flex items-baseline justify-between mt-2 text-xs">
                  <span className="font-semibold truncate">{d.name}</span>
                  <span className="text-psion-accent ml-2">{d.year}</span>
                </figcaption>
              </figure>
            ))}
          </div>
        </div>

        {/* What it does */}
        <Section title="What it does">
          <p className="text-sm sm:text-[0.95rem]">
            Pick a machine from the menu and it boots its original operating
            system — the same EPOC software and built-in apps you'd have used
            on the real device. Across the range you also get full peripheral
            support:
          </p>
          <div className="flex flex-wrap gap-2 mt-4">
            {CAPABILITIES.map(c => (
              <span
                key={c}
                className="inline-flex items-center bg-psion-mid border border-psion-accent/20 rounded-full px-3 py-1.5 text-xs"
              >
                <span className="inline-block w-1.5 h-1.5 bg-psion-highlight rounded-sm mr-2" />
                {c}
              </span>
            ))}
          </div>
          <p className="text-sm sm:text-[0.95rem] mt-4">
            Save states let you freeze a device exactly where you left it, and
            you can export and import those states across any of the devices.
          </p>
          <p className="text-sm sm:text-[0.95rem] mt-3">
            There's also a built-in{' '}
            <a
              href="#/apps"
              className="text-psion-charcoal font-semibold underline decoration-psion-highlight decoration-2 underline-offset-2 hover:text-psion-highlight transition-colors"
            >
              App Library
            </a>{' '}
            — a searchable catalogue of classic Psion programs and games that
            you can install straight onto a running device in a single click.
          </p>
          <p className="text-sm sm:text-[0.95rem] mt-3">
            You can emulate all models with or without the surrounding device
            on display.
          </p>
        </Section>

        {/* Machines supported */}
        <Section title="Machines supported">
          <div className="grid gap-3 sm:grid-cols-3">
            {FAMILIES.map(f => (
              <div
                key={f.tag}
                className="bg-psion-mid border border-psion-accent/15 rounded-lg p-4"
              >
                <span className="inline-block text-[10px] font-semibold uppercase tracking-wide text-psion-charcoal bg-psion-highlight rounded px-1.5 py-0.5 mb-2">
                  {f.tag}
                </span>
                <h3 className="text-sm font-semibold mb-1">{f.title}</h3>
                <p className="text-xs text-psion-accent leading-relaxed">{f.models}</p>
              </div>
            ))}
          </div>
        </Section>

        {/* A short history of Psion (with a couple of inline screenshots) */}
        <Section title="A short history of Psion">
          <figure className="sm:float-right sm:w-48 sm:ml-5 mb-3">
            <img
              src={img('series5')}
              alt="Psion Series 5"
              loading="lazy"
              className="w-full h-auto rounded-lg border border-psion-accent/15 select-none"
              draggable={false}
            />
            <figcaption className="text-[11px] text-psion-accent mt-1 text-center">
              Series&nbsp;5 (1997) — EPOC32 and the sliding keyboard
            </figcaption>
          </figure>
          <p className="text-sm mb-3">
            Psion was founded in London in 1980 by David Potter, starting out
            writing games and software for the Sinclair home computers. In 1984
            it built its own hardware — the <strong>Psion Organiser</strong>,
            the first practical pocket computer — followed by the hugely
            successful 1986 <strong>Organiser&nbsp;II</strong> with its
            Datapaks and OPL programming language.
          </p>

          <figure className="sm:float-left sm:w-48 sm:mr-5 mb-3">
            <img
              src={img('series3c')}
              alt="Psion Series 3c"
              loading="lazy"
              className="w-full h-auto rounded-lg border border-psion-accent/15 select-none"
              draggable={false}
            />
            <figcaption className="text-[11px] text-psion-accent mt-1 text-center">
              Series&nbsp;3c (1996) — the SIBO clamshell line
            </figcaption>
          </figure>
          <p className="text-sm mb-3">
            At the turn of the decade Psion designed the <strong>SIBO</strong>{' '}
            architecture and EPOC operating system, producing the 1991{' '}
            <strong>Series&nbsp;3</strong>: a clamshell palmtop with a real
            keyboard and graphical apps that became the company's defining
            product. The 3a, 3c, 3mx and pocket-sized Siena followed, alongside
            the rugged Workabout and the Acorn-badged Pocket Book for schools.
          </p>
          <p className="text-sm mb-3">
            In 1997 Psion moved to a new 32-bit, ARM-based OS (EPOC32) and the
            acclaimed <strong>Series&nbsp;5</strong>, with its touchscreen and
            sliding keyboard. The 5mx, tiny Revo, Ericsson MC218, Oregon
            Scientific Osaris and the larger Series&nbsp;7 and netBook all built
            on it.
          </p>
          <p className="text-sm">
            That OS proved Psion's most lasting legacy: in 1998 it was spun out
            — with Nokia, Ericsson and Motorola — as <strong>Symbian</strong>,
            which went on to power hundreds of millions of phones. Psion left
            the consumer market in the early 2000s, but its palmtops remain
            icons of mobile computing — and every one above is preserved here,
            running exactly as it did.
          </p>
        </Section>

        {/* Call to action */}
        <div className="clear-both mt-12 text-center bg-psion-mid border border-psion-accent/15 rounded-lg p-6">
          <p className="text-sm mb-3">
            Open the menu to choose a machine, or jump straight in:
          </p>
          <button
            type="button"
            onClick={onChooseDevice}
            className="inline-flex items-center gap-2 bg-psion-highlight text-psion-charcoal font-semibold text-sm rounded px-4 py-2 hover:brightness-95 transition"
          >
            <span aria-hidden="true">☰</span> Pick a device
          </button>
        </div>
      </div>
    </div>
  );
}

function Section({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="mt-10 first:mt-0">
      <h2 className="text-xs font-semibold uppercase tracking-wide text-psion-accent border-b border-psion-accent/20 pb-2 mb-4">
        {title}
      </h2>
      {children}
    </section>
  );
}
