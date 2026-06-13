// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// WPRT ("Printer via PC") tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/wprt.test.mts
// or `npm run test:wprt` from frontend/.
//
// Covers: the WPRT_DATA page/packet assembler, the print-primitive
// parser + text extraction + PDF rendering, the LINK Register codec,
// and an end-to-end run of PlpClient.connectPrint()/waitForPrintJob()
// against an in-process mock device (same harness style as
// plp.client-spec.test.mts — real framing, link and NCP layers).

import { PlpClient } from '../plp/client-spec.ts';
import { WprtJobAssembler, isJobSentinelPage, WPRT_JOB_SENTINEL, WPRT_JOB_SENTINEL_WIRE } from '../plp/wprt-spec.ts';
import { parsePage, extractPageText, renderJobToPdf } from '../../lib/printer/wprt-render.ts';
import { encodePdu, decodePdu, reqConPdu, ackPdu, dataPdu } from '../plp/link.ts';
import { FrameDecoder } from '../plp/framing.ts';
import {
  encodeNcp, decodeNcp, makeConnectPacket, makeConnectResponsePacket,
  makeNcpInfoPacket, makeCompletePacket,
} from '../plp/ncp-spec.ts';
import {
  NCP, NCP_VERSION, NCP_SERVICE_LINK, NCP_LINK_CHAN,
  PDU_CONT_REQ, PDU_CONT_ACK, PDU_CONT_DATA, PDU_CONT_DISC,
  WPRT, WPRT_MORE, WPRT_LAST,
} from '../plp/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq<T>(actual: T, expected: T, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg}: got ${String(actual)}, want ${String(expected)}`);
    failures++;
  }
}
const sleep = (ms: number) => new Promise<void>(r => setTimeout(r, ms));

// ── Wire-data builders ──────────────────────────────────────────────

function concat(...parts: (number[] | Uint8Array)[]): Uint8Array {
  let total = 0;
  for (const p of parts) total += p.length;
  const out = new Uint8Array(total);
  let off = 0;
  for (const p of parts) { out.set(p, off); off += p.length; }
  return out;
}
const u32 = (n: number) => [n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, (n >>> 24) & 0xFF];
const u16 = (n: number) => [n & 0xFF, (n >> 8) & 0xFF];
const compactLen = (n: number) => [(n << 2) | 2];   // 1-byte form
const str = (s: string) => Array.from(s, c => c.charCodeAt(0));

const PAGE_MARKER = [0xe8, 0x03, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00];

// A representative page: Courier font, two text runs (one underlined,
// one justified), a rect and a line.
function buildTestPage(): Uint8Array {
  return concat(
    PAGE_MARKER,
    [0x00], u32((1 << 2) | 1),                      // WPRT_START: body section, page 1
    [0x07], compactLen(11), str('Courier New'),     // WPRT_USE_FONT
      [2], u16(200), u32(0), u32(200), u32(0),      //   screenFont 2, 10pt, plain
    [0x27], compactLen(15), str('Hello from EPOC'), // WPRT_DRAW_TEXT
      u32(1440), u32(1440),                         //   1in, 1in
    [0x09, 0x01],                                   // underline on
    [0x28], compactLen(9), str('Justified'),        // WPRT_DRAW_TEXT_JUSTIFIED
      u32(1440), u32(2000), u32(8000), u32(2400),   //   box
      u32(2300), [0x01], u32(0),                    //   baseline, centre, margin 0
    [0x09, 0x00],                                   // underline off
    [0x0d, 0x55, 0x55, 0x55],                       // pen dark grey
    [0x19], u32(1440), u32(2880), u32(7200), u32(2880),  // WPRT_DRAW_LINE
    [0x11, 0x01],                                   // brush solid
    [0x20], u32(1440), u32(3000), u32(2880), u32(3600),  // WPRT_DRAW_RECT
    [0x01],                                         // WPRT_END
  );
}

// ── WprtJobAssembler ───────────────────────────────────────────────

{
  const page = buildTestPage();
  const asm = new WprtJobAssembler();

  // Job-start sentinel, exactly as the real 5mx emits it: the fifteen
  // wire bytes are a complete WPRT_DATA reply (header + 9-byte page).
  let job = asm.feed(WPRT_JOB_SENTINEL_WIRE);
  check(job === null, 'sentinel packet alone does not complete the job');
  eq(asm.pagesSoFar, 1, 'sentinel page assembled');

  // Real page split across two packets: header packet + continuation
  // that carries the end-of-job marker.
  const splitAt = 20;
  job = asm.feed(concat([WPRT_MORE, WPRT_LAST], u32(page.length), page.subarray(0, splitAt)));
  check(job === null, 'mid-page packet does not complete the job');
  job = asm.feed(concat([WPRT_LAST], page.subarray(splitAt)));
  check(job !== null, 'end-of-job marker completes the job');
  if (job) {
    eq(job.pages.length, 2, 'job has sentinel + one real page');
    check(isJobSentinelPage(job.pages[0]), 'first page is the job sentinel');
    eq(job.pages[1].length, page.length, 'real page reassembled to full length');
  }
  eq(asm.pagesSoFar, 0, 'assembler resets after delivering a job');
}

