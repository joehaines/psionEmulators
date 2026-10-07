// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Which psion.wasm a saved session belongs to.
//
// A saved session is the WHOLE of the module's linear memory, and that
// includes the C++ vtables and every other function pointer, which in
// WebAssembly are indices into the module's own function table. Those
// indices, and the addresses of the static data around them, are decided by
// the compiler and linker: two builds of identical source can place them
// differently (a different Emscripten, a different host — the 6.0.1 build of
// the same commit on a Raspberry Pi did), and almost any code change moves
// them. Restored into any other engine, a snapshot calls the wrong functions:
// at best getDeviceInfo traps and the restore falls back to a cold boot; at
// worst the machine wedges on "Restoring saved session…" for good, which is
// what the desktop app did on a Pi with a session saved by a local build.
//
// STATE_SCHEMA_VERSION cannot catch that — it is bumped by hand, for object
// layout changes, and it has drifted before. So every save also records the
// engine that made it, as the SHA-256 of psion.wasm (the same hex that
// `sha256sum psion.wasm` prints), and a save only restores on that engine.
// Builds are reproducible (the CI engine is bit-identical to a local 6.0.1
// build on x64), so a release that does not touch the engine keeps sessions.
//
// frontend/public/emulator-worker.js carries its own copy of engineIdOf
// (a worker cannot import this file); workerRestore.test.mts checks the two
// agree.

/**
 * Identity of a psion.wasm: lowercase hex SHA-256. Without SubtleCrypto (an
 * insecure origin, e.g. a dev server reached over plain HTTP on a LAN) it is
 * an FNV-1a fingerprint instead, prefixed so it can never equal a digest.
 * Either way the main thread and the worker share an origin, so they compute
 * the same kind.
 */
export async function engineIdOf(bytes: Uint8Array): Promise<string> {
  const subtle = globalThis.crypto?.subtle;
  if (subtle) {
    const digest = new Uint8Array(await subtle.digest('SHA-256', bytes as BufferSource));
    return Array.from(digest, (b) => b.toString(16).padStart(2, '0')).join('');
  }
  // Two FNV-1a lanes with different offset bases, plus the length.
  let a = 0x811c9dc5, b = 0x01000193 ^ 0x5bd1e995;
  for (let i = 0; i < bytes.length; i++) {
    a = Math.imul(a ^ bytes[i], 0x01000193);
    b = Math.imul(b ^ bytes[i], 0x01000193);
  }
  return `fnv:${bytes.length.toString(16)}:${(a >>> 0).toString(16)}${(b >>> 0).toString(16)}`;
}

export interface Engine {
  /** The psion.wasm bytes, for instantiating with Emscripten's wasmBinary. */
  bytes: ArrayBuffer;
  id: string;
}

let enginePromise: Promise<Engine> | null = null;

/**
 * Fetch psion.wasm once and fingerprint it. The main-thread module is
 * instantiated from these same bytes (wasmBridge), so the identity stamped
 * on its saves is the identity of the code that ran — never of a newer file
 * fetched separately.
 */
export function fetchEngine(): Promise<Engine> {
  if (!enginePromise) {
    enginePromise = (async () => {
      const resp = await fetch(import.meta.env.BASE_URL + 'psion.wasm');
      if (!resp.ok) throw new Error(`psion.wasm: HTTP ${resp.status}`);
      const bytes = await resp.arrayBuffer();
      return { bytes, id: await engineIdOf(new Uint8Array(bytes)) };
    })();
    // A failed fetch must not be cached forever; the next caller retries.
    enginePromise.catch(() => { enginePromise = null; });
  }
  return enginePromise;
}

/** This build's engine identity, or null when psion.wasm cannot be read. */
export async function currentEngineId(): Promise<string | null> {
  try {
    return (await fetchEngine()).id;
  } catch {
    return null;
  }
}

/**
 * True when a stored save (or bundle entry) was made by the engine `id`.
 * Saves from before engines were recorded carry no `engine` and never match:
 * there is no way to know what made them.
 */
export function madeByEngine(stored: unknown, id: string | null): boolean {
  if (!id || !stored || typeof stored !== 'object') return false;
  return (stored as { engine?: unknown }).engine === id;
}
