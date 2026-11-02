// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The worker's half of the Psion LX Wi-Fi relay (emulator-worker.js:
// attachNetHost / drainNet / deliverNetEvents). The core queues its socket
// requests as binary records (wasm/main.cpp, JsNetHost); the worker decodes
// them into the NetCommand objects window.psionHost.net takes, and feeds the
// host's NetEvents back into the core. This loads the REAL worker source in
// a vm sandbox against a stub module that plays the core's side, and checks:
//
//   - 'netHost' attaches the core to the JS host and resets the host's flows;
//   - records decode to the right commands, with their payloads intact;
//   - events reach the matching core entry points, data through the heap;
//   - a flow whose backlog is large is paused, and resumed once it drains.
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/workerNet.test.mts

import { readFileSync } from 'node:fs';
import vm from 'node:vm';

let failures = 0;
function check(cond: unknown, msg: string) {
  console.log(`${cond ? '✓' : '✗'} ${msg}`);
  if (!cond) failures++;
}

// ── Stub module: the core's network side ─────────────────────────────────
const heap = new Uint8Array(1 << 20);
let next = 1024;
const calls: string[] = [];
let queue: number[] = [];           // pending command records, as bytes
let backlog = 0;
function record(kind: number, id: number, ip: number, port: number, data: number[] = []) {
  const b: number[] = [kind];
  for (const v of [id, ip]) for (let i = 0; i < 4; i++) b.push((v >>> (8 * i)) & 255);
  b.push(port & 255, port >> 8);
  for (let i = 0; i < 4; i++) b.push((data.length >>> (8 * i)) & 255);
  queue.push(...b, ...data);
}
const mod = {
  HEAPU8: heap,
  _malloc(n: number) { const p = next; next += (n + 7) & ~7; return p; },
  _free() {},
  netAttachHost(on: boolean) { calls.push(`attach ${on}`); return true; },
  netPendingBytes() { return queue.length; },
  netDrainCommands(ptr: number, cap: number) {
    const n = Math.min(queue.length, cap);
    heap.set(queue.slice(0, n), ptr);
    queue = queue.slice(n);
    return n;
  },
  netPrepareInput(n: number) { const p = mod._malloc(n); calls.push(`prepare ${n}`); return p; },
  lastInput: 0,
  netTcpOpened(id: number) { calls.push(`opened ${id}`); },
  netTcpFailed(id: number) { calls.push(`failed ${id}`); },
  netTcpEof(id: number) { calls.push(`eof ${id}`); },
  netTcpData(id: number, len: number) {
    const p = next - ((len + 7) & ~7);
    calls.push(`data ${id} ${new TextDecoder().decode(heap.slice(p, p + len))}`);
  },
  netUdpData(id: number, ip: number, port: number, len: number) { calls.push(`udp ${id} ${ip.toString(16)} ${port} ${len}`); },
  netTcpBacklog() { return backlog; },
  getSimCycles() { return 0; },
  getDeviceInfo() { return { deviceName: 'Stub', lcdWidth: 8, lcdHeight: 8, audioSampleRate: 8000 }; },
};

// ── Sandbox: run the real worker source ──────────────────────────────────
const workerSrc = readFileSync(new URL('../../../public/emulator-worker.js', import.meta.url), 'utf8');
const posted: Array<Record<string, unknown>> = [];
const sandbox: Record<string, unknown> = {
  onmessage: null,
  postMessage: (m: Record<string, unknown>) => posted.push(m),
  importScripts: () => {},
  createPsionModule: async () => mod,
  indexedDB: { open() { return {}; } },
  crypto: globalThis.crypto,
  fetch: async () => ({ ok: true, headers: { get: () => null }, arrayBuffer: async () => new Uint8Array(8).buffer }),
  performance: { now: () => Date.now() },
  setTimeout: () => 0,
  console: { log: () => {}, error: () => {}, warn: () => {} },
};
sandbox.self = sandbox;
vm.createContext(sandbox);
vm.runInContext(workerSrc, sandbox, { filename: 'emulator-worker.js' });
const deliver = sandbox.onmessage as unknown as (e: { data: Record<string, unknown> }) => Promise<void>;
await deliver({ data: { type: 'init', baseUrl: '/' } });

const commandsPosted = () => posted.filter((m) => m.type === 'netCommands')
  .flatMap((m) => m.cmds as Array<Record<string, unknown>>);

// Attach.
await deliver({ data: { type: 'netHost', on: true } });
check(calls.includes('attach true'), "'netHost' attaches the core to the JS socket host");
check(commandsPosted().some((c) => c.kind === 'reset'), 'and tells the host to drop any old flows');

// Requests out: connect, send, shutdown, UDP, close.
posted.length = 0;
record(1, 5, 0x0a000202, 8080);
record(2, 5, 0, 0, [...new TextEncoder().encode('GET /')]);
record(3, 5, 0, 0);
record(5, 1024, 0x0a000203, 53, [1, 2, 3]);
await deliver({ data: { type: 'netEvents', events: [] } });     // ends in a drain
const cmds = commandsPosted();
check(cmds.length === 4, `four records decode to four commands (${cmds.length})`);
check(cmds[0]?.kind === 'tcp-connect' && cmds[0].id === 5 && cmds[0].ip === 0x0a000202 && cmds[0].port === 8080,
      'tcp-connect carries id, address and port');
check(cmds[1]?.kind === 'tcp-send' && new TextDecoder().decode(cmds[1].data as Uint8Array) === 'GET /',
      'tcp-send carries its bytes');
check(cmds[2]?.kind === 'tcp-shutdown' && cmds[2].id === 5, 'tcp-shutdown');
check(cmds[3]?.kind === 'udp-send' && cmds[3].ip === 0x0a000203 && cmds[3].port === 53
      && (cmds[3].data as Uint8Array).length === 3, 'udp-send carries address, port and bytes');

// Events in.
calls.length = 0;
await deliver({ data: { type: 'netEvents', events: [
  { kind: 'tcp-opened', id: 5 },
  { kind: 'tcp-data', id: 5, data: new TextEncoder().encode('HTTP/1.0 200') },
  { kind: 'udp-data', id: 1024, ip: 0x0a000203, port: 53, data: new Uint8Array(12) },
  { kind: 'tcp-eof', id: 5 },
] } });
check(calls.includes('opened 5'), 'tcp-opened reaches the core');
check(calls.includes('data 5 HTTP/1.0 200'), 'tcp-data reaches the core through the heap');
check(calls.includes('udp 1024 a000203 53 12'), 'udp-data reaches the core with its source');
check(calls.includes('eof 5'), 'tcp-eof reaches the core');

// Flow control.
posted.length = 0;
backlog = 300 * 1024;
await deliver({ data: { type: 'netEvents', events: [] } });
check(commandsPosted().some((c) => c.kind === 'tcp-pause' && c.id === 5), 'a large backlog pauses the socket');
posted.length = 0;
backlog = 0;
await deliver({ data: { type: 'netEvents', events: [] } });
check(commandsPosted().some((c) => c.kind === 'tcp-resume' && c.id === 5), 'and it resumes once the guest catches up');

// A closed flow is forgotten.
posted.length = 0;
record(4, 5, 0, 0);
backlog = 300 * 1024;
await deliver({ data: { type: 'netEvents', events: [] } });
check(commandsPosted().some((c) => c.kind === 'tcp-close' && c.id === 5)
      && !commandsPosted().some((c) => c.kind === 'tcp-pause'), 'a closed flow is closed and no longer flow-controlled');

if (failures) { console.error(`\n${failures} check(s) failed`); process.exit(1); }
console.log('\nworker net relay ok');
