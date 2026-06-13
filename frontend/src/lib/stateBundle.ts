// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// State-bundle file format for the "Download All" / "Upload" controls in
// the device panel.
//
// Goals: round-trip every device's heap + storage images without OOMing
// the browser on multi-device exports, and stay parseable by future
// builds via a versioned JSON header.
//
// File layout (little-endian throughout):
//
//   [ 8 bytes  ]  Magic "PSIONST1" (ASCII, no NUL)
//   [ 4 bytes  ]  Header JSON length (u32)
//   [ N bytes  ]  Header JSON (UTF-8) — schema below
//   [ rest     ]  Concatenated chunk payloads, each gzip-compressed,
//                 indexed by offset/length entries in the header.
//
// The whole file is constructed as a `Blob` of multiple parts so we never
// materialise a single multi-GB string (the v1 JSON-with-base64 format
// did and crashed the tab once two or three heaps were in the bundle).
// The header itself stays small (a few KB) because all binary data lives
// in the trailing payload, referenced by byte offsets.
//
// Chunk semantics:
//   heap     — the full WASM linear memory snapshot (already gzip at save
//              time and stored that way in IDB; copied verbatim into the
//              payload).
//   cf       — CompactFlash card image (raw at the IDB layer; gzipped
//              here at export time).
//   ssd0/1   — Psion SSD pack image per slot (raw → gzip here).
//   datapak0/1 — Organiser II Datapak/Rampak per slot (raw → gzip here).
//
// Backward compatibility: decodeBundle also accepts the legacy v1 JSON
// format (the one that JSON.stringify-d base64 strings) so users with an
// old export aren't stranded. The legacy path is read-only — encodeBundle
// always emits the new binary format.

const MAGIC = new TextEncoder().encode('PSIONST1');
const HEADER_LEN_BYTES = 4;
const BUNDLE_VERSION = 2;

export interface BundleChunk {
  /** Offset into the file's payload region (relative to payload start). */
  offset: number;
  /** Compressed length in bytes. */
  length: number;
}

export interface BundleDeviceMeta {
  id: string;
  /** Profile displayName at save time; informational only. */
  displayName?: string;
  /** STATE_SCHEMA_VERSION at save time. */
  schemaVersion: number;
  chunks: {
    heap?: BundleChunk;
    cf?: BundleChunk;
    ssd0?: BundleChunk;
    ssd1?: BundleChunk;
    datapak0?: BundleChunk;
    datapak1?: BundleChunk;
  };
}

export interface BundleHeader {
  bundleVersion: 2;
  savedAt: string;
  devices: BundleDeviceMeta[];
}

export interface BundleDeviceInput {
  id: string;
  displayName?: string;
  schemaVersion: number;
  /**
   * The IDB-stored heap blob. Already gzip-compressed (that's the wire
   * shape we store at save time). Pass it through unchanged.
   */
  heap?: Uint8Array;
  /** Raw CF image; the encoder gzips it. */
  cf?: Uint8Array;
  ssd0?: Uint8Array;
  ssd1?: Uint8Array;
  datapak0?: Uint8Array;
  datapak1?: Uint8Array;
}

export interface DecodedDeviceChunks {
  /** Always returned as already gzip-compressed; caller decompresses. */
  heap?: Uint8Array;
  cf?: Uint8Array;
  ssd0?: Uint8Array;
  ssd1?: Uint8Array;
  datapak0?: Uint8Array;
  datapak1?: Uint8Array;
}

export interface DecodedBundle {
  header: BundleHeader;
  /** deviceId → compressed chunks. */
  devices: Map<string, DecodedDeviceChunks>;
}

// ── gzip helpers (use CompressionStream when available; falls back to a
//    pass-through stub so unit tests can still run in environments
//    without it — the encoded shape is still valid as long as both sides
//    agree). All saved heaps go through compress() at save time, so the
//    fallback only matters on platforms that lack both APIs.

async function gzip(data: Uint8Array): Promise<Uint8Array> {
  if (typeof CompressionStream === 'undefined') return data;
  const cs = new CompressionStream('gzip');
  const writer = cs.writable.getWriter();
  // Cast around the TS 5.7+ stricter Uint8Array<ArrayBufferLike> →
  // BufferSource typing — the runtime accepts any ArrayBufferView.
  void writer.write(data as BufferSource);
  void writer.close();
  return concatStream(cs.readable);
}

async function gunzip(data: Uint8Array): Promise<Uint8Array> {
  if (data.length < 2 || data[0] !== 0x1f || data[1] !== 0x8b) return data;
  if (typeof DecompressionStream === 'undefined') return data;
  const ds = new DecompressionStream('gzip');
  const writer = ds.writable.getWriter();
  void writer.write(data as BufferSource);
  void writer.close();
  return concatStream(ds.readable);
}