// ── Primitive parser + text extraction ─────────────────────────────

{
  const parsed = parsePage(buildTestPage());
  eq(parsed.errors.length, 0, `page parses cleanly (${parsed.errors[0] ?? ''})`);
  const ops = parsed.primitives.map(p => p.op);
  check(ops.includes('font') && ops.includes('text') && ops.includes('textJustified')
        && ops.includes('line') && ops.includes('rect'), 'all primitives surfaced');
  const font = parsed.primitives.find(p => p.op === 'font');
  if (font && font.op === 'font') {
    eq(font.font.face, 'Courier New', 'font face decoded');
    eq(font.font.actualSizeTwips, 200, 'font size decoded');
  }
  const text = extractPageText(parsed);
  check(text.includes('Hello from EPOC'), 'text extraction finds the text run');
  check(text.includes('Justified'), 'text extraction finds the justified run');
  check(text.indexOf('Hello') < text.indexOf('Justified'),
        'lines come out in baseline order');
}

{
  // Truncated page: parser keeps what it has and reports the error.
  const page = buildTestPage();
  const parsed = parsePage(page.subarray(0, page.length - 6));
  check(parsed.errors.length > 0, 'truncated page reports an error');
  check(parsed.primitives.length > 0, 'truncated page still yields leading primitives');
}

// ── PDF rendering ──────────────────────────────────────────────────

function latin1Str(b: Uint8Array): string {
  let s = '';
  for (const x of b) s += String.fromCharCode(x);
  return s;
}

{
  const pdf = renderJobToPdf([parsePage(buildTestPage())]);
  const s = latin1Str(pdf);
  check(s.startsWith('%PDF-1.4\n'), 'PDF header present');
  check(s.endsWith('%%EOF\n'), 'PDF trailer present');
  check(s.includes('(Hello from EPOC) Tj'), 'text run rendered');
  check(s.includes('/BaseFont /Courier'), 'Courier mapped from Courier New');
  check(s.includes(' re '), 'rect rendered');
  const sx = /startxref\n(\d+)\n%%EOF\n$/.exec(s);
  check(sx !== null && s.slice(Number(sx![1]), Number(sx![1]) + 4) === 'xref',
        'startxref points at the xref table');
}

{
  // Bitmap: 4×2 1bpp uncompressed (rows padded to 4 bytes), drawn into
  // a 1in square — renders as a DeviceGray image XObject.
  const bmp = concat(
    PAGE_MARKER,
    [0x25],
    u32(1440), u32(1440), u32(2880), u32(2880),     // dest rect
    u32(0x28 + 8), u32(0x28),                       // data length / offset
    u32(4), u32(2),                                 // 4×2 px
    u32(0), u32(0),                                 // raw w/h
    u32(1),                                         // 1bpp b&w
    [0, 0, 0, 0, 0, 0, 0, 0],                       // reserved words
    u32(0),                                         // uncompressed
    [0x05, 0, 0, 0,  0x0A, 0, 0, 0],                // two padded rows
  );
  const parsed = parsePage(bmp);
  eq(parsed.errors.length, 0, `bitmap page parses (${parsed.errors[0] ?? ''})`);
  const prim = parsed.primitives[0];
  check(prim?.op === 'bitmap', 'bitmap primitive surfaced');
  if (prim && prim.op === 'bitmap') {
    eq(prim.gray.length, 8, 'one grey byte per pixel');
    eq(prim.gray[0], 0xFF, '1bpp set bit = white');
    eq(prim.gray[1], 0x00, '1bpp clear bit = black');
  }
  const s = latin1Str(renderJobToPdf([parsed]));
  check(s.includes('/Subtype /Image') && s.includes('/DeviceGray'),
        'bitmap embedded as a grey image XObject');
  check(s.includes('/Im1 Do'), 'bitmap placed on the page');
}

// ── Mock device for the end-to-end run ──────────────────────────────

