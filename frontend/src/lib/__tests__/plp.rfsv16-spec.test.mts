// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// RFSV16 codec tests. Request vectors are the exact NCP payloads
// observed on the wire against the live Series 3c v5.20f / Siena
// v4.20f ROMs (via _plp_repro.mts); the FDIRREAD reply layout matches
// the live M:\ listing (one u16 buffer length, then repeated
// 16-byte+name records).
//
// Run: node --experimental-strip-types frontend/src/lib/__tests__/plp.rfsv16-spec.test.mts

import {
  buildFOpen, buildFClose, buildFRead, buildFWrite, buildFDirRead,
  buildDelete, buildStatusDevice, decodeRfsv16Reply, decodeDirEntries,
  FOPEN_MODE,
} from '../plp/rfsv16-spec.ts';

let failures = 0;
function check(name: string, cond: boolean, extra?: string) {
  if (cond) console.log(`ok   ${name}`);
  else { console.error(`FAIL ${name}${extra ? ` — ${extra}` : ''}`); failures++; }
}
const hex = (b: Uint8Array) => Array.from(b).map(x => x.toString(16).padStart(2, '0')).join(' ');

// ── Request encodings (live wire vectors) ───────────────────────────
check('STATUSDEVICE M:',
  hex(buildStatusDevice('M:')) === '20 00 03 00 4d 3a 00');
check('FOPEN dir M:\\ (mode 0x0430)',
  hex(buildFOpen(FOPEN_MODE.OPEN_EXISTING | FOPEN_MODE.RECORD_DIR | FOPEN_MODE.SHARE, 'M:\\'))
    === '00 00 06 00 30 04 4d 3a 5c 00');
check('FDIRREAD handle 1',
  hex(buildFDirRead(1)) === '06 00 02 00 01 00');
check('FCLOSE handle 1',
  hex(buildFClose(1)) === '02 00 02 00 01 00');
check('FREAD handle 1 len 256',
  hex(buildFRead(1, 256)) === '04 00 04 00 01 00 00 01');
check('FWRITE handle 1 two bytes',
  hex(buildFWrite(1, new Uint8Array([0xAB, 0xCD]))) === '0a 00 04 00 01 00 ab cd');
check('DELETE M:\\X',
  hex(buildDelete('M:\\X')) === '14 00 05 00 4d 3a 5c 58 00');

// ── Reply decoding ──────────────────────────────────────────────────
{
  // Live: FOPEN dir reply `2a 00 04 00 00 00 01 00` = status 0, handle 1.
  const r = decodeRfsv16Reply(new Uint8Array([0x2a, 0, 0x04, 0, 0, 0, 0x01, 0]));
  check('FOPEN reply status/handle', !!r && r.status === 0 && (r.data[0] | (r.data[1] << 8)) === 1);
}
{
  // Live: absent SSD pack reply `2a 00 02 00 c2 ff` = status -62.
  const r = decodeRfsv16Reply(new Uint8Array([0x2a, 0, 0x02, 0, 0xc2, 0xff]));
  check('negative status decodes signed', !!r && r.status === -62, String(r?.status));
}
{
  // Live: FDIRREAD end `2a 00 02 00 dc ff` = -36 (E_SIBO_FILE_EOF).
  const r = decodeRfsv16Reply(new Uint8Array([0x2a, 0, 0x02, 0, 0xdc, 0xff]));
  check('EOF status', !!r && r.status === -36, String(r?.status));
}

// ── FDIRREAD entry decode (live layout: bufLen then repeated records) ──
{
  const rec = (name: string, attr: number, size: number) => {
    const n = [...new TextEncoder().encode(name), 0];
    const out = new Uint8Array(16 + n.length);
    out[2] = attr & 0xFF; out[3] = (attr >> 8) & 0xFF;
    out[4] = size & 0xFF; out[5] = (size >> 8) & 0xFF;
    out.set(n, 16);
    return out;
  };
  const parts = [rec('WLD', 0x0010, 0), rec('SPR', 0x0010, 0), rec('HOST.TXT', 0x0021, 26)];
  let total = 0; for (const p of parts) total += p.length;
  const data = new Uint8Array(2 + total);
  data[0] = total & 0xFF; data[1] = (total >> 8) & 0xFF;
  let off = 2; for (const p of parts) { data.set(p, off); off += p.length; }
  const entries = decodeDirEntries(data);
  check('three entries decoded', entries.length === 3, String(entries.length));
  check('directory flag', entries[0].longName === 'WLD' && entries[0].isDirectory);
  check('file entry', entries[2].longName === 'HOST.TXT' && !entries[2].isDirectory && entries[2].size === 26);
}

process.exit(failures === 0 ? 0 : 1);
