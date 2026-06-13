// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrDA upper-layer wire-format unit tests (IAS, OBEX, Tiny TP). These
// assert exact bytes against hand-computed expectations so an encoder
// regression trips independently of the e2e mock. Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/irda.wire.test.mts

import {
  nameHeader,
  lengthHeader,
  bodyHeader,
  encodeConnect,
  encodePut,
  encodeDisconnect,
  parseConnectReply,
  responseCode,
} from '../irda/obex.ts';
import { encodeGetValueByClass, parseGetValueByClassReply } from '../irda/ias.ts';
import { encodeFileCommand, isAckYes } from '../irda/eikonir.ts';
import { TinyTp } from '../irda/tinytp.ts';
import { IAS_EIKONIR_V2_CLASS, EIKONIR_LSAPSEL } from '../irda/types.ts';
import {
  OBEX_HDR_NAME,
  OBEX_HDR_LENGTH,
  OBEX_HDR_BODY,
  OBEX_HDR_END_OF_BODY,
  OBEX_CONNECT,
  OBEX_PUT,
  OBEX_PUT_FINAL,
  OBEX_RESP_SUCCESS,
} from '../irda/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) {
    console.error(`FAIL: ${msg}`);
    failures++;
  }
}
function eqBytes(a: Uint8Array, b: number[], msg: string) {
  if (a.length !== b.length) {
    console.error(`FAIL: ${msg}: length ${a.length} vs ${b.length} (got [${Array.from(a).map((x) => x.toString(16))}])`);
    failures++;
    return;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      console.error(`FAIL: ${msg}: byte ${i} is 0x${a[i].toString(16)}, want 0x${b[i].toString(16)}`);
      failures++;
      return;
    }
  }
}

// ── OBEX headers ──────────────────────────────────────────────────────
{
  // Name "A" → id 01, len 0007, UTF-16BE 'A' + NUL.
  eqBytes(nameHeader('A'), [OBEX_HDR_NAME, 0x00, 0x07, 0x00, 0x41, 0x00, 0x00], 'nameHeader("A")');
  // Length 0x12345678 → C3 + 4 BE bytes.
  eqBytes(lengthHeader(0x12345678), [OBEX_HDR_LENGTH, 0x12, 0x34, 0x56, 0x78], 'lengthHeader');
  // Body [01 02 03] → 48 00 06 01 02 03.
  eqBytes(bodyHeader(OBEX_HDR_BODY, new Uint8Array([1, 2, 3])), [OBEX_HDR_BODY, 0x00, 0x06, 1, 2, 3], 'bodyHeader');
  eqBytes(bodyHeader(OBEX_HDR_END_OF_BODY, new Uint8Array(0)), [OBEX_HDR_END_OF_BODY, 0x00, 0x03], 'empty End-of-Body');
}

// ── OBEX packets ──────────────────────────────────────────────────────
{
  eqBytes(encodeConnect(0x2000), [OBEX_CONNECT, 0x00, 0x07, 0x10, 0x00, 0x20, 0x00], 'encodeConnect');
  const put = encodePut(false, [bodyHeader(OBEX_HDR_BODY, new Uint8Array([0xaa]))]);
  eqBytes(put, [OBEX_PUT, 0x00, 0x07, OBEX_HDR_BODY, 0x00, 0x04, 0xaa], 'encodePut non-final');
  const putF = encodePut(true, []);
  eqBytes(putF, [OBEX_PUT_FINAL, 0x00, 0x03], 'encodePut final empty');
  eqBytes(encodeDisconnect(), [0x81, 0x00, 0x03], 'encodeDisconnect');

  const reply = new Uint8Array([OBEX_RESP_SUCCESS, 0x00, 0x07, 0x10, 0x00, 0x04, 0x00]);
  const parsed = parseConnectReply(reply);
  check(parsed.code === OBEX_RESP_SUCCESS, 'parseConnectReply code');
  check(parsed.maxPacket === 0x0400, `parseConnectReply mtu = 0x${parsed.maxPacket.toString(16)} (want 0x400)`);
  check(responseCode(reply) === OBEX_RESP_SUCCESS, 'responseCode');
}

