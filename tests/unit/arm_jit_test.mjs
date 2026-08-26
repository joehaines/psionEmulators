// Runs the WASM half of the ARM region-compiler differential test.
//
// tests/unit/arm_jit_test (the C++ half) writes a case file containing, per
// case, the guest page, the pre-state, a TIMELINE of the interpreter's full
// architectural state after each tick, and the generated WASM module. This
// script replays each module from the same pre-state and compares against the
// timeline entry at the tick count the region reports.
//
// Bridged instructions are serviced by a stub installed in the imported table.
// The stub does two jobs: it applies the interpreter's recorded effect for that
// instruction, and — the part that actually tests anything — it asserts that
// the state the generated code handed it is exactly the state the interpreter
// saw at the same point.
//
// Usage: node tests/unit/arm_jit_test.mjs <cases.bin>
import { readFileSync } from 'node:fs';

const path = process.argv[2];
if (!path) { console.error('usage: arm_jit_test.mjs <cases.bin>'); process.exit(2); }
const buf = readFileSync(path);
const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);

const PAGE_BASE = 0x00021000, PAGE_WORDS = 64;
const GPR_BASE = 256;
const CPSR_ADDR = GPR_BASE + 64;
const PREFETCH1 = CPSR_ADDR + 4, PREFETCH0 = CPSR_ADDR + 8, PREFETCH_COUNT = CPSR_ADDR + 12;
const PENDING_IRQ = CPSR_ADDR + 16, PENDING_FIQ = CPSR_ADDR + 17;
const FAULT0 = CPSR_ADDR + 48, FAULT1 = CPSR_ADDR + 56;
const EXIT_TICKS = CPSR_ADDR + 20;
const INSN_CYCLE = CPSR_ADDR + 24, CYCLE_SINK_PTR = CPSR_ADDR + 32, CYCLE_SINK = CPSR_ADDR + 40;
const BRIDGE_INDEX = 1;
const ENTRY_WORDS = 21;   // 16 GPRs, CPSR, prefetch[1], prefetch[0], count, cycles

let off = 0;
if (dv.getUint32(off, true) !== 0x334A4D41) { console.error('bad magic'); process.exit(2); }
off += 4;
const stubLen = dv.getUint32(off, true); off += 4;
const stubBytes = buf.subarray(off, off + stubLen);
off += stubLen + ((4 - (stubLen % 4)) % 4);
const count = dv.getUint32(off, true); off += 4;

const mem = new WebAssembly.Memory({ initial: 2 });
const u32 = new Uint32Array(mem.buffer);
const u8 = new Uint8Array(mem.buffer);
const table = new WebAssembly.Table({ initial: 4, element: 'anyfunc' });

