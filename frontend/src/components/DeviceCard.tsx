// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import type { DeviceProfile } from '../types/emulator';
import { DEVICE_LOGO_MAP } from '../lib/deviceMeta';

interface Props {
  profile: DeviceProfile;
  onSelect: (profile: DeviceProfile) => void;
}

export default function DeviceCard({ profile, onSelect }: Props) {
  const supported = profile.status === 'supported';

  return (
    <button
      onClick={() => supported && onSelect(profile)}
      disabled={!supported}
      className={[
        'relative flex flex-col items-center gap-3 p-5 rounded-xl border text-left transition-all',
        supported
          ? 'border-psion-accent/40 bg-psion-dark shadow-sm hover:border-psion-accent hover:shadow-md hover:scale-105 cursor-pointer'
          : 'border-gray-200 bg-gray-50 opacity-60 cursor-not-allowed',
      ].join(' ')}
      title={supported ? `Load ${profile.displayName}` : 'Not yet emulated'}
    >
      {/* Device logo */}
      <div className="w-24 h-16 flex items-center justify-center bg-gray-100 border border-psion-accent/20 rounded-md overflow-hidden">
        {DEVICE_LOGO_MAP[profile.id] ? (
          <img
            src={`${import.meta.env.BASE_URL}device-logos/${DEVICE_LOGO_MAP[profile.id]}`}
            alt={profile.displayName}
            className="max-w-full max-h-full object-contain"
          />
        ) : (
          <span className="text-gray-400 text-xs font-mono">
            {profile.displayName.split(' ').map(w => w[0]).join('').slice(0, 3)}
          </span>
        )}
      </div>

      <div className="text-center">
        <p className="text-sm font-semibold text-psion-charcoal">{profile.displayName}</p>
        {!supported && (
          <span className="inline-block mt-1 px-2 py-0.5 rounded-full text-xs bg-gray-200 text-gray-500">
            Coming soon
          </span>
        )}
      </div>
    </button>
  );
}
