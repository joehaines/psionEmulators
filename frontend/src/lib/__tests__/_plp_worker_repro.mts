// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Deterministic reproduction of the WORKER-MODE Remote Link upload stall that
// the browser "Try it" flow hits (and which the native harness — lossless,
// continuously drained — never shows). Loads the REAL emscripten WASM emulator
// (frontend/public/psion.js) in Node and faithfully replays the web worker's
// timing model around it:
//   • real-time-paced stepFrameFull() (1/64 s of sim per frame, TICK_WALL_CAP)
//   • drainSerial() once per worker tick (device→host)
//   • the serialWrite turnaround pump (host→device, FIFO-overflow retry)
//   • the per-sim-frame sim-time keepalive injection
// and drives a real PlpClient upload across that worker boundary. The C++
// device log (cpu.log → console.log) surfaces here exactly as in the browser.
//
// Usage:
//   node --experimental-strip-types frontend/src/lib/__tests__/_plp_worker_repro.mts [sizeBytes]
//   LOG=1 …   enable the device-side byte log (the heisenbug "works with logging" case)
//   PDU=1 …   trace host PLP PDUs in/out

import { createRequire } from 'node:module';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';
import { setSerialPumpForward, setSimKeepAliveForward } from '../wasmBridge.ts';
import type { Pdu } from '../plp/link.ts';

const require = createRequire(import.meta.url);
const PUB = new URL('../../../public/', import.meta.url).pathname;
const ROM = new URL('../../../../roms/5mx_v1.05(260)_eng.bin', import.meta.url).pathname;
const SIZE = Number(process.argv[2] ?? 190_000);
const LOOPS = Number(process.env.LOOPS ?? 1);   // repeat the upload in ONE session
const LOG = !!process.env.LOG;
const PDU = !!process.env.PDU;

const SERIAL_CAP = 4096;
const UART = 2;
const SIM_FRAME_MS = 1000 / 64;
const TICK_WALL_CAP = 12;
const now = () => performance.now();
// Wall-cost knobs modelling browser-worker overhead the harness otherwise
// skips: RENDER_MS = LCD readLCD+putImageData per rendered tick; MSG_MS =
// postMessage cost per serial frame crossing the worker boundary. These steal
// real time from stepFrameFull while the host keeps sending, which is how the
// browser starves the guest's RemoteLinkServer (and why Show Logs — which adds
// a postMessage per logged byte — changes the regime).
const RENDER_MS = Number(process.env.RENDER_MS ?? 0);
const MSG_MS = Number(process.env.MSG_MS ?? 0);
function busy(ms: number): void { if (ms <= 0) return; const e = now() + ms; while (now() < e) { /* spin */ } }

/* eslint-disable @typescript-eslint/no-explicit-any */
let mod: any;
let serialScratch = 0;
let serialPumpOn = false;
let simKeepAliveBytes: Uint8Array | null = null;

// Worker boundary queues.
const hostToWorker: Uint8Array[] = [];   // serialWrite payloads from PlpClient (BATCH mode)
let workerToHost: Uint8Array[] = [];     // device→host chunks consumed by PlpClient.read
// BATCH=1 drains all host writes at the top of each worker tick (cooperative,
// no starvation). Default models the BROWSER worker: each serialWrite is an
// independent task (setImmediate) that competes with the stepFrameFull tick on
// the single worker thread — its 30ms turnaround pump / 200ms FIFO-overflow
// retry then starve the frame loop under upload load, which is the regime that
// stalls in the browser but not in a batched cooperative model.
const BATCH = !!process.env.BATCH;

function drainSerial(): void {
  for (;;) {
    const n = mod.serialReadToHost(UART, serialScratch, SERIAL_CAP);
    if (n <= 0) break;
    const bytes = mod.HEAPU8.slice(serialScratch, serialScratch + n);
    workerToHost.push(bytes);
    busy(MSG_MS);   // postMessage(serialRx) cost
    if (n < SERIAL_CAP) break;
  }
}