// Per-case state the bridge stub reads.
let cur = null;
// Every hand-over to the interpreter, across all cases. An inlined load that
// missed its guard shows up here, so comparing this against the number of
// inlined load positions says whether the inline path is actually firing —
// a guard that never hits would pass every case and prove nothing.
let bridgeCallsTotal = 0;
const bridgeStub = () => {
  bridgeCallsTotal++;
  const n = cur.bridgeCall++;
  const idx = cur.bridgeAt[n];
  if (idx === undefined) { cur.bad.push('bridge called more times than the region has bridges'); return 0; }
  const fast = cur.bridgeFast[n];
  // What the interpreter saw going in must be what the generated code handed
  // over. This is the assertion the whole bridge design rests on — and the two
  // bridge forms promise different things, so they are checked differently.
  //
  //   fast: the word is a constant, so only r15 has to be in place, at the
  //         address the burst loop would have left it (instruction + 12).
  //   slow: tick() re-fetches, so r15 is instruction + 8 and the pipeline has
  //         to describe this instruction.
  const before = cur.timeline[idx];
  const after = cur.timeline[idx + 1];
  // The bridged instruction's own address, carried over rather than derived
  // from its position: a region that followed a branch is not contiguous.
  const insnPc = cur.bridgePc[n];
  for (let r = 0; r < 15; r++) {
    if (u32[(GPR_BASE >> 2) + r] !== before[r])
      cur.bad.push(`bridge ${idx}: r${r} handed ${hex(u32[(GPR_BASE >> 2) + r])}, interpreter had ${hex(before[r])}`);
  }
  const wantPc = insnPc + (fast ? 12 : 8);
  if (u32[(GPR_BASE >> 2) + 15] !== wantPc)
    cur.bad.push(`bridge ${idx} (${fast ? 'fast' : 'slow'}): r15 handed ${hex(u32[(GPR_BASE >> 2) + 15])}, expected ${hex(wantPc)}`);
  if (u32[CPSR_ADDR >> 2] !== before[16]) cur.bad.push(`bridge ${idx}: CPSR handed ${hex(u32[CPSR_ADDR >> 2])}, interpreter had ${hex(before[16])}`);
  if (!fast) {
    if (u32[PREFETCH1 >> 2] !== before[17]) cur.bad.push(`bridge ${idx}: prefetch[1] handed ${hex(u32[PREFETCH1 >> 2])}, interpreter had ${hex(before[17])}`);
    if (u32[PREFETCH0 >> 2] !== before[18]) cur.bad.push(`bridge ${idx}: prefetch[0] handed ${hex(u32[PREFETCH0 >> 2])}, interpreter had ${hex(before[18])}`);
    if (u32[PREFETCH_COUNT >> 2] !== before[19]) cur.bad.push(`bridge ${idx}: prefetchCount handed ${u32[PREFETCH_COUNT >> 2]}, interpreter had ${before[19]}`);
  }

  // A bridged STORE writes guest memory, and the stub stands in for the
  // interpreter — so it has to make that write too, or the memory comparison at
  // the end would fault the region for a byte it correctly left to the
  // interpreter.
  for (const [t, a, b] of cur.storeLog) {
    if (t !== idx) continue;
    const h = hostAddr(a);
    if (h >= 0) u8[h] = b;
  }
  // Apply the interpreter's effect for this one instruction. The fast form is
  // ARM710::jitExecOne, which does not touch the pipeline, so neither does the
  // stub standing in for it.
  for (let r = 0; r < 16; r++) u32[(GPR_BASE >> 2) + r] = after[r];
  u32[CPSR_ADDR >> 2] = after[16];
  if (!fast) {
    u32[PREFETCH1 >> 2] = after[17];
    u32[PREFETCH0 >> 2] = after[18];
  }
  u32[PREFETCH_COUNT >> 2] = after[19];
  const cycles = after[20] - before[20];
  // ARM710::jitStepOne advances the device cycle sink itself, so the stub does
  // too — otherwise the region's flush would be the only writer and a real
  // bridge's cycles would go missing from device time.
  addSink(cycles);
  return cycles;
};
// A JS function cannot go into a WASM table directly, so it is wrapped by a
// trampoline module — emitted by the C++ half with the same writer the regions
// use — whose only export is a function of the bridge signature.
const stubModule = new WebAssembly.Module(stubBytes);
// The writer always imports the emulator's memory, so the trampoline gets it
// too even though it never touches it.
const stubInst = new WebAssembly.Instance(stubModule,
  { env: { memory: mem, f: bridgeStub, __indirect_function_table: table } });
table.set(BRIDGE_INDEX, stubInst.exports.trampoline);

function addSink(cycles) {
  const p = u32[CYCLE_SINK_PTR >> 2];
  if (!p) return;
  const lo = u32[p >> 2], hi = u32[(p >> 2) + 1];
  const v = (BigInt(hi) << 32n) | BigInt(lo >>> 0);
  const nv = v + BigInt(cycles);
  u32[p >> 2] = Number(nv & 0xFFFFFFFFn);
  u32[(p >> 2) + 1] = Number((nv >> 32n) & 0xFFFFFFFFn);
}

// ── Synthetic data-side micro-TLB ──────────────────────────────────────────
// The C++ half compiles inlined loads against this; the constants and
// testDataWord() are duplicated from arm_jit_test.cpp on purpose, so a change
// on one side has to be made on the other rather than silently agreeing.
const MRU_BASE = 7168;
const ENT = [7232, 7264, 7296];
const DATA = [8192, 8448, 8704];
const GUEST = [0x00300000, 0x00401000, 0x00502000];
const OFF_ADDRMASK = 0, OFF_ADDR = 4, OFF_LV1 = 8, OFF_LV2 = 12;
const OFF_PERMCACHE = 16, OFF_PERMCACHEPG = 17;
const OFF_HOSTREAD = 24, OFF_HOSTWRITE = 28;
// The decoded-page table an inlined store probes. Filled with the empty-slot
// sentinel, so the probe runs on every store and matches nothing — what is
// being checked here is that it leaves memory alone, not the eviction itself.
const DECODE_PTR = 6144, DECODE_TABLE = 12288, DECODE_SLOTS = 256, DECODE_STRIDE = 8;
const testDataWord = (va) => (0xA5A50000 ^ Math.imul(va, 2654435761)) >>> 0;

