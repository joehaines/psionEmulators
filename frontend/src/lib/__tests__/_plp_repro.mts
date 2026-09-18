// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Local repro harness for the PLP drive-list path on SA-1100 devices.
//
// Runs the REAL PlpClient against the REAL device ROM via the native
// emulator harness, relaying UART3 bytes over a unix socket
// (harness --serial-bridge-socket). This drives the full link + NCP +
// RFSV handshake locally, without a browser round-trip — the only way
// (besides the browser) to exercise GET_DRIVE_LIST against the actual
// EPOC ROM. See docs/series7-plp-drivelist-2026-06-06.md.
//
// Result (2026-06-06): the stack WORKS end-to-end — the device returns
// the drive list ("C: D: ... Z:") — but the reply lands ~5-6 SIM-seconds
// after the request (vs milliseconds on real hardware). That latency is
// the EKA1 F32-server wake (RequestComplete routing) issue; the native
// run still succeeds because RfsvClient's retry catches the slow answer.
//
// Usage:
//   node --experimental-strip-types frontend/src/lib/__tests__/_plp_repro.mts [series7|netbook]
//   NO_REALTIME=1 …   run the sim as fast as possible (default: realtime)
//   POLLHZ=5 …        override the PlpClient poll rate
//
// Requires the harness built (harness/build.sh) and the ROMs present.

import * as net from 'node:net';
import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';
import { linkConSeq } from '../deviceMeta.ts';
import type { Pdu } from '../plp/link.ts';
import { IrReceiveClient } from '../irda/receive.ts';
import { isJobSentinelPage } from '../plp/wprt-spec.ts';
import { parsePage, extractPageText, renderJobToPdf } from '../printer/wprt-render.ts';

const device = process.argv[2] ?? 'series7';
const REPO = new URL('../../../../', import.meta.url).pathname;
const SOCK = `/tmp/psion-plp-${device}.sock`;
const HARNESS = `${REPO}harness/run`;