async function concatStream(stream: ReadableStream<Uint8Array>): Promise<Uint8Array> {
  const reader = stream.getReader();
  const parts: Uint8Array[] = [];
  let total = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    parts.push(value);
    total += value.length;
  }
  const out = new Uint8Array(total);
  let off = 0;
  for (const p of parts) { out.set(p, off); off += p.length; }
  return out;
}

// ── Encoder ────────────────────────────────────────────────────────────

/**
 * Builds the binary state bundle. Returns a Blob assembled from references
 * to each compressed chunk — no intermediate flat string ever exists, so
 * exporting 17 devices' heaps doesn't OOM the tab.
 */
export async function encodeBundle(devices: BundleDeviceInput[]): Promise<Blob> {
  // First pass: compress every chunk that isn't already compressed.
  // heap is already gzip-compressed at save time (see triggerSave in
  // useEmulator.ts); everything else is raw at the IDB layer.
  interface CompressedDevice {
    id: string;
    displayName?: string;
    schemaVersion: number;
    chunks: { name: keyof DecodedDeviceChunks; bytes: Uint8Array }[];
  }
  const compressed: CompressedDevice[] = [];
  for (const d of devices) {
    const cd: CompressedDevice = {
      id: d.id,
      displayName: d.displayName,
      schemaVersion: d.schemaVersion,
      chunks: [],
    };
    if (d.heap && d.heap.length > 0) {
      cd.chunks.push({ name: 'heap', bytes: d.heap });
    }
    for (const k of ['cf', 'ssd0', 'ssd1', 'datapak0', 'datapak1'] as const) {
      const raw = d[k];
      if (raw && raw.length > 0) {
        cd.chunks.push({ name: k, bytes: await gzip(raw) });
      }
    }
    compressed.push(cd);
  }

  // Second pass: build the header with offset/length entries.
  const header: BundleHeader = {
    bundleVersion: BUNDLE_VERSION,
    savedAt: new Date().toISOString(),
    devices: [],
  };
  let payloadOffset = 0;
  const payloadParts: Uint8Array[] = [];
  for (const cd of compressed) {
    const meta: BundleDeviceMeta = {
      id: cd.id,
      displayName: cd.displayName,
      schemaVersion: cd.schemaVersion,
      chunks: {},
    };
    for (const { name, bytes } of cd.chunks) {
      meta.chunks[name] = { offset: payloadOffset, length: bytes.length };
      payloadParts.push(bytes);
      payloadOffset += bytes.length;
    }
    header.devices.push(meta);
  }

  // Serialise the header (small — a few KB even for many devices) and
  // assemble the final blob from references. Blob assembly does not
  // materialise the contents into one buffer — it stores references —
  // which is the whole point of switching away from JSON.stringify.
  const headerJson = new TextEncoder().encode(JSON.stringify(header));
  const headerLen = new Uint8Array(HEADER_LEN_BYTES);
  new DataView(headerLen.buffer).setUint32(0, headerJson.length, true);

  // Cast each Uint8Array part to BlobPart — TS 5.7+ narrowed the
  // BlobPart inference on Uint8Array<ArrayBufferLike>, but the runtime
  // accepts any ArrayBufferView via the Blob constructor.
  const parts: BlobPart[] = [MAGIC as BlobPart, headerLen as BlobPart, headerJson as BlobPart];
  for (const p of payloadParts) parts.push(p as BlobPart);
  return new Blob(parts, { type: 'application/octet-stream' });
}

// ── Decoder ────────────────────────────────────────────────────────────

/**
 * Parses a bundle file. Accepts the new binary format and, for back-compat,
 * the legacy v1 JSON-with-base64 format. Returns each device's chunks as
 * still-gzip-compressed Uint8Arrays — the caller decompresses on demand,
 * which keeps multi-device imports memory-light (only one chunk is
 * decompressed at a time downstream).
 */
export async function decodeBundle(fileBytes: Uint8Array): Promise<DecodedBundle> {
  if (looksLikeBinary(fileBytes)) return decodeBinaryBundle(fileBytes);
  return decodeLegacyJsonBundle(fileBytes);
}

function looksLikeBinary(bytes: Uint8Array): boolean {
  if (bytes.length < MAGIC.length) return false;
  for (let i = 0; i < MAGIC.length; i++) {
    if (bytes[i] !== MAGIC[i]) return false;
  }
  return true;
}