function resetGuestData() {
  for (let i = 0; i < 3; i++)
    for (let w = 0; w < 64; w++)
      u32[(DATA[i] >> 2) + w] = testDataWord((GUEST[i] + w * 4) >>> 0);
}

// Guest address -> host address, through the same bias the entries carry.
function hostAddr(guest) {
  for (let i = 0; i < 3; i++) {
    const lo = GUEST[i], hi = GUEST[i] + 256;
    if (guest >= lo && guest < hi) return DATA[i] + (guest - lo);
  }
  return -1;
}

function buildMicroTlb() {
  // 0x11: this (privileged, read) combination has been computed, and it is
  // allowed. 0x00 in a sub-page slot is the "not computed" case, which has to
  // reach the interpreter.
  // 0x33: both (privileged, read) and (privileged, write) computed and allowed.
  // 0 in a sub-page slot is the "not computed" case, which has to reach the
  // interpreter.
  const ent = [
    { lv2: 0,          lv1: 0x00300C00, perm: 0x33, permPg: [0, 0, 0, 0],          tag: 0x00300000 },
    { lv2: 0x00402002, lv1: 0,          perm: 0,    permPg: [0x33, 0, 0x33, 0x33], tag: 0x00400000 },
    { lv2: 0x00500001, lv1: 0,          perm: 0,    permPg: [0x33, 0, 0x33, 0x33], tag: 0x00500000 },
  ];
  for (let i = 0; i < 3; i++) {
    const e = ENT[i];
    u32[(e + OFF_ADDRMASK) >> 2] = 0xFFF00000;
    u32[(e + OFF_ADDR) >> 2] = ent[i].tag;
    u32[(e + OFF_LV2) >> 2] = ent[i].lv2;
    u8[e + OFF_PERMCACHE] = ent[i].perm;
    for (let k = 0; k < 4; k++) u8[e + OFF_PERMCACHEPG + k] = ent[i].permPg[k];
    // Biased so that the pointer plus the guest address is the host address.
    u32[(e + OFF_HOSTREAD) >> 2] = (DATA[i] - GUEST[i]) >>> 0;
    u32[(e + OFF_HOSTWRITE) >> 2] = (DATA[i] - GUEST[i]) >>> 0;
    u32[(e + OFF_LV1) >> 2] = ent[i].lv1;
    for (let w = 0; w < 64; w++)
      u32[(DATA[i] >> 2) + w] = testDataWord((GUEST[i] + w * 4) >>> 0);
  }
  // Ways are indexed on address bits 12+. Way 7 is left empty so a load that
  // lands there has to go to the interpreter.
  for (let w = 0; w < 8; w++) u32[(MRU_BASE >> 2) + w] = 0;
  u32[(MRU_BASE >> 2) + ((GUEST[0] >>> 12) & 7)] = ENT[0];
  u32[(MRU_BASE >> 2) + ((GUEST[1] >>> 12) & 7)] = ENT[1];
  u32[(MRU_BASE >> 2) + ((GUEST[2] >>> 12) & 7)] = ENT[2];
  u32[(MRU_BASE >> 2) + (((GUEST[2] + 0x4000) >>> 12) & 7)] = ENT[2];
  u32[DECODE_PTR >> 2] = DECODE_TABLE;
  for (let k = 0; k < DECODE_SLOTS; k++)
    u32[(DECODE_TABLE + k * DECODE_STRIDE) >> 2] = 0xFFFFFFFF;
}
buildMicroTlb();

const imports = { env: { memory: mem, __indirect_function_table: table } };
const moduleCache = new Map();

let pass = 0;
const failures = [];

