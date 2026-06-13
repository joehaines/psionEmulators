// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { IrLap } from '../irda/irlap.ts';
import { encodeSirFrame } from '../irda/framing.ts';
const frames: Uint8Array[] = [];
const lap = new IrLap((body) => frames.push(encodeSirFrame(body)));
// monkeypatch srcAddr deterministically by reflection
(lap as any).srcAddr = 0x12345678;
lap.discover(50);
const hex = (u: Uint8Array) => Array.from(u).map(b=>b.toString(16).padStart(2,'0')).join(' ');
for (const f of frames) {
  console.log('SIR:', hex(f));
}
// also dump raw bodies pre-framing
