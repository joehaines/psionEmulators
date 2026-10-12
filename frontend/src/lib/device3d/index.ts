// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Which machines have a 3D model, and how to build each.
//
// Kept free of any three.js import at module level so the header can ask
// "does this device have a 3D view?" without pulling the renderer into the
// main bundle — the models themselves load with the lazy Device3DView chunk.

import type { Device3DBuildOptions, Device3DModel } from './types';

type Builder = (opts: Device3DBuildOptions) => Promise<Device3DModel>;

const LOADERS: Record<string, () => Promise<Builder>> = {
  '5mx': () => import('./psion5mx').then(m => m.buildPsion5mx),
};

export function has3DModel(deviceId: string | null | undefined): boolean {
  return !!deviceId && deviceId in LOADERS;
}

export async function load3DModel(deviceId: string): Promise<Builder | null> {
  const loader = LOADERS[deviceId];
  return loader ? loader() : null;
}

export type { Device3DModel, PhotoScreenLayout } from './types';