for (let i = 0; i < count; i++) {
  const bodyLen = dv.getUint32(off, true); off += 4;
  const page = []; for (let w = 0; w < PAGE_WORDS; w++) { page.push(dv.getUint32(off, true)); off += 4; }
  const pre = []; for (let r = 0; r < 16; r++) { pre.push(dv.getUint32(off, true)); off += 4; }
  const preCpsr = dv.getUint32(off, true); off += 4;
  const maxCycles = dv.getUint32(off, true); off += 4;
  const nInline = dv.getUint32(off, true); off += 4;
  const nBridged = dv.getUint32(off, true); off += 4;
  const nSpan = dv.getUint32(off, true); off += 4;
  const loops = dv.getUint32(off, true); off += 4;
  const nBridgeAt = dv.getUint32(off, true); off += 4;
  const bridgeAt = [], bridgeFast = [], bridgePc = [];
  for (let b = 0; b < nBridgeAt; b++) {
    bridgeAt.push(dv.getUint32(off, true)); off += 4;
    bridgeFast.push(dv.getUint32(off, true) !== 0); off += 4;
    bridgePc.push(dv.getUint32(off, true)); off += 4;
  }
  const nStores = dv.getUint32(off, true); off += 4;
  const storeLog = [];
  for (let k = 0; k < nStores; k++) {
    const t = dv.getUint32(off, true); off += 4;
    const a = dv.getUint32(off, true); off += 4;
    const b = dv.getUint32(off, true); off += 4;
    storeLog.push([t, a, b]);
  }
  const tlLen = dv.getUint32(off, true); off += 4;
  const timeline = [];
  for (let t = 0; t < tlLen; t++) {
    const e = []; for (let w = 0; w < ENTRY_WORDS; w++) { e.push(dv.getUint32(off, true)); off += 4; }
    timeline.push(e);
  }
  const modLen = dv.getUint32(off, true); off += 4;
  const modBytes = buf.subarray(off, off + modLen);
  off += modLen + ((4 - (modLen % 4)) % 4);

  const key = page.slice(0, Math.max(bodyLen, 1)).join(',') + '|' + nSpan + '|' + maxCycles;
  let inst = moduleCache.get(key);
  if (!inst) {
    if (!WebAssembly.validate(modBytes)) {
      failures.push({ page, bodyLen, preCpsr, pre, why: ['module failed WebAssembly.validate'] });
      continue;
    }
    inst = new WebAssembly.Instance(new WebAssembly.Module(modBytes), imports);
    moduleCache.set(key, inst);
  }

  // Lay the guest page into memory so a bridged fetch reads the same words.
  for (let w = 0; w < PAGE_WORDS; w++) u32[(PAGE_BASE >> 2) + w] = page[w];
  for (let r = 0; r < 16; r++) u32[(GPR_BASE >> 2) + r] = pre[r];
  u32[CPSR_ADDR >> 2] = preCpsr;
  u32[PREFETCH1 >> 2] = timeline[0][17];
  u32[PREFETCH0 >> 2] = timeline[0][18];
  u32[PREFETCH_COUNT >> 2] = timeline[0][19];
  // Both fault slots start dirty, so a region that forgets to clear them beside
  // the words it writes fails rather than passing by luck.
  u32[FAULT0 >> 2] = 0xDEADBEEF; u32[(FAULT0 >> 2) + 1] = 0xDEADBEEF;
  u32[FAULT1 >> 2] = 0xDEADBEEF; u32[(FAULT1 >> 2) + 1] = 0xDEADBEEF;
  u8[PENDING_IRQ] = 0; u8[PENDING_FIQ] = 0;
  u32[EXIT_TICKS >> 2] = 0xFFFFFFFF;
  u32[INSN_CYCLE >> 2] = 0; u32[(INSN_CYCLE >> 2) + 1] = 0;
  u32[CYCLE_SINK_PTR >> 2] = CYCLE_SINK;
  u32[CYCLE_SINK >> 2] = 0; u32[(CYCLE_SINK >> 2) + 1] = 0;

  // The guest data areas start every case as the pattern both halves agree on,
  // so a store's effect is the only difference between them afterwards.
  resetGuestData();
  cur = { timeline, bridgeAt, bridgeFast, bridgePc, storeLog, bridgeCall: 0, bad: [] };
  // maxTicks is deliberately generous here: the tick cap exists to keep the
  // caller's per-batch hook cadence, and the cycle budget is what bounds a pass.
  const cycles = inst.exports.blk0(maxCycles, 1 << 20) >>> 0;
  const ticks = u32[EXIT_TICKS >> 2];
  const bad = cur.bad;

  if (ticks >= timeline.length) {
    bad.push(`region reported ${ticks} ticks, beyond the ${timeline.length - 1}-tick timeline`);
  } else {
    const want = timeline[ticks];
    for (let r = 0; r < 16; r++) {
      const got = u32[(GPR_BASE >> 2) + r];
      if (got !== want[r]) bad.push(`r${r}: interpreter ${hex(want[r])} vs jit ${hex(got)}`);
    }
    const gotCpsr = u32[CPSR_ADDR >> 2];
    if (gotCpsr !== want[16]) bad.push(`CPSR: interpreter ${hex(want[16])} (${nzcv(want[16])}) vs jit ${hex(gotCpsr)} (${nzcv(gotCpsr)})`);
    // A prefetch slot is only live while the count says so: at count 0 the
    // interpreter refills both before reading either, so their contents are
    // dead and a region is free to leave whatever was there.
    if (want[19] !== 0) {
      if (u32[PREFETCH1 >> 2] !== want[17]) bad.push(`prefetch[1]: interpreter ${hex(want[17])} vs jit ${hex(u32[PREFETCH1 >> 2])}`);
      if (u32[PREFETCH0 >> 2] !== want[18]) bad.push(`prefetch[0]: interpreter ${hex(want[18])} vs jit ${hex(u32[PREFETCH0 >> 2])}`);
    }
    if (u32[PREFETCH_COUNT >> 2] !== want[19]) bad.push(`prefetchCount: interpreter ${want[19]} vs jit ${u32[PREFETCH_COUNT >> 2]}`);
    // Same liveness rule as the words above: at count 0 the interpreter
    // rewrites both fault slots before reading either.
    if (want[19] !== 0)
      for (const [n, a] of [['prefetchFaults[0]', FAULT0], ['prefetchFaults[1]', FAULT1]])
        if (u32[a >> 2] !== 0 || u32[(a >> 2) + 1] !== 0)
          bad.push(`${n}: left dirty (${hex(u32[(a >> 2) + 1])}${hex(u32[a >> 2]).slice(2)}) — a stale fault beside a fresh word is a spurious prefetch abort`);
    if (cycles !== want[20]) bad.push(`cycles: interpreter ${want[20]} vs jit ${cycles} (at ${ticks} ticks)`);
    // Device time and the instruction counter must have received exactly the
    // cycles the region reports — batching them is only sound if nothing goes
    // missing.
    // Guest memory. Every byte the interpreter wrote within the region's tick
    // count has to be there, and nothing else may have moved — an inlined store
    // that writes the right value to the wrong address fails here and nowhere
    // else.
    const want8 = new Map();
    for (const [t, a, b] of storeLog) if (t < ticks) want8.set(a, b);
    for (let i = 0; i < 3; i++) {
      for (let k = 0; k < 256; k++) {
        const guest = (GUEST[i] + k) >>> 0;
        const expect = want8.has(guest)
          ? want8.get(guest)
          : (testDataWord(guest & ~3) >>> (8 * (guest & 3))) & 0xFF;
        const got = u8[DATA[i] + k];
        if (got !== expect) {
          bad.push(`mem[${hex(guest)}]: interpreter ${hex(expect)} vs jit ${hex(got)}`);
          break;
        }
      }
    }
    const sink = u32[CYCLE_SINK >> 2];
    if (sink !== cycles) bad.push(`cycle sink: ${sink}, region returned ${cycles}`);
    const ic = u32[INSN_CYCLE >> 2];
    const bridgedCycles = bridgeAt.slice(0, cur.bridgeCall)
      .reduce((a, idx) => a + (timeline[idx + 1][20] - timeline[idx][20]), 0);
    if (ic !== cycles - bridgedCycles)
      bad.push(`insnCycleApprox: ${ic}, expected ${cycles - bridgedCycles} (bridged cycles are tick()'s own)`);
  }

  if (bad.length === 0) pass++;
  else failures.push({ page, bodyLen, preCpsr, pre, nSpan, nBridged, loops, why: bad });
}

