// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useState } from 'react';
import type { DeviceProfile } from '../types/emulator';
import { loadPsionModule } from '../lib/wasmBridge';
import DeviceCard from './DeviceCard';

interface Props {
  onSelect: (romUrl: string) => void;
}

export default function DeviceSelector({ onSelect }: Props) {
  const [profiles, setProfiles] = useState<DeviceProfile[]>([]);

  useEffect(() => {
    // Pull the device list from the WASM module (single source of truth)
    loadPsionModule().then(mod => {
      const json = mod.getAllDeviceProfilesJSON();
      setProfiles(JSON.parse(json) as DeviceProfile[]);
    });
  }, []);

  const handleSelect = (profile: DeviceProfile) => {
    onSelect(`${import.meta.env.BASE_URL}roms/${profile.romFilename}`);
  };

  return (
    <div className="max-w-4xl mx-auto px-6 py-10">
      <h2 className="text-xl font-mono font-semibold text-gray-200 mb-2">Select a device</h2>
      <p className="text-gray-500 text-sm mb-8">
        ROMs are served from the server. Click a supported device to start.
      </p>

      {profiles.length === 0 ? (
        <p className="text-gray-600 font-mono text-sm">Loading device list…</p>
      ) : (
        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 gap-4">
          {profiles.map(p => (
            <DeviceCard key={p.id} profile={p} onSelect={handleSelect} />
          ))}
        </div>
      )}
    </div>
  );
}
