// Throughput benchmark for the WebAssembly build.
//
// Boots a device for N simulated seconds and reports how much emulated time
// the host managed per wall second. This is the browser-side equivalent of the
// harness's SA1100_THROUGHPUT line, and the only way to know whether a change
// that helps the native build helps the shipped one — they are different
// compilers with different inlining, and the WASM build has historically not
// even had LTO.
//
//   node scripts/wasm-bench.mjs <psion.js> <rom> <deviceId> [simSeconds]
//        [--card <image>] [--post-attach <simSeconds>]
//        [--script <steps.json>] [--shot-dir <dir>]
//
// A plain boot is a poor workload for anything that has to amortise a cost:
// the guest runs each stretch of initialisation once and then idles. --card
// attaches a CF image after the boot window and keeps running, which is closer
// to an application — the same code, over and over.
//
// --script goes further: it drives the guest's own UI, so the timed window can
// be an application actually doing something rather than a desktop waiting for
// input. The file is a JSON array of steps, run in order:
//
//   { "run":  2.0 }                      advance 2 simulated seconds
//   { "card": "path.img", "run": 30 }    insert a CF image, then advance
//   { "tap":  [677, 458] }               pen down, hold, up, settle
//   { "key":  66 }                       an EpocKey code, down then up
//   { "shot": "desktop" }                write <shot-dir>/desktop.pgm
//   { "timer": "start" } ... { "timer": "stop" }    time what is between them
//   { "repeat": 30, "steps": [ ... ] }   run a sub-sequence N times
//   { "measure": 6.0 }                   sugar for start / run 6 / stop
//
// "hold" and "then" override the press and settle times of a tap or key.
// Everything outside the timer is untimed, so boot, mount and the whole launch
// sequence stay out of the number — which is the point: what a user feels is
// the application's steady state, not how long it took to start. Put the input
// INSIDE the timer when the input is the workload (a document being paged
// through is the guest doing layout, not the guest idling).
import { readFileSync, writeFileSync, copyFileSync, mkdirSync } from 'node:fs';
import { createRequire } from 'node:module';
import { resolve } from 'node:path';
import { performance } from 'node:perf_hooks';

const argv = process.argv.slice(2);
const flag = (name) => {
  const i = argv.indexOf(name);
  if (i < 0) return null;
  const v = argv[i + 1];
  argv.splice(i, 2);
  return v;
};
const cardPath = flag('--card');
const postAttachSeconds = Number(flag('--post-attach') ?? 0);
const scriptPath = flag('--script');
const shotDir = flag('--shot-dir') ?? '/tmp';
const [modPath, romPath, deviceId, secsArg] = argv;
const simSeconds = Number(secsArg ?? 8);
// The emscripten output is CommonJS, but the repo is "type": "module", so a
// .js file next to it is treated as ESM and require() hands back an empty
// namespace. Copy it alongside itself under a .cjs name — same directory, so
// the runtime still finds psion.wasm next to it.
const require = createRequire(import.meta.url);
// Absolute, because require() reads a bare relative path as a module NAME and
// looks it up in node_modules rather than beside the file.
const cjsPath = resolve(modPath.replace(/\.js$/, '') + '.bench.cjs');
copyFileSync(modPath, cjsPath);
const createPsionModule = require(cjsPath);

const rom = new Uint8Array(readFileSync(romPath));
const mod = await createPsionModule();

// Emscripten does not inherit the host environment, so anything the emulator
// reads with getenv has to be handed over explicitly. Forward every PSION_ var
// so a benchmark can be run against an engine the same way the harness is.
for (const [k, v] of Object.entries(process.env))
  if (k.startsWith('PSION_')) mod.setEnvVar(k, v);

const ptr = mod.prepareROMUpload(rom.length);
mod.HEAPU8.set(rom, ptr);
const name = mod.loadBufferedROM(rom.length, deviceId ?? '');
if (!name) { console.error('device not recognised'); process.exit(1); }

const clockHz = mod.getClockHz();

// ── Guest driving ──────────────────────────────────────────────────────────
const advance = (seconds) => {
  const n = Math.round(seconds * 64);
  for (let i = 0; i < n; i++) mod.stepFrameFull();
};
const lcdSize = () => {
  const info = mod.getDeviceInfo();
  return [info.lcdWidth ?? info.screenWidth ?? 0, info.lcdHeight ?? info.screenHeight ?? 0];
};
// A PGM of the current frame, so a launch sequence can be checked by looking at
// it rather than by hoping the coordinates were right.
const shot = (name) => {
  const [w, h] = lcdSize();
  if (!w || !h) return;
  const ptr = mod._malloc(w * h * 4);
  mod.readLCD(ptr);
  const px = mod.HEAPU8.subarray(ptr, ptr + w * h * 4);
  const gray = Buffer.alloc(w * h);
  for (let i = 0; i < w * h; i++) gray[i] = px[i * 4];
  mod._free(ptr);
  mkdirSync(shotDir, { recursive: true });
  const path = `${shotDir}/${name}.pgm`;
  writeFileSync(path, Buffer.concat([Buffer.from(`P5\n${w} ${h}\n255\n`), gray]));
  console.error(`  shot ${path} (${w}x${h})`);
};
const insertCard = (p) => {
  const card = new Uint8Array(readFileSync(p));
  const cptr = mod.prepareCFImageUpload(card.length);
  mod.HEAPU8.set(card, cptr);
  if (!mod.attachCFImage(card.length)) { console.error('card attach failed'); process.exit(1); }
};