const ROMS: Record<string, { rom: string; args: string[]; protocol?: 'rfsv16' | 'rfsv32'; beam?: boolean; irrecv?: boolean; print?: boolean }> = {
  series7: {
    rom: `${REPO}roms/series7_v1.05(254)_b756_eng.bin`,
    args: ['--boot-seconds', '20', '--serial-attach', '3', '0.5',
           '--serial-poll-until', '60', '3',
           // Optional: inject dense periodic taps to keep WServ continuously
           // busy (mimic the browser, where the live screen + faithful-touch
           // AtoD heartbeat compete with the RemoteLinkServer serial thread).
           ...(process.env.WSERV_LOAD
               ? Array.from({ length: 60 }, (_, i) =>
                   ['--tap-seq', '320', '6', String(7 + i * 0.3)]).flat()
               : [])],
  },
  netbook: {
    rom: `${REPO}roms/netBook_BL_v011_eng.bin`,
    args: ['--boot-seconds', '3', '--post-attach-seconds', '60',
           '--card-path', `${REPO}roms/netbook_os.img`,
           '--serial-attach', '3', '3.5',
           '--serial-poll-until', '60', '3'],
  },
  // Series 3c (SIBO / EPOC16): boots to the System screen, dismisses the
  // cold-boot dialogs (Esc x2), opens the Communications dialog with
  // Psion+L, RightArrow to "Link cable" and Enter to start the link
  // server, then attaches the host bridge on Condor uartIndex 0.
  series3c: {
    rom: `${REPO}roms/series3c_v5.20f_eng.bin`,
    protocol: 'rfsv16',
    args: ['--boot-seconds', '38',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--press-key', '30', '20', '64', '--press-key', '30.3', '76', '16',
           '--press-key', '33', '15', '32', '--press-key', '35', '3', '16',
           '--serial-attach', '0', '36.5',
           // HANG_PROBE: after the PLP phase, poke the device UI (Pson+L
           // opens Communications if the kernel is alive) and screenshot.
           ...(process.env.HANG_PROBE
               ? ['--press-key', '70', '20', '64', '--press-key', '70.3', '76', '16',
                  '--screenshot', '/tmp/s3c-hangprobe.pgm']
               : []),
           '--serial-poll-until', '120', '0'],
  },
  series3mx: {
    rom: `${REPO}roms/series3mx_v6.16f_eng.bin`,
    protocol: 'rfsv16',
    args: ['--boot-seconds', '38',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--press-key', '30', '20', '64', '--press-key', '30.3', '76', '16',
           '--press-key', '33', '15', '32', '--press-key', '35', '3', '16',
           '--serial-attach', '0', '36.5',
           '--serial-poll-until', '120', '0'],
  },
  // Workabout boot menu: Menu -> System screen -> Enter, then the
  // standard Psion+L Remote link dialog (Right toggles Off -> On).
  workaboutmx: {
    rom: `${REPO}roms/workaboutMX_v7.20f_eng.bin`,
    protocol: 'rfsv16',
    args: ['--boot-seconds', '40',
           '--press-key', '26', '148', '32', '--press-key', '28', '17', '32',
           '--press-key', '30', '3', '16',
           '--press-key', '33', '20', '64', '--press-key', '33.3', '76', '16',
           '--press-key', '36', '15', '32', '--press-key', '38', '3', '16',
           '--serial-attach', '0', '31',
           '--serial-poll-until', '120', '0'],
  },
  // Beam-capture flow: connect over the cable link, upload a fresh
  // Word file (so something on the System screen is NOT in-use), then
  // the key script flips Use back to "Psion IR", highlights the new
  // file and triggers Infrared send. The harness's --serial-capture
  // file ends up holding the device's beam-protocol opening bytes.
  // Device→host beam: close every running app (Shift+Psion+A, Y) so
  // the default Data file is beamable, then Infrared-send it
  // (Psion+Tab, Up). The node side runs the host IrDA responder and
  // receives the file. Fully deterministic — no link phase, so no
  // "Updating lists" rescan races.
  'series3c-beam': {
    rom: `${REPO}roms/series3c_v5.20f_eng.bin`,
    beam: true,
    args: ['--boot-seconds', '24',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--serial-attach', '0', '20',
           '--press-key', '26', '18', '64', '--press-key', '26.1', '20', '64',
           '--press-key', '26.4', '65', '16',
           '--press-key', '30', '89', '16',
           '--press-key', '36', '20', '64', '--press-key', '36.3', '2', '16',
           '--press-key', '39', '16', '32',
           '--serial-capture', '/tmp/s3c-beam-capture.bin',
           '--screenshot', '/tmp/s3c-beam-final.pgm',
           '--serial-poll-until', '75', '0'],
  },
  'series3c-irrecv': {
    rom: `${REPO}roms/series3c_v5.20f_eng.bin`,
    irrecv: true,
    args: ['--boot-seconds', '38',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--press-key', '30', '20', '64', '--press-key', '30.3', '2', '16',
           '--press-key', '33', '17', '32',
           '--serial-attach', '0', '26',
           '--serial-capture', '/tmp/s3c-irrecv-capture.bin',
           '--screenshot', '/tmp/s3c-irrecv-final.pgm',
           '--serial-poll-until', '95', '0'],
  },
  // 3mx infrared rides the ASIC9MX's second integrated UART ("UART0",
  // I/O 0x40-0x4E) — host bridge uartIndex 1 — with the same Psion+Tab
  // Infrared screen and 136-byte beam dialect as the 3c.
  'series3mx-irrecv': {
    rom: `${REPO}roms/series3mx_v6.16f_eng.bin`,
    irrecv: true,
    args: ['--boot-seconds', '38',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--press-key', '30', '20', '64', '--press-key', '30.3', '2', '16',
           '--press-key', '33', '17', '32',
           '--serial-attach', '1', '26',
           '--serial-capture', '/tmp/s3mx-irrecv-capture.bin',
           '--screenshot', '/tmp/s3mx-irrecv-final.pgm',
           ...(process.env.SHOT_EVERY ? ['--screenshot-every', '5', '/tmp/s3mx-irt'] : []),
           '--serial-poll-until', '95', '1'],
  },
  'series3mx-beam': {
    rom: `${REPO}roms/series3mx_v6.16f_eng.bin`,
    beam: true,
    args: ['--boot-seconds', '24',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--serial-attach', '1', '20',
           '--press-key', '26', '18', '64', '--press-key', '26.1', '20', '64',
           '--press-key', '26.4', '65', '16',
           '--press-key', '30', '89', '16',
           '--press-key', '36', '20', '64', '--press-key', '36.3', '2', '16',
           '--press-key', '39', '16', '32',
           '--serial-capture', '/tmp/s3mx-beam-capture.bin',
           '--screenshot', '/tmp/s3mx-beam-final.pgm',
           '--serial-poll-until', '75', '1'],
  },
  // Device→host beam on the Siena: exit all apps, highlight the Data
  // file, dedicated IR Send key (PageUp / EpocKey 10).
  'siena-beam': {
    rom: `${REPO}roms/siena_v4.20f_eng.bin`,
    beam: true,
    args: ['--boot-seconds', '24',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--press-key', '28', '4', '16',
           '--serial-attach', '0', '20',
           '--press-key', '32', '18', '64', '--press-key', '32.1', '20', '64',
           '--press-key', '32.4', '65', '16',
           '--press-key', '36', '89', '16',
           '--press-key', '42', '10', '32',
           '--serial-capture', '/tmp/siena-beam-capture.bin',
           '--screenshot', '/tmp/siena-beam-final.pgm',
           '--serial-poll-until', '80', '0'],
  },
  // Siena receiver probe: its dedicated IR Receive key (PageDown /
  // EpocKey 11) arms the receive screen directly.
  'siena-irrecv': {
    rom: `${REPO}roms/siena_v4.20f_eng.bin`,
    irrecv: true,
    args: ['--boot-seconds', '36',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--press-key', '28', '4', '16',
           '--press-key', '32', '11', '32',
           '--serial-attach', '0', '20',
           '--serial-capture', '/tmp/siena-irrecv-capture.bin',
           '--screenshot', '/tmp/siena-irrecv-final.pgm',
           ...(process.env.SHOT_EVERY ? ['--screenshot-every', '1', '/tmp/siena-irrecv-t'] : []),
           '--serial-poll-until', '90', '0'],
  },
  siena: {
    rom: `${REPO}roms/siena_v4.20f_eng.bin`,
    protocol: 'rfsv16',
    args: ['--boot-seconds', '40',
           '--press-key', '8', '4', '16', '--press-key', '18', '4', '16',
           '--press-key', '28', '4', '16',
           '--press-key', '32', '20', '64', '--press-key', '32.3', '76', '16',
           '--press-key', '35', '15', '32', '--press-key', '37', '3', '16',
           '--serial-attach', '0', '38.5',
           '--serial-poll-until', '120', '0'],
  },
  // CL-PS711x devices: cable on UART1, ER3/ER4 link flavour (conSeq 2,
  // passive — see client-spec.ts ensureLinkUp). Used to profile the slow
  // RFSV directory/file reads reported in the browser.
  series5: {
    rom: `${REPO}roms/series5_v1.01(144)_eng.bin`,
    args: ['--serial-attach', '1', '25', '--serial-poll-until', '180', '1'],
  },
  osaris: {
    rom: `${REPO}roms/Osaris_v1.02(209)_eng.bin`,
    args: ['--serial-attach', '1', '8', '--serial-poll-until', '160', '1'],
  },
  // Geofox One: cable on UART1 like the Series 5, and it ships with
  // Remote Link already set to Cable at 115200, so there is no key
  // script — plugging the bridge in is the whole setup. Attaching at 20 s
  // is deliberately well after the machine has settled, which is the
  // order a user connects in.
  geofox: {
    rom: `${REPO}roms/Geofox_v1.01(146)_eng.bin`,
    args: ['--serial-attach', '1', '20', '--serial-poll-until', '180', '1'],
  },
  // Windermere baseline for A/B latency comparison.
  '5mx': {
    rom: `${REPO}roms/5mx_v1.05(260)_eng.bin`,
    args: ['--serial-attach', '2', '8', '--serial-poll-until', '160', '2'],
  },
  // Printer via PC, end-to-end: connect the WPRT print client over the
  // cable, then key-script the device to create a Word document
  // (Ctrl+N from the System screen, Enter to accept the dialog), type
  // "hi", and print it (Ctrl+P, Enter — the printer defaults to
  // "Printer via PC"). The node side pulls the job over SYS$WPRT and
  // renders it to /tmp/5mx-print.pdf. Screenshots before/after the
  // print keystroke pin down where the script is if anything drifts.
  '5mx-print': {
    rom: `${REPO}roms/5mx_v1.05(260)_eng.bin`,
    print: true,
    args: ['--serial-attach', '2', '8',
           // Generous gaps: on first boot the System screen can take
           // >20s to service the first keystroke (observed via the
           // periodic screenshots), and Word(01) takes a few seconds
           // to launch after the Create-new-file dialog is confirmed.
           '--press-key', '18',   '22', '64',   // Ctrl …
           '--press-key', '18.3', '78', '16',   // … + N  → New file dialog
           '--press-key', '32',   '3',  '16',   // Enter  → create + open Word
           '--press-key', '42',   '72', '16',   // h
           '--press-key', '42.6', '73', '16',   // i
           '--screenshot-every', '15', '/tmp/5mx-print-t',
           '--press-key', '48',   '22', '64',   // Ctrl …
           '--press-key', '48.3', '80', '16',   // … + P  → Print dialog
           '--press-key', '53',   '3',  '16',   // Enter  → Print
           '--screenshot', '/tmp/5mx-print-final.pgm',
           '--serial-poll-until', '150', '2'],
  },
};