class Bridge {
  private fromDevice: number[] = [];
  private fromClient: number[] = [];
  read(): Uint8Array {
    if (this.fromDevice.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromDevice); this.fromDevice = []; return out;
  }
  write(d: Uint8Array): number {
    for (const b of d) this.fromClient.push(b);
    return d.length;
  }
  deviceTx(d: Uint8Array): void { for (const b of d) this.fromDevice.push(b); }
  deviceRx(): Uint8Array {
    if (this.fromClient.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromClient); this.fromClient = []; return out;
  }
}

// Speaks the device side of the print flow: link handshake, NCP info,
// rejects the first SYS$WPRT.* Connect (server not loaded), accepts a
// LINK Register for SYS$WPRT, accepts the retried Connect, answers
// WPRT_LEVEL, and serves a queued job packet-by-packet on WPRT_DATA —
// holding the reply back until a job is queued, like the real spooler.
class MockPrintDevice {
  private bridge: Bridge;
  private decoder = new FrameDecoder();
  private myMagic = new Uint8Array([0x51, 0x62, 0x73, 0x84]);
  private linkUp = false;
  private seqTx = 0;
  private clientInfoSeen = false;
  private wprtRegistered = false;
  private wprtServerChan = 0;
  private wprtClientChan = 0;
  private timer: ReturnType<typeof setInterval> | null = null;

  // Job queue: packets ready to stream, one per WPRT_DATA command.
  private packets: Uint8Array[] = [];
  private dataPollPending = false;
  registerSeen = false;

  constructor(bridge: Bridge) { this.bridge = bridge; }
  start(): void { this.timer = setInterval(() => this.tick(), 1); }
  stop(): void { if (this.timer !== null) { clearInterval(this.timer); this.timer = null; } }

  // "User prints": queue the job's packets; release a held poll. The
  // job opens with the sentinel reply verbatim (header included), the
  // way the real 5mx ROM sends it.
  queueJob(pages: Uint8Array[]): void {
    this.packets.push(new Uint8Array(WPRT_JOB_SENTINEL_WIRE));
    const all = pages.map((p, i) => ({ data: p, last: i === pages.length - 1 }));
    all.forEach((page, idx) => {
      // First packet of the page carries the long header; split big
      // pages at 100 bytes to exercise continuation packets.
      const isJobEnd = idx === all.length - 1;
      const CHUNK = 100;
      const head = page.data.subarray(0, Math.min(CHUNK, page.data.length));
      const restLen = page.data.length - head.length;
      this.packets.push(concat(
        [restLen > 0 ? WPRT_MORE : (isJobEnd ? WPRT_LAST : WPRT_MORE),
         page.last ? WPRT_LAST : WPRT_MORE],
        u32(page.data.length), head,
      ));
      for (let off = CHUNK; off < page.data.length; off += CHUNK) {
        const tail = off + CHUNK >= page.data.length;
        this.packets.push(concat(
          [tail && isJobEnd ? WPRT_LAST : WPRT_MORE],
          page.data.subarray(off, off + CHUNK),
        ));
      }
    });
    if (this.dataPollPending) { this.dataPollPending = false; this.serveDataPacket(); }
  }

  private tick(): void {
    const bytes = this.bridge.deviceRx();
    if (bytes.length === 0) return;
    for (const f of this.decoder.feed(bytes)) this.handleFrame(f.payload);
  }
  private sendPdu(pdu: { cont: number; seq: number; data: Uint8Array }): void {
    this.bridge.deviceTx(encodePdu(pdu));
  }
  private sendDataPdu(data: Uint8Array): void {
    this.seqTx = (this.seqTx + 1) & 0x7FF;
    if (this.seqTx === 0) this.seqTx = 1;
    this.sendPdu(dataPdu(this.seqTx, data));
  }