let scriptedWall = 0, scriptedSeconds = 0;
// Simulated time and wall time both accumulate only between timer start and
// stop, so the reported ratio is the guest's steady state and nothing else.
let timerFrom = null, timerSimFrom = 0, simClock = 0;
const advanceTimed = (seconds) => { advance(seconds); simClock += seconds; };
const runSteps = (steps) => {
  for (const step of steps) {
    if (step.card) { console.error(`  card ${step.card}`); insertCard(step.card); }
    if (step.tap) {
      const [x, y] = step.tap;
      console.error(`  tap ${x},${y}`);
      mod.sendTouch(x, y, true);  advanceTimed(step.hold ?? 0.12);
      mod.sendTouch(x, y, false); advanceTimed(step.then ?? 0.6);
    }
    if (step.key !== undefined) {
      mod.sendKey(step.key, true);  advanceTimed(step.hold ?? 0.08);
      mod.sendKey(step.key, false); advanceTimed(step.then ?? 0.7);
    }
    if (step.run) advanceTimed(step.run);
    if (step.timer === 'start') { timerFrom = performance.now(); timerSimFrom = simClock; }
    if (step.timer === 'stop' && timerFrom !== null) {
      scriptedWall += (performance.now() - timerFrom) / 1000;
      scriptedSeconds += simClock - timerSimFrom;
      timerFrom = null;
    }
    if (step.measure) {
      const t = performance.now();
      advanceTimed(step.measure);
      scriptedWall += (performance.now() - t) / 1000;
      scriptedSeconds += step.measure;
    }
    if (step.repeat) for (let i = 0; i < step.repeat; i++) runSteps(step.steps ?? []);
    if (step.shot) shot(step.shot);
  }
};
if (scriptPath) runSteps(JSON.parse(readFileSync(scriptPath, 'utf8')));

const frames = scriptPath ? 0 : Math.round(simSeconds * 64);
const t0 = performance.now();
for (let i = 0; i < frames; i++) mod.stepFrameFull();

// The card goes in after the boot window, and the timed window is what runs
// AFTER it — the application, not the boot.
let wall = (performance.now() - t0) / 1000;
let timedSeconds = simSeconds;
if (scriptPath) { wall = scriptedWall; timedSeconds = scriptedSeconds; }
if (cardPath) {
  const card = new Uint8Array(readFileSync(cardPath));
  const cptr = mod.prepareCFImageUpload(card.length);
  mod.HEAPU8.set(card, cptr);
  if (!mod.attachCFImage(card.length)) { console.error('card attach failed'); process.exit(1); }
  const t1 = performance.now();
  const postFrames = Math.round(postAttachSeconds * 64);
  for (let i = 0; i < postFrames; i++) mod.stepFrameFull();
  wall = (performance.now() - t1) / 1000;
  timedSeconds = postAttachSeconds;
}

// Framebuffer fingerprint: the end-to-end check that two engines produced the
// same guest. Register-level self-checks prove each region right in isolation;
// this proves the whole run landed in the same place.
if (mod.readLCD && mod.getDeviceInfo) {
  const info = mod.getDeviceInfo();
  const w = info.lcdWidth ?? info.screenWidth ?? 0, h = info.lcdHeight ?? info.screenHeight ?? 0;
  if (w > 0 && h > 0) {
    const ptr = mod._malloc(w * h * 4);
    mod.readLCD(ptr);
    let hsh = 2166136261 >>> 0;
    const px = mod.HEAPU8.subarray(ptr, ptr + w * h * 4);
    for (let i = 0; i < px.length; i++) { hsh = (hsh ^ px[i]) >>> 0; hsh = Math.imul(hsh, 16777619) >>> 0; }
    mod._free(ptr);
    console.log(`FRAMEBUFFER: ${w}x${h} fnv=${hsh.toString(16).padStart(8, '0')}`);
  }
}
if (mod.dumpJitStats) mod.dumpJitStats();
// Instructions actually executed. Sim time is fixed by the frame count, so a
// run that retires far fewer instructions for the same sim time is not faster —
// it is idling, which is what a divergence looks like from out here.
if (mod.getInsnCount) console.log(`EXECUTED_INSNS: ${mod.getInsnCount()}`);
const simCycles = mod.getSimCycles();
// With a card attached, only the post-attach window was timed, so the ratio has
// to be against that window rather than against everything simulated.
const simSec = (cardPath || scriptPath) ? timedSeconds : simCycles / clockHz;
console.log(
  `WASM_THROUGHPUT: device=${name} sim_s=${simSec.toFixed(2)} wall_s=${wall.toFixed(2)} ` +
  `realtime_MHz=${(clockHz / 1e6).toFixed(1)} ` +
  `sim_realtime_ratio=${(simSec / wall).toFixed(3)}x`);