async function decodeBinaryBundle(bytes: Uint8Array): Promise<DecodedBundle> {
  let p = MAGIC.length;
  if (bytes.length < p + HEADER_LEN_BYTES) {
    throw new Error('Bundle truncated before header length');
  }
  const headerLen = new DataView(bytes.buffer, bytes.byteOffset + p, HEADER_LEN_BYTES).getUint32(0, true);
  p += HEADER_LEN_BYTES;
  if (bytes.length < p + headerLen) {
    throw new Error('Bundle truncated inside header');
  }
  const headerJson = new TextDecoder().decode(bytes.subarray(p, p + headerLen));
  const header = JSON.parse(headerJson) as BundleHeader;
  if (header.bundleVersion !== BUNDLE_VERSION) {
    throw new Error(
      `Unsupported bundle version: ${header.bundleVersion} ` +
      `(this build understands v${BUNDLE_VERSION})`
    );
  }
  const payloadStart = p + headerLen;

  const devices = new Map<string, DecodedDeviceChunks>();
  for (const meta of header.devices) {
    const out: DecodedDeviceChunks = {};
    for (const k of ['heap', 'cf', 'ssd0', 'ssd1', 'datapak0', 'datapak1'] as const) {
      const c = meta.chunks[k];
      if (!c) continue;
      const start = payloadStart + c.offset;
      const end = start + c.length;
      if (end > bytes.length) {
        throw new Error(`Chunk '${k}' for device '${meta.id}' overruns the file`);
      }
      out[k] = bytes.slice(start, end);
    }
    devices.set(meta.id, out);
  }
  return { header, devices };
}

// Legacy v1 (JSON+base64). Lossy on memory but kept for users who saved a
// small bundle under the old build. Not used by encodeBundle — the binary
// format is the only emitter going forward.
async function decodeLegacyJsonBundle(bytes: Uint8Array): Promise<DecodedBundle> {
  interface LegacyEntry {
    displayName?: string;
    heap?: string;
    cf?: string;
    ssd?: (string | null)[];
    datapak?: (string | null)[];
  }
  interface LegacyBundle {
    bundleVersion: number;
    schemaVersion: number;
    savedAt?: string;
    devices: Record<string, LegacyEntry>;
  }
  const text = new TextDecoder().decode(bytes);
  const legacy = JSON.parse(text) as LegacyBundle;
  if (legacy.bundleVersion !== 1) {
    throw new Error(`Unsupported legacy bundle version: ${legacy.bundleVersion}`);
  }
  const header: BundleHeader = {
    bundleVersion: BUNDLE_VERSION,
    savedAt: legacy.savedAt ?? new Date().toISOString(),
    devices: [],
  };
  const devices = new Map<string, DecodedDeviceChunks>();
  for (const [id, entry] of Object.entries(legacy.devices)) {
    const chunks: DecodedDeviceChunks = {};
    // The legacy format stored heap as already-gzip bytes inside the
    // base64 string. CF/SSD/Datapak were ALSO gzip-then-base64 (see the
    // old exportAllStates — it called `compress` on them before
    // base64). So every chunk in legacy format is already gzip on the
    // wire, just base64-wrapped. After base64 decode we already have
    // the gzipped bytes; caller treats them identically to the new
    // format.
    if (entry.heap)     chunks.heap = base64ToUint8(entry.heap);
    if (entry.cf)       chunks.cf   = base64ToUint8(entry.cf);
    if (entry.ssd?.[0]) chunks.ssd0 = base64ToUint8(entry.ssd[0]!);
    if (entry.ssd?.[1]) chunks.ssd1 = base64ToUint8(entry.ssd[1]!);
    if (entry.datapak?.[0]) chunks.datapak0 = base64ToUint8(entry.datapak[0]!);
    if (entry.datapak?.[1]) chunks.datapak1 = base64ToUint8(entry.datapak[1]!);
    header.devices.push({
      id,
      displayName: entry.displayName,
      schemaVersion: legacy.schemaVersion,
      chunks: {},  // not used by the importer in this fallback path
    });
    devices.set(id, chunks);
  }
  return { header, devices };
}

function base64ToUint8(b64: string): Uint8Array {
  const binary = atob(b64);
  const out = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) out[i] = binary.charCodeAt(i);
  return out;
}

/**
 * Helper for the import path: gunzip a chunk that's still in its
 * stored / on-wire form. Exposed so the hook can keep its own compress
 * helpers private and route everything through this module.
 */
export async function decompressChunk(bytes: Uint8Array): Promise<Uint8Array> {
  return gunzip(bytes);
}

/**
 * Helper used by the save path: gzip a freshly-sliced heap before
 * writing to IDB. Centralised here so encode/decode and storage all
 * agree on the wire format.
 */
export async function compressChunk(bytes: Uint8Array): Promise<Uint8Array> {
  return gzip(bytes);
}

// Exposed for tests that want to introspect the layout without going
// through Blob.arrayBuffer().
export const _internal = { MAGIC, BUNDLE_VERSION, HEADER_LEN_BYTES };