  private handleFrame(payload: Uint8Array): void {
    const pdu = decodePdu(payload);
    if (!pdu) return;
    if (pdu.cont === PDU_CONT_REQ) {
      if (pdu.data.length === 0) this.sendPdu(reqConPdu(this.myMagic, 4));
      else if (pdu.data.length === 4) {
        this.sendPdu(ackPdu(0));
        this.linkUp = true;
        this.sendDataPdu(encodeNcp(makeNcpInfoPacket(NCP_VERSION.EPOC_ER3,
          new Uint8Array([0x01, 0x02, 0x03, 0x04]))));
        this.sendDataPdu(encodeNcp(makeConnectPacket(1, NCP_SERVICE_LINK)));
      }
      return;
    }
    if (pdu.cont === PDU_CONT_ACK) { this.linkUp = true; return; }
    if (pdu.cont === PDU_CONT_DISC) { this.linkUp = false; this.seqTx = 0; return; }
    if (pdu.cont !== PDU_CONT_DATA) return;
    this.sendPdu(ackPdu(pdu.seq));

    const p = decodeNcp(pdu.data);
    if (!p) return;
    if (p.type === NCP.INFO) { this.clientInfoSeen = true; return; }
    if (p.type === NCP.CONNECT) {
      const name = new TextDecoder().decode(
        p.data[p.data.length - 1] === 0 ? p.data.subarray(0, -1) : p.data);
      if (!this.clientInfoSeen || name !== 'SYS$WPRT.*' || !this.wprtRegistered) {
        this.sendDataPdu(encodeNcp(makeConnectResponsePacket(0, p.srcChan, 1)));
        return;
      }
      this.wprtServerChan = 12;
      this.wprtClientChan = p.srcChan;
      this.sendDataPdu(encodeNcp(makeConnectResponsePacket(
        this.wprtServerChan, this.wprtClientChan, 0)));
      return;
    }
    if (p.type !== NCP.COMPLETE) return;

    // LINK channel: the Register command.
    if (p.destChan === NCP_LINK_CHAN) {
      if (p.data[0] !== 0x00) return;
      const opId = p.data[1] | (p.data[2] << 8);
      const name = new TextDecoder().decode(p.data.subarray(3));
      if (name === 'SYS$WPRT') { this.wprtRegistered = true; this.registerSeen = true; }
      // Reply: [0x01][opId][status u16][?? u16][name]
      this.sendDataPdu(encodeNcp(makeCompletePacket(NCP_LINK_CHAN, NCP_LINK_CHAN, concat(
        [0x01], u16(opId), u16(this.wprtRegistered ? 0 : 1), u16(0), str('SYS$WPRT.*'),
      ))));
      return;
    }

    // WPRT channel commands.
    if (p.destChan !== this.wprtServerChan || p.srcChan !== this.wprtClientChan) return;
    const cmd = p.data[0];
    if (cmd === WPRT.LEVEL) {
      this.sendWprtReply(new Uint8Array([0x00, p.data[1], p.data[2]]));
    } else if (cmd === WPRT.DATA) {
      if (this.packets.length === 0) { this.dataPollPending = true; return; }
      this.serveDataPacket();
    }
  }

  private serveDataPacket(): void {
    const pkt = this.packets.shift();
    if (pkt) this.sendWprtReply(pkt);
  }
  private sendWprtReply(data: Uint8Array): void {
    this.sendDataPdu(encodeNcp(makeCompletePacket(
      this.wprtClientChan, this.wprtServerChan, data)));
  }
}

// ── End-to-end: connectPrint + waitForPrintJob ──────────────────────

async function e2e(): Promise<void> {
  const bridge = new Bridge();
  const device = new MockPrintDevice(bridge);
  device.start();
  const client = new PlpClient(() => bridge.read(), d => bridge.write(d), { pollHz: 200 });
  client.start();
  try {
    await client.connectPrint(10_000);
    check(device.registerSeen, 'client fell back to LINK Register when the first Connect failed');

    const jobPromise = client.waitForPrintJob();
    // Let the WPRT_DATA poll land and be held by the device, then "print".
    await sleep(100);
    device.queueJob([buildTestPage()]);
    const job = await Promise.race([
      jobPromise,
      new Promise<never>((_, rej) => setTimeout(() => rej(new Error('e2e job timed out')), 8000)),
    ]);
    eq(job.pages.length, 2, 'e2e job: sentinel + one page');
    check(isJobSentinelPage(job.pages[0]), 'e2e job: sentinel page intact');
    const parsed = parsePage(job.pages[1]);
    eq(parsed.errors.length, 0, `e2e page parses (${parsed.errors[0] ?? ''})`);
    check(extractPageText(parsed).includes('Hello from EPOC'),
          'e2e page text survives the full stack');

    // Aborting a listen rejects promptly.
    const wait2 = client.waitForPrintJob();
    await sleep(50);
    client.abortPrintWait();
    let aborted = false;
    await wait2.catch(() => { aborted = true; });
    check(aborted, 'abortPrintWait rejects the pending listen');
  } finally {
    client.disconnect();
    device.stop();
  }
}

e2e().then(() => {
  if (failures > 0) {
    console.error(`\n${failures} failure(s)`);
    process.exit(1);
  }
  console.log('wprt.test.mts: all tests passed');
}).catch(e => {
  console.error(`FAIL: e2e threw: ${e instanceof Error ? e.message : e}`);
  process.exit(1);
});
