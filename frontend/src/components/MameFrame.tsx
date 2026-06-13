// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

interface Props {
  baseUrl: string;
  onClose: () => void;
  onSwitchToNative: () => void;
  deviceName: string;
  deviceId: string;
}

// Series 3a runs under MAME 0.253 compiled to WebAssembly, served from
// /mame-3a/. The bundle (psion3aweb.{js,wasm,data,data.js}) is produced
// by scripts/build-mame-3a.sh; the standalone index.html shows a
// "bundle not built" notice when the artefacts are absent so deploys
// without the MAME build still render a useful message.
export default function MameFrame({ baseUrl, onClose, onSwitchToNative, deviceName, deviceId }: Props) {
  return (
    <div className="absolute inset-0 flex flex-col bg-psion-dark">
      <div className="px-3 py-1.5 text-[11px] text-gray-400 border-b border-psion-accent/30 flex items-center gap-3 flex-shrink-0">
        <span>{deviceName} · MAME 0.253</span>
        <button
          type="button"
          onClick={onSwitchToNative}
          className="ml-auto px-2 py-0.5 rounded hover:bg-psion-mid text-gray-300"
          title="Run on the native emulator core instead of MAME (may show a 'Media is corrupt' dialog on first boot)"
        >
          Native emulator
        </button>
        <button
          type="button"
          onClick={onClose}
          className="px-2 py-0.5 rounded hover:bg-psion-mid text-gray-300"
        >
          Close
        </button>
      </div>
      <iframe
        src={`${baseUrl}mame-3a/index.html?machine=${encodeURIComponent(deviceId)}`}
        title={deviceName}
        className="flex-1 w-full border-0 bg-psion-dark"
        allow="autoplay; clipboard-read; clipboard-write"
      />
    </div>
  );
}