const cfg = ROMS[device];
if (!cfg) { console.error(`unknown device ${device}`); process.exit(2); }
// Synthetic targets (e.g. series3c-beam) map onto a real harness device id.
const harnessDevice = device.split('-')[0];

try { fs.unlinkSync(SOCK); } catch { /* not there */ }

const log = (...a: unknown[]) => console.error(`[repro ${((Date.now() - t0) / 1000).toFixed(2)}s]`, ...a);
const t0 = Date.now();

// Byte plumbing between the socket and the PlpClient.
let rxBuf: number[] = [];          // bytes from device → client
let sock: net.Socket | null = null;

const server = net.createServer(s => {
  sock = s;
  log('harness connected to relay socket');
  s.on('data', d => { for (const b of d) rxBuf.push(b); });
  s.on('close', () => log('socket closed'));
  s.on('error', e => log('socket error', (e as Error).message));
  void runClient();
});
server.listen(SOCK, () => {
  log(`listening on ${SOCK}; spawning harness for ${device}`);
  const quiet = process.env.VERBOSE ? [] : ['--quiet-logs'];
  const child = spawn(HARNESS, [cfg.rom, '--device', harnessDevice, ...quiet,
                                '--serial-bridge-socket', SOCK, ...cfg.args],
                      { env: { ...process.env, ...(process.env.NO_REALTIME ? {} : { PSION_REALTIME: '1' }) }, stdio: ['ignore', 'ignore', 'inherit'] });
  child.on('exit', code => { log(`harness exited (${code})`); process.exit(code ?? 0); });
});