function hex(v) { return '0x' + (v >>> 0).toString(16).padStart(8, '0'); }
function nzcv(c) { return ['N', 'Z', 'C', 'V'].map((f, i) => ((c >>> (31 - i)) & 1) ? f : '-').join(''); }

console.log(`arm_jit: ${pass}/${count} cases match the interpreter`);
console.log(`arm_jit: ${bridgeCallsTotal} hand-overs to the interpreter`);
if (failures.length) {
  const byBody = new Map();
  for (const f of failures) {
    const k = f.page.slice(0, f.bodyLen).map(hex).join(' ');
    if (!byBody.has(k)) byBody.set(k, { n: 0, sample: f });
    byBody.get(k).n++;
  }
  console.log(`\n${failures.length} failures across ${byBody.size} distinct region bodies:\n`);
  let shown = 0;
  for (const [body, g] of byBody) {
    if (shown++ >= 10) { console.log(`  … and ${byBody.size - 10} more bodies`); break; }
    console.log(`  [${body}]  (${g.n} seeds)  span=${g.sample.nSpan} bridged=${g.sample.nBridged} loops=${g.sample.loops}`);
    console.log(`     preCPSR ${nzcv(g.sample.preCpsr)}  r1=${hex(g.sample.pre[1])} r2=${hex(g.sample.pre[2])}`);
    for (const w of g.sample.why.slice(0, 6)) console.log(`     ${w}`);
  }
  process.exit(1);
}
