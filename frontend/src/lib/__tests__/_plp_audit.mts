// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Remote Link audit: the REAL PlpClient against the REAL ROM, device by device,
// over the native harness's socket bridge (the same arrangement as _plp_repro).
//
// One pass per device:
//   connect → drive list → directory read → 40 KB upload → download + byte
//   compare → delete → close the session the way the dialog does → open a
//   SECOND session (a re-plug on the passive / SIBO devices, an adoption of the
//   parked session on the R5 ones) → connect → drive list.
// and prints one OK/FAIL line per step with its wall time. The second session is
// the interesting one: most of the device-specific failures this tool found
// (3-bit receive window, unannounced NCP Info after a re-handshake, host
// acknowledging every duplicate) only appear there or in the transfer.
//
// Usage (needs harness/run built and the ROMs present):
//   node --experimental-strip-types frontend/src/lib/__tests__/_plp_audit.mts <device>
//   devices: revo 5mx 5mxpro mc218 series7 netbook netpad series5 osaris geofox
//            series3c series3mx siena workaboutmx
//   SIZE=n    transfer size in bytes (default 40000)
//   PDU=1     trace every PDU with its first bytes
//   HLOG=1    show the harness's own log
//   REPLUG=s  sim second at which the cable is re-plugged (devices that re-plug)
//   LAG=s     how long after the re-plug the second client starts (default 1.5;
//             the dialog starts it immediately, i.e. ~0)
//   TAG=x     distinguishes the socket path so audits can run in parallel
//   PRINTPARK=1  (R5 devices) instead run the Printer-via-PC <-> Remote Link
//             hand-over: print connect, park, link connect, park, print connect
//
// Each run takes real time (the sim is paced to the wall clock): about 20-130 s.

import * as net from 'node:net';
import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';
import { linkConSeq } from '../deviceMeta.ts';