// Faithful port of emulator-worker.js serialWrite handler.
function processSerialWrite(data: Uint8Array): void {
  let off = 0;
  const writeDeadline = now() + (process.env.NODROP ? 2000 : 200);
  let stalls = 0;
  while (off < data.length) {
    const nn = Math.min(SERIAL_CAP, data.length - off);
    mod.HEAPU8.set(data.subarray(off, off + nn), serialScratch);
    const acc = mod.serialWriteFromHost(UART, serialScratch, nn);
    off += acc;
    if (acc < nn) {
      if (!mod.serialPumpCycles) break;
      if (process.env.NODROP) {
        stalls = acc === 0 ? stalls + 1 : 0;
        if (stalls > 2000 || now() > writeDeadline) break;
      } else if (now() > writeDeadline) break;
      mod.serialPumpCycles();
      drainSerial();
    }
  }
  if (mod.serialPumpCycles) {
    mod.serialPumpCycles();
    if (serialPumpOn) {
      const deadline = now() + 30;
      while (now() < deadline) {
        const n = mod.serialReadToHost(UART, serialScratch, SERIAL_CAP);
        if (n > 0) {
          const bytes = mod.HEAPU8.slice(serialScratch, serialScratch + n);
          workerToHost.push(bytes);
          break;
        }
        mod.serialPumpCycles();
      }
    }
  }
  drainSerial();
}

// Faithful port of emulator-worker.js tick() frame loop (real-time paced).
let nextFrameDue = 0;
let rendered = false;
function workerTick(): void {
  const t0 = now();
  rendered = false;
  // BATCH mode only: drain host writes cooperatively before the frame loop.
  if (BATCH) while (hostToWorker.length) processSerialWrite(hostToWorker.shift()!);
  if (nextFrameDue === 0) nextFrameDue = t0;
  while (t0 >= nextFrameDue && (now() - t0) < TICK_WALL_CAP) {
    if (serialPumpOn && simKeepAliveBytes && mod.serialWriteFromHost && !process.env.NOSKA) {
      const kn = Math.min(simKeepAliveBytes.length, SERIAL_CAP);
      mod.HEAPU8.set(simKeepAliveBytes.subarray(0, kn), serialScratch);
      mod.serialWriteFromHost(UART, serialScratch, kn);
    }
    mod.stepFrameFull();
    if (process.env.DPF) drainSerial();   // drain device→host after EACH frame (test txQueue-overflow fix)
    nextFrameDue += SIM_FRAME_MS;
    rendered = true;
  }
  if (now() - nextFrameDue > 250) nextFrameDue = now();
  drainSerial();
  if (rendered) busy(RENDER_MS);   // LCD readLCD + putImageData per rendered tick
}

const pduStr = (p: Pdu) => `cont=${p.cont} seq=${p.seq} len=${p.data.length}`;