function pduStr(dir: string, p: Pdu): string {
  const prev = Array.from(p.data.subarray(0, 12)).map(b => b.toString(16).padStart(2, '0')).join(' ');
  return `${dir} cont=${p.cont} seq=${p.seq} len=${p.data.length} ${prev}`;
}

// ── SIBO beam SENDER (host → armed device receiver) ─────────────────
// Replays the wire shapes the 3c's own sender produces, so the armed
// receiver tells us what each step of the protocol expects.
async function runBeamSender(): Promise<void> {
  const { IrSiboSendClient } = await import('../irda/sibosend.ts');
  // Give the key script time to arm the receiver (DownArrow at 33s).
  log('waiting for the receiver to arm…');
  await new Promise(r => setTimeout(r, 36_000));
  rxBuf = [];
  const sender = new IrSiboSendClient(
    () => { if (rxBuf.length === 0) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; },
    d => { sock?.write(Buffer.from(d)); return d.length; },
    { headerLen: device.startsWith('siena') ? 132 : 136, eofChunk: false },
  );
  try {
    await sender.sendFile('M:\\WRD\\HOST2.WRD',
      new TextEncoder().encode('beamed by IrSiboSendClient'),
      { onPhase: p => log('PHASE', p), onProgress: n => log('sent', n) });
    log('SEND-BEAM COMPLETE');
    console.error('\n*** SEND-BEAM SUCCESS ***');
  } catch (e) {
    log('SEND-BEAM FAILED:', (e as Error).message);
    console.error('\n*** SEND-BEAM FAILED ***');
  }
  // Let the harness run to its --serial-poll-until end so its final
  // screenshot (and M:-drive scan) reflect the post-beam device state.
  await new Promise(r => setTimeout(r, 45_000));
  process.exit(0);
}