const device = process.argv[2];
const REPO = new URL('../../../../', import.meta.url).pathname;
const SOCK = `/tmp/psion-audit-${device}-${process.env.TAG ?? "x"}.sock`;
const R = `${REPO}roms/`;
type Cfg = { noPoll?: boolean; rom: string; uart: number; args: string[]; protocol?: 'rfsv16'|'rfsv32'; drive?: string; replugAt?: number; until: number };
const C: Record<string, Cfg> = {
  revo:   { rom: `${R}Revo/Revo_v1.06(390)_eng/Revo_v1.06(390)_eng.bin`, uart: 2, args: ['--serial-attach','2','8'], until: 240 },
  '5mx':  { rom: `${R}Series5mx/5mx_v1.05(260)_eng/5mx_v1.05(260)_eng.bin`, uart: 2, args: ['--serial-attach','2','8'], until: 240 },
  '5mxpro': { rom: `${R}Series5mxPRO/5mxPRO_v1.05(319)_patch_site_eng/5mxPRO_v1.05(319)_patch_eng.bin`, uart: 2, args: ['--serial-attach','2','8'], until: 240 },
  mc218:  { rom: `${R}MC218/MC218_v1.05(259)_eng/MC218_v1.05(259)_eng.bin`, uart: 2, args: ['--serial-attach','2','8'], until: 240 },
  series7:{ rom: `${R}Series7/S7_v1.05(254)_b756_eng/S7_v1.05(254)_b756_eng.bin`, uart: 3, args: ['--boot-seconds','20','--serial-attach','3','0.5'], until: 300 },
  netbook:{ noPoll: true, rom: `${R}netBook/BootLoader/netBook_BL_v011_eng/netBook_BL_v011_eng.bin`, uart: 3, args: ['--boot-seconds','3','--post-attach-seconds','300','--card-path',`${R}netBook/netBook_v1.05(450)_eng/OS.IMG`,'--serial-attach','3','3.5'], until: 300 },
  netpad: { rom: `${R}netPad/netPad_v1.75(247)_eng/Netpad.img`, uart: 3, args: ['--skip-card','--serial-attach','3','0.5'], until: 300 },
  series5:{ rom: `${R}Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin`, uart: 1, args: ['--serial-attach','1','25'], replugAt: 200, until: 300 },
  osaris: { rom: `${R}Osaris/Osaris_v1.02(209)_eng/Osaris_v1.02(209)_eng.bin`, uart: 1, args: ['--serial-attach','1','8'], replugAt: 200, until: 300 },
  geofox: { rom: `${R}Geofox/Geofox_v1.01(146)_eng/Geofox_v1.01(146)_eng.bin`, uart: 1, args: ['--serial-attach','1','20'], replugAt: 200, until: 300 },
  series3c: { rom: `${R}Series3c/oak_v5.20f_eng/oak_v5.20f_eng.bin`, uart: 0, protocol: 'rfsv16', drive: 'M:', replugAt: 200, until: 300,
    args: ['--boot-seconds','38','--press-key','8','4','16','--press-key','18','4','16','--press-key','30','20','64','--press-key','30.3','76','16','--press-key','33','15','32','--press-key','35','3','16','--serial-attach','0','36.5'] },
  series3mx: { rom: `${R}Series3mx/maple_v6.16f_eng/maple_v6.16f_eng.bin`, uart: 0, protocol: 'rfsv16', drive: 'M:', replugAt: 200, until: 300,
    args: ['--boot-seconds','38','--press-key','8','4','16','--press-key','18','4','16','--press-key','30','20','64','--press-key','30.3','76','16','--press-key','33','15','32','--press-key','35','3','16','--serial-attach','0','26'] },
  siena: { rom: `${R}Siena/vine_v4.20f_eng/vine_v4.20f_eng.bin`, uart: 0, protocol: 'rfsv16', drive: 'M:', replugAt: 200, until: 300,
    args: ['--boot-seconds','40','--press-key','8','4','16','--press-key','18','4','16','--press-key','28','4','16','--press-key','32','20','64','--press-key','32.3','76','16','--press-key','35','15','32','--press-key','37','3','16','--serial-attach','0','38.5'] },
  workaboutmx: { rom: `${R}WorkaboutMX/w2mx_v7.20f_eng/w2mx_v7.20f_eng.bin`, uart: 0, protocol: 'rfsv16', drive: 'M:', replugAt: 200, until: 300,
    args: ['--boot-seconds','54','--press-key','26','148','32','--press-key','28','17','32','--press-key','30','3','16','--serial-attach','0','31','--press-key','33','20','64','--press-key','33.3','76','16','--press-key','36','15','32','--press-key','38','3','16'] },
};
const cfg = C[device];
if (cfg && process.env.REPLUG && cfg.replugAt) { cfg.replugAt = Number(process.env.REPLUG); }
const LAG = Number(process.env.LAG ?? 1.5);
if (!cfg) { console.error('unknown', device); process.exit(2); }
const SIZE = Number(process.env.SIZE ?? 40000);
const t0 = Date.now();
const log = (...a: unknown[]) => console.error(`[${device} ${((Date.now()-t0)/1000).toFixed(1)}s]`, ...a);
try { fs.unlinkSync(SOCK); } catch {}
let rx: number[] = []; let sock: net.Socket | null = null;
const read = () => { if (!rx.length) return new Uint8Array(0); const o = new Uint8Array(rx); rx = []; return o; };
const write = (d: Uint8Array) => { sock?.write(Buffer.from(d)); return d.length; };
const results: string[] = [];
const step = async <T>(name: string, fn: () => Promise<T>): Promise<T | undefined> => {
  const t = Date.now();
  try { const v = await fn(); results.push(`OK   ${name} (${Date.now()-t}ms)`); log('OK', name, Date.now()-t, 'ms'); return v; }
  catch (e) { results.push(`FAIL ${name} (${Date.now()-t}ms): ${(e as Error).message}`); log('FAIL', name, (e as Error).message); return undefined; }
};
const mk = () => new PlpClient(read, write, { pollHz: 200, protocol: cfg.protocol ?? 'rfsv32', conSeq: linkConSeq(device),
  chunkSize: cfg.uart === 3 ? 2048 : undefined,
  onState: s => log('state', s),
  ...(process.env.PDU ? { onPduIn: p => log('  IN ', p.cont, p.seq, p.data.length, Buffer.from(p.data.subarray(0,24)).toString('hex')), onPduOut: p => log('  OUT', p.cont, p.seq, p.data.length, Buffer.from(p.data.subarray(0,24)).toString('hex')) } : {}) });