(async () => {
  console.error(`[harness] loading WASM…  size=${SIZE}B  LOG=${LOG ? 1 : 0}`);
  // frontend/package.json is "type":"module", so psion.js is treated as ESM
  // (where its CJS `module.exports=` never runs). Require a .cjs copy instead,
  // and point locateFile at the real .wasm beside the original.
  const cjs = `${PUB}psion.node.${process.pid}.cjs`;
  fs.copyFileSync(`${PUB}psion.js`, cjs);
  process.on('exit', () => { try { fs.unlinkSync(cjs); } catch { /* ignore */ } });
  const createPsionModule = require(cjs);
  mod = await createPsionModule({
    print: () => {}, printErr: () => {},
    locateFile: (p: string) => p.endsWith('.wasm') ? `${PUB}psion.wasm` : p,
  });
  serialScratch = mod._malloc(SERIAL_CAP);

  const rom = fs.readFileSync(ROM);
  const ptr = mod.prepareROMUpload(rom.length);
  mod.HEAPU8.set(rom, ptr);
  mod.loadBufferedROM(rom.length, '5mx');
  console.error('[harness] booting (unpaced preroll)…');
  for (let i = 0; i < 64 * 45; i++) mod.stepFrameFull();   // ~45 s sim boot
  if (LOG && mod.setLoggingEnabled) mod.setLoggingEnabled(true);
  mod.serialAttachHost(UART);
  console.error('[harness] serial attached; starting worker tick + PlpClient');

  setSerialPumpForward(on => { serialPumpOn = on; });
  setSimKeepAliveForward(b => { simKeepAliveBytes = b ? b.slice() : null; });

  const worker = setInterval(workerTick, 3);

  let lastIn: Pdu | null = null;
  let maxDataSeq = -1, lastDataSeq = -1;
  const client = new PlpClient(
    () => { if (!workerToHost.length) return new Uint8Array(0); let t=0; for(const c of workerToHost) t+=c.length; const o=new Uint8Array(t); let off=0; for(const c of workerToHost){o.set(c,off);off+=c.length;} workerToHost=[]; return o; },
    d => {
      const c = d.slice();
      // Browser model: each serialWrite is an independent worker task competing
      // with the stepFrameFull tick. Batched model: queue for the tick to drain.
      if (BATCH) hostToWorker.push(c);
      else setImmediate(() => { if (mod) { processSerialWrite(c); busy(MSG_MS); } });
      return d.length;
    },
    {
      pollHz: 200, protocol: 'rfsv32', conSeq: 4,
      ...(process.env.CHUNK ? { chunkSize: Number(process.env.CHUNK) } : {}),
      onState: s => console.error('[host] state', s),
      onPduIn: p => { lastIn = p; if (PDU) console.error('  IN ', pduStr(p)); },
      onPduOut: p => {
        if (p.cont === 3) {
          // Watch the host Tx data seq cross the mod-2048 wrap (2047→1), the
          // suspected desync point from the field iOS capture.
          if (p.seq > maxDataSeq) maxDataSeq = p.seq;
          if (lastDataSeq >= 2040 && p.seq <= 4)
            console.error(`[host] *** Tx seq WRAP ${lastDataSeq} -> ${p.seq} (maxSeq=${maxDataSeq}) ***`);
          lastDataSeq = p.seq;
        }
        if (PDU && p.cont !== 0) console.error('  OUT', pduStr(p));
      },
    },
  );
  client.start();

  let code = 1;
  try {
    await client.connect(45_000);
    console.error(`[host] connected; uploading ${LOOPS}x ${SIZE}B in one session…`);
    const payload = new Uint8Array(SIZE);
    for (let i = 0; i < SIZE; i++) payload[i] = (i * 31 + 7) & 0xFF;
    let lastDone = 0, lastMoveAt = now();
    const watchdog = setInterval(() => {
      if (now() - lastMoveAt > 25_000)
        console.error(`[host] STALL: no upload progress for 25s at ${lastDone}/${SIZE}, maxTxSeq=${maxDataSeq}, lastInbound=${lastIn ? pduStr(lastIn) : 'none'}`);
    }, 5_000);
    const t0 = now();
    for (let loop = 0; loop < LOOPS; loop++) {
      lastDone = 0; lastMoveAt = now();
      await client.uploadFile('C:\\Documents\\big.bin', payload, (done) => {
        if (done !== lastDone) { lastDone = done; lastMoveAt = now(); }
      });
      console.error(`[host] upload ${loop + 1}/${LOOPS} done (maxTxSeq=${maxDataSeq})`);
    }
    clearInterval(watchdog);
    const secs = ((now() - t0) / 1000).toFixed(1);
    console.error(`[host] all uploads done in ${secs}s; verifying…`);
    let dlDone = 0, dlMoveAt = now(), dlPct = -1;
    const dlWatch = setInterval(() => {
      if (now() - dlMoveAt > 25_000) console.error(`[host] DOWNLOAD STALL at ${dlDone}/${SIZE}, lastInbound=${lastIn ? pduStr(lastIn) : 'none'}`);
    }, 5_000);
    const back = await client.downloadFile('C:\\Documents\\big.bin', (b: number) => {
      if (b !== dlDone) { dlDone = b; dlMoveAt = now(); }
      const pct = Math.floor((b / SIZE) * 100);
      if (pct >= dlPct + 10) { dlPct = pct; console.error(`[host] download ${pct}% (${b})`); }
    });
    clearInterval(dlWatch);
    let ok = back.length === payload.length;
    if (ok) for (let i = 0; i < payload.length; i++) if (back[i] !== payload[i]) { ok = false; break; }
    console.error(`\nRESULT: ${LOOPS}x ${SIZE}B in ${secs}s, maxTxSeq=${maxDataSeq}, round-trip ${ok ? 'OK' : 'CORRUPT'} (${back.length}B)`);
    code = ok ? 0 : 1;
  } catch (e) {
    console.error(`\nRESULT: FAIL ${(e as Error).message}`);
  } finally {
    clearInterval(worker);
    try { client.disconnect(); } catch { /* ignore */ }
    process.exit(code);
  }
})();