async function runClient(): Promise<void> {
  if (cfg.irrecv) { return runBeamSender(); }
  const client = new PlpClient(
    () => { if (rxBuf.length === 0) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; },
    d => { sock?.write(Buffer.from(d)); return d.length; },
    {
      pollHz: Number(process.env.POLLHZ ?? 50),
      protocol: cfg.protocol ?? 'rfsv32',
      // ER3/ER4 CL-PS711x devices use the 0x22 Req_Con + passive handshake.
      conSeq: linkConSeq(device),
      onState: s => log('STATE →', s),
      onPduIn: p => log(pduStr('  IN ', p)),
      onPduOut: p => log(pduStr(' OUT ', p)),
    },
  );
  client.setChunkSize(256);
  if (process.env.NO_KEEPALIVE) {
    // Disable the link keepalive for A/B measurement.
    (client as unknown as { link: { keepAlive(): void } }).link.keepAlive = () => {};
  }
  client.start();
  try {
    if (cfg.beam) {
      // No PLP phase — the key script closes all apps and beams the
      // default Data file; we just listen as the IrDA responder.
      client.stop();
      rxBuf = [];
      (globalThis as Record<string, unknown>).__irDebug = true;
      const ir = new IrReceiveClient(
        () => { if (rxBuf.length === 0) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; },
        d => { sock?.write(Buffer.from(d)); return d.length; },
        { pollHz: 50, nickname: 'PsionWeb' },
      );
      ir.start();
      try {
        const f = await Promise.race([
          ir.receive({ onFileInfo: (n, s) => log('FILE INFO', n, s) }),
          new Promise<never>((_, rej) => setTimeout(() => rej(new Error('beam receive timed out')), 60_000)),
        ]);
        log('BEAM RECEIVED:', f.name, f.data.length, 'bytes');
        console.error('\n*** BEAM SUCCESS ***');
      } catch (e) {
        log('BEAM FAILED:', (e as Error).message);
        console.error('\n*** BEAM STALLED — see capture ***');
      }
      return;
    }
    if (cfg.print) {
      log('connectPrint…');
      await client.connectPrint(40_000);
      log('WPRT CONNECTED — waiting for the print job (key script prints at ~36s)…');
      const job = await Promise.race([
        client.waitForPrintJob((b, p) => log(`receiving… ${b} bytes, ${p} page(s)`)),
        new Promise<never>((_, rej) =>
          setTimeout(() => rej(new Error('print job timed out')), 100_000)),
      ]);
      const rawPages = job.pages.filter(p => !isJobSentinelPage(p));
      log(`JOB RECEIVED: ${job.pages.length} packets-pages (${rawPages.length} real)`);
      const parsed = rawPages.map(parsePage);
      parsed.forEach((p, i) => {
        log(`page ${i + 1}: ${p.primitives.length} primitives, ${p.errors.length} parse error(s)` +
            (p.errors.length ? ` — ${p.errors[0]}` : ''));
        log(`page ${i + 1} text: ${JSON.stringify(extractPageText(p).slice(0, 300))}`);
      });
      fs.writeFileSync('/tmp/5mx-print.pdf', renderJobToPdf(parsed));
      fs.writeFileSync('/tmp/5mx-print-raw.bin',
        Buffer.concat(rawPages.map(p => Buffer.from(p))));
      log('PDF written to /tmp/5mx-print.pdf, raw stream to /tmp/5mx-print-raw.bin');
      console.error('\n*** PRINT VIA PC SUCCESS ***');
      return;
    }
    log('connecting…');
    await client.connect(90_000);
    log('CONNECTED — listing drives…');
    let t = Date.now();
    const drives = await client.listDrives();
    log(`DRIVES (${Date.now() - t}ms) →`, drives.join(' '));
    if (cfg.protocol === 'rfsv16') {
      // Exercise the EPOC16 directory listing + a file round-trip on M:.
      // BAD_DIR=1 first sends the malformed "M\" path (the pre-fix
      // RemoteLinkDialog drive-root shape) to verify the device
      // survives the FOPEN error and keeps serving the link.
      if (process.env.BAD_DIR) {
        try {
          await client.listDirectory('M\\');
          log('BAD DIR unexpectedly succeeded');
        } catch (e) {
          log('BAD DIR error (expected):', (e as Error).message);
        }
      }
      // IN_USE_TEST=1 mirrors the browser-reported failure: list
      // M:\AGN\, download the in-use AGENDA.AGN (FOPEN -40), then make
      // sure the NEXT operation (an upload) still reaches the wire.
      if (process.env.IN_USE_TEST) {
        const agn = await client.listDirectory('M:\\AGN\\');
        log('DIR M:\\AGN\\ →', agn.map(e => e.longName).join(' '));
        try {
          const got = await client.downloadFile('M:\\AGN\\AGENDA.AGN');
          log('IN-USE DOWNLOAD unexpectedly succeeded:', got.length, 'bytes');
        } catch (e) {
          log('IN-USE DOWNLOAD error (expected):', (e as Error).message);
        }
        // Mimic the human gap between the failed download and the next
        // click — during which only keepalive Acks flow (IDLE_GAP=20).
        const gap = Number(process.env.IDLE_GAP ?? 0);
        if (gap > 0) { log(`idling ${gap}s (keepalive-only)…`); await new Promise(r => setTimeout(r, gap * 1000)); }
        // REPLACE_INUSE=1: FOPEN-replace the file Agenda holds open —
        // the worst-case upload target. Pin whether the device answers
        // (an error status) or goes silent (host-side timeout).
        if (process.env.REPLACE_INUSE) {
          try {
            await client.uploadFile('M:\\AGN\\AGENDA.AGN',
              new TextEncoder().encode('overwrite attempt'));
            log('REPLACE-IN-USE unexpectedly succeeded');
          } catch (e) {
            log('REPLACE-IN-USE error:', (e as Error).message);
          }
        }
        // LONG_NAME=1: upload with a browser-typical long filename —
        // EPOC16 is 8.3; pin whether the device errors or goes silent.
        if (process.env.LONG_NAME) {
          for (const name of ['My Notes File.txt', 'screenshot 2026-06-11 19.55.25.png']) {
            try {
              await client.uploadFile(`M:\\AGN\\${name}`,
                new TextEncoder().encode('long name probe'));
              log(`LONG-NAME upload "${name}" unexpectedly succeeded`);
            } catch (e) {
              log(`LONG-NAME upload "${name}" error:`, (e as Error).message);
            }
          }
          // …and prove the link still serves requests afterwards.
          const after = await client.listDirectory('M:\\');
          log('POST-LONG-NAME DIR ok:', after.length, 'entries');
        }
        await client.uploadFile('M:\\HOST2.TXT',
          new TextEncoder().encode('post-failure upload'));
        log('POST-FAILURE UPLOAD ok');
        await client.deleteFile('M:\\HOST2.TXT');
        log('POST-FAILURE DELETE ok');
      }
      const entries = await client.listDirectory('M:\\');
      log('DIR M:\\ →', entries.map(e => e.longName + (e.isDirectory ? '/' : '')).join(' '));
      const body = new TextEncoder().encode('hello from the host bridge');
      await client.uploadFile('M:\\HOST.TXT', body);
      log('UPLOAD ok');
      const back = await client.downloadFile('M:\\HOST.TXT');
      log('DOWNLOAD →', new TextDecoder().decode(back));
      if (new TextDecoder().decode(back) !== 'hello from the host bridge') {
        throw new Error('round-trip mismatch');
      }
      await client.deleteFile('M:\\HOST.TXT');
      log('DELETE ok');
    } else {
      // Timed directory reads — profiles the slow-RFSV-read report on the
      // CL-PS711x devices (quick connect/drive-list, very slow reads).
      t = Date.now();
      const root = await client.listDirectory('C:\\');
      log(`READDIR C:\\ (${Date.now() - t}ms) → ${root.length} entries:`,
          root.map(e => e.longName).slice(0, 8).join(', '));
      const sub = root.find(e => e.isDirectory);
      if (sub) {
        t = Date.now();
        const inner = await client.listDirectory(`C:\\${sub.longName}\\`);
        log(`READDIR C:\\${sub.longName}\\ (${Date.now() - t}ms) → ${inner.length} entries`);
      }
    }
    console.error('\n*** SUCCESS ***');
    if (process.env.HANG_PROBE) {
      // Mimic the browser dialog: stay CONNECTED (keepalive running)
      // while the key script pokes the device UI, so the screenshot
      // shows whether the kernel still responds with a live link.
      log('HANG_PROBE: staying connected while the harness key script runs…');
      await new Promise(r => setTimeout(r, 300_000));
    }
  } catch (e) {
    log('FAILED:', (e as Error).message);
    console.error('\n*** STALLED — see PDU trace above ***');
  } finally {
    try { client.disconnect(); } catch { /* ignore */ }
    setTimeout(() => process.exit(0), 200);
  }
}
