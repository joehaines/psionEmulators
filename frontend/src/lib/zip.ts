// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal ZIP reader for the app library's per-app bundles. The
// bundles are produced by scripts/build-app-library.mts (stored or
// raw-deflate entries, no zip64, no encryption); inflate rides the
// browser's native DecompressionStream('deflate-raw') so no library is
// needed. Hand-rolled like the rest of lib/ (cf. converters/png-encoder).

export interface ZipEntry {
  name: string;
  size: number;          // uncompressed
  compressedSize: number;
  method: number;        // 0 = stored, 8 = deflate
  headerOffset: number;  // offset of the local file header
}

const EOCD_SIG = 0x06054b50;
const CEN_SIG  = 0x02014b50;
const LOC_SIG  = 0x04034b50;

// Parse the central directory. Throws on malformed input.
export function listZipEntries(zip: Uint8Array): ZipEntry[] {
  const dv = new DataView(zip.buffer, zip.byteOffset, zip.byteLength);
  // Find EOCD by scanning back from the end (comment can pad up to 64K).
  let eocd = -1;
  const minPos = Math.max(0, zip.length - 22 - 65536);
  for (let i = zip.length - 22; i >= minPos; i--) {
    if (dv.getUint32(i, true) === EOCD_SIG) { eocd = i; break; }
  }
  if (eocd < 0) throw new Error('zip: no end-of-central-directory record');
  const count = dv.getUint16(eocd + 10, true);
  let pos = dv.getUint32(eocd + 16, true);

  const entries: ZipEntry[] = [];
  for (let n = 0; n < count; n++) {
    if (dv.getUint32(pos, true) !== CEN_SIG) throw new Error('zip: bad central directory entry');
    const method = dv.getUint16(pos + 10, true);
    const compressedSize = dv.getUint32(pos + 20, true);
    const size = dv.getUint32(pos + 24, true);
    const nameLen = dv.getUint16(pos + 28, true);
    const extraLen = dv.getUint16(pos + 30, true);
    const commentLen = dv.getUint16(pos + 32, true);
    const headerOffset = dv.getUint32(pos + 42, true);
    const name = latin1(zip.subarray(pos + 46, pos + 46 + nameLen));
    entries.push({ name, size, compressedSize, method, headerOffset });
    pos += 46 + nameLen + extraLen + commentLen;
  }
  return entries;
}

// Extract one entry's bytes.
export async function readZipEntry(zip: Uint8Array, entry: ZipEntry): Promise<Uint8Array> {
  const dv = new DataView(zip.buffer, zip.byteOffset, zip.byteLength);
  const p = entry.headerOffset;
  if (dv.getUint32(p, true) !== LOC_SIG) throw new Error('zip: bad local header');
  const nameLen = dv.getUint16(p + 26, true);
  const extraLen = dv.getUint16(p + 28, true);
  const dataStart = p + 30 + nameLen + extraLen;
  const raw = zip.subarray(dataStart, dataStart + entry.compressedSize);
  if (entry.method === 0) return raw.slice();
  if (entry.method !== 8) throw new Error(`zip: unsupported method ${entry.method}`);
  return inflateRaw(raw, entry.size);
}

// Extract every entry: { 'path/in/zip': bytes }.
export async function unzipAll(zip: Uint8Array): Promise<Map<string, Uint8Array>> {
  const out = new Map<string, Uint8Array>();
  for (const e of listZipEntries(zip)) {
    if (e.name.endsWith('/')) continue;     // directory marker
    out.set(e.name, await readZipEntry(zip, e));
  }
  return out;
}

async function inflateRaw(data: Uint8Array, expectedSize: number): Promise<Uint8Array> {
  const ds = new DecompressionStream('deflate-raw');
  const stream = new Blob([data as BlobPart]).stream().pipeThrough(ds);
  const buf = new Uint8Array(await new Response(stream).arrayBuffer());
  if (expectedSize > 0 && buf.length !== expectedSize) {
    throw new Error(`zip: inflated ${buf.length} bytes, expected ${expectedSize}`);
  }
  return buf;
}

function latin1(b: Uint8Array): string {
  let s = '';
  for (const x of b) s += String.fromCharCode(x);
  return s;
}