// ── IAS GetValueByClass ───────────────────────────────────────────────
// These exact bytes were captured off the real 5mx (scripts/test-infrared.sh
// trace): the query for class "Epoc32:EikonIr:v2.0" / attr "IrDA:TinyTP:LsapSel"
// and the device's SUCCESS reply carrying integer LSAP 8.
{
  const req = encodeGetValueByClass(IAS_EIKONIR_V2_CLASS, 'IrDA:TinyTP:LsapSel');
  // op(84) classLen(13) "Epoc32:EikonIr:v2.0" attrLen(13) "IrDA:TinyTP:LsapSel"
  check(req[0] === 0x84, 'IAS op byte (GetValueByClass | last)');
  check(req[1] === 0x13, `IAS class length = ${req[1]} (want 0x13=19)`);
  check(String.fromCharCode(...req.subarray(2, 2 + 0x13)) === 'Epoc32:EikonIr:v2.0', 'IAS class name');
  check(req[2 + 0x13] === 0x13, `IAS attr length = ${req[2 + 0x13]} (want 0x13)`);
  check(String.fromCharCode(...req.subarray(3 + 0x13)) === 'IrDA:TinyTP:LsapSel', 'IAS attr name');
  // Full captured request bytes from the device transcript.
  eqBytes(req, [
    0x84, 0x13, 0x45, 0x70, 0x6f, 0x63, 0x33, 0x32, 0x3a, 0x45, 0x69, 0x6b, 0x6f, 0x6e,
    0x49, 0x72, 0x3a, 0x76, 0x32, 0x2e, 0x30, 0x13, 0x49, 0x72, 0x44, 0x41, 0x3a, 0x54,
    0x69, 0x6e, 0x79, 0x54, 0x50, 0x3a, 0x4c, 0x73, 0x61, 0x70, 0x53, 0x65, 0x6c,
  ], 'IAS GetValueByClass(EikonIr v2) request matches captured device bytes');

  // Captured device reply: op(84) ret(00) listLen(0001) objId(0002)
  // type(01 int) value(00000008) — LSAP-SEL 8.
  const reply = new Uint8Array([0x84, 0x00, 0x00, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x08]);
  const res = parseGetValueByClassReply(reply);
  check(res.retCode === 0, 'IAS reply retCode success');
  check(res.integer === EIKONIR_LSAPSEL, `IAS reply integer = ${res.integer} (want ${EIKONIR_LSAPSEL})`);

  const fail = new Uint8Array([0x84, 0x01]); // NO_SUCH_CLASS (the old "OBEX" reply)
  const fres = parseGetValueByClassReply(fail);
  check(fres.retCode === 1 && fres.integer === null, 'IAS reply failure surfaces retCode, null integer');
}

// ── EPOC Eikon-IR transfer protocol ───────────────────────────────────
{
  // FILE line: matches the captured device transcript
  // "FILE 30 32 0 0 PsionWeb.txt" for a 30-byte file.
  const cmd = encodeFileCommand('PsionWeb.txt', 30);
  check(String.fromCharCode(...cmd) === 'FILE 30 32 0 0 PsionWeb.txt',
    `FILE command = "${String.fromCharCode(...cmd)}"`);
  // A caller-supplied path is stripped to the bare name+ext.
  const cmd2 = encodeFileCommand('C:\\Documents\\Memo.txt', 5);
  check(String.fromCharCode(...cmd2) === 'FILE 5 32 0 0 Memo.txt',
    `FILE command strips path: "${String.fromCharCode(...cmd2)}"`);
  // ACK parsing (case-insensitive, like EPOC's CompareF).
  check(isAckYes(new Uint8Array([0x41, 0x43, 0x4b, 0x20, 0x59]) /* "ACK Y" */), 'ACK Y accepted');
  check(!isAckYes(new Uint8Array([0x41, 0x43, 0x4b, 0x20, 0x4e]) /* "ACK N" */), 'ACK N rejected');
  check(!isAckYes(new Uint8Array(0)), 'empty ACK rejected');
}

// ── Tiny TP segmentation + credit ─────────────────────────────────────
// Wrapped in an async main: top-level await of timer-based promises
// deadlocks node's --experimental-strip-types loader, so async tests run
// here and the process exits explicitly.
async function main() {
  // Fake LM-MUX capturing data PDUs and faking a confirm with credit 5.
  const sent: Uint8Array[] = [];
  const fakeLmp = {
    connect: () => Promise.resolve(new Uint8Array([5])), // peer initial credit 5
    setDataHandler: () => {},
    setDisconnectHandler: () => {},
    sendData: (_lsap: number, data: Uint8Array) => sent.push(data),
    disconnect: () => {},
  };
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const ttp = new TinyTp(fakeLmp as any, 6, 7);
  await ttp.connect(1000);
  check(ttp.availableSendCredit === 5, `send credit from confirm = ${ttp.availableSendCredit} (want 5)`);

  // Send a 150-byte SDU → must segment into 60+60+30 (3 PDUs); all but
  // the last carry the M bit.
  const sdu = new Uint8Array(150).map((_, i) => i & 0xff);
  await ttp.send(sdu, 1000);
  check(sent.length === 3, `150-byte SDU produced ${sent.length} PDUs (want 3)`);
  check((sent[0][0] & 0x80) !== 0, 'segment 0 has M bit');
  check((sent[1][0] & 0x80) !== 0, 'segment 1 has M bit');
  check((sent[2][0] & 0x80) === 0, 'final segment clears M bit');
  // Reassemble payloads (strip the 1-byte TTP header) and compare.
  const reassembled = new Uint8Array([
    ...sent[0].subarray(1),
    ...sent[1].subarray(1),
    ...sent[2].subarray(1),
  ]);
  check(reassembled.length === 150, `reassembled length ${reassembled.length} (want 150)`);
  for (let i = 0; i < 150; i++) {
    if (reassembled[i] !== (i & 0xff)) {
      check(false, `reassembled byte ${i} mismatch`);
      break;
    }
  }
  // Three sends spent three credits.
  check(ttp.availableSendCredit === 2, `send credit after 3 PDUs = ${ttp.availableSendCredit} (want 2)`);
}

main().then(() => {
  if (failures > 0) {
    console.error(`\n${failures} test failure(s)`);
    process.exit(1);
  }
  console.log('OK — all IrDA wire-format tests passed');
  process.exit(0);
}).catch((e) => {
  console.error('FAIL: unexpected error', e);
  process.exit(1);
});