const done = () => { console.log(`\n=== ${device} ===\n` + results.join('\n')); process.exit(0); };
async function run() {
  const sibo = cfg.protocol === 'rfsv16';
  const drive = cfg.drive ?? 'C:';
  const name = sibo ? `${drive}\\AUDIT.BIN` : `${drive}\\audit test.bin`;
  const data = new Uint8Array(SIZE); for (let i = 0; i < SIZE; i++) data[i] = (i * 131 + (i >> 8) * 7) & 0xff;
  if (process.env.PRINTPARK) {
    const p = mk(); p.start();
    await step('connectPrint#1', () => p.connectPrint(20_000));
    p.abortPrintWait();
    await p.releaseQuiesced();           // what the fixed PrinterDialog does on Stop
    await new Promise(r => setTimeout(r, 3000));
    const q = mk(); q.start();
    await step('RemoteLink connect after print park', () => q.connect(15_000));
    if (q.state === 'connected') await step('drives after print park', async () => log((await q.listDrives()).join(' ')));
    await q.releaseQuiesced();
    const p2 = mk(); p2.start();
    await step('connectPrint#2 after link park', () => p2.connectPrint(20_000));
    p2.abortPrintWait(); await p2.releaseQuiesced();
    return done();
  }
  const a = mk(); a.start();
  await step('connect#1', () => a.connect(90_000));
  if (a.state !== 'connected') return done();
  await step('drives#1', async () => { const d = await a.listDrives(); log(d.join(' ')); return d; });
  await step('dir', async () => { const e = await a.listDirectory(`${drive}\\`); log(e.length, 'entries'); if (!e.length) throw new Error('empty'); });
  await step(`upload ${SIZE}`, () => a.uploadFile(name, data));
  await step('download+verify', async () => { const b = await a.downloadFile(name); if (b.length !== data.length) throw new Error(`len ${b.length}`); for (let i=0;i<b.length;i++) if (b[i]!==data[i]) throw new Error(`mismatch @${i}`); });
  await step('delete', () => a.deleteFile(name));
  // second session
  if (cfg.replugAt) {
    a.disconnect();
    const wait = cfg.replugAt + LAG - (Date.now() - spawnAt) / 1000;
    log('waiting for replug', wait.toFixed(1), 's');
    if (wait > 0) await new Promise(r => setTimeout(r, wait * 1000));
    else results.push('WARN replug already passed');
  } else {
    await a.releaseQuiesced();
    await new Promise(r => setTimeout(r, 5000));
  }
  const b = mk(); b.start();
  await step('connect#2 (15s, as dialog)', () => b.connect(15_000));
  if (b.state === 'connected') await step('drives#2', async () => { const d = await b.listDrives(); log(d.join(' ')); });
  b.disconnect();
  done();
}
let spawnAt = 0;
const server = net.createServer(s => { sock = s; s.on('data', d => { for (const x of d) rx.push(x); }); void run(); });
server.listen(SOCK, () => {
  const extra = cfg.replugAt ? ['--serial-detach', String(cfg.uart), String(cfg.replugAt), '--serial-attach', String(cfg.uart), String(cfg.replugAt + 0.3)] : [];
  spawnAt = Date.now();
  const child = spawn(`${REPO}harness/run`, [cfg.rom, '--device', device, '--quiet-logs', '--serial-bridge-socket', SOCK, ...cfg.args, ...extra,
    ...(cfg.noPoll ? [] : ['--serial-poll-until', String(cfg.until), String(cfg.uart)])], { env: { ...process.env, PSION_REALTIME: '1' }, stdio: ['ignore','ignore', process.env.HLOG ? 'inherit' : 'ignore'] });
  child.on('exit', c => { log('harness exit', c); results.push(`harness exited ${c}`); done(); });
});
setTimeout(() => { results.push('GLOBAL TIMEOUT'); done(); }, (cfg.until + 30) * 1000);
