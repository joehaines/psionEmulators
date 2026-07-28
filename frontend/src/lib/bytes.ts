// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Byte-buffer helpers shared by the removable-media dialogs.

// Equality check on two byte buffers. We compare via Uint32Array when both
// views are 4-byte aligned (the typical case — the buffers come from
// `new Uint8Array(...)` which gives a fresh, 0-offset ArrayBuffer);
// otherwise we fall back to a byte loop. Used by the CF / SSD auto-refresh
// pollers to skip a re-render when the device hasn't actually written
// anything.
export function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
  if (a.byteLength !== b.byteLength) return false;
  if ((a.byteOffset & 3) === 0 && (b.byteOffset & 3) === 0) {
    const words = a.byteLength >>> 2;
    const a32 = new Uint32Array(a.buffer, a.byteOffset, words);
    const b32 = new Uint32Array(b.buffer, b.byteOffset, words);
    for (let i = 0; i < words; i++) if (a32[i] !== b32[i]) return false;
    const tailStart = words << 2;
    for (let i = tailStart; i < a.byteLength; i++) if (a[i] !== b[i]) return false;
    return true;
  }
  for (let i = 0; i < a.byteLength; i++) if (a[i] !== b[i]) return false;
  return true;
}
