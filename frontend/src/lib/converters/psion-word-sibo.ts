// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// SIBO / EPOC16 Psion Word (.WRD) -> plain text, including recovery of
// password-protected documents.
//
// This is the EPOC16 word processor format used by the Series 3 / 3a /
// 3c / 3mx / Siena / Workabout / Pocket Book — completely distinct from
// the EPOC32 (Series 5) CRichText format handled by word.ts.  A SIBO
// Word file is a flat sequence of length-prefixed records after a fixed
// header:
//
//   offset 0x00  "PSIONWPDATAFILE\0"          16-byte magic
//   offset 0x10  u16  document flags           0x0001 = plain, 0x0100 = encrypted
//   offset 0x12  u16  (0 plain / 1 encrypted)
//   offset 0x14  16 bytes                       encryption "key" field:
//                                               9 bytes of obfuscated key
//                                               material followed by the
//                                               first 7 repeated (all 0xEA
//                                               filler on a plain file).
//   offset 0x28  records begin                  each: u16 type, u16 len, len bytes
//
// Record types (from the format and confirmed against the ROM + 3-Lib
// samples): 1 printer/page setup, 2 layout metrics, 3 printer name,
// 4/5 misc, 6 paragraph styles, 7 emphasis styles, 8 BODY TEXT, 9 the
// outline/structure list.  Only record 8 holds the document text.
//
// Password protection (per WordCrypt's README and verified here by
// encrypting known documents on the emulated 3a): the password derives a
// 16-bit key; from it Word builds a 9-byte additive keystream and
// enciphers ONLY the body (record 8) as
//
//     cipher[i] = (plain[i] + pad[i]) mod 256
//     pad[i]    = key9[(i mod 16) mod 9]
//
// i.e. the 9-byte key is laid into a 16-byte block that resets every 16
// bytes.  Nothing else in the file changes.  The key field at 0x14 is a
// one-way-obfuscated copy of the key that even the original tool's author
// could not invert, so — like WordCrypt — we recover the keystream from
// the ciphertext instead of from the password.  Because the cipher is a
// short additive repeating key over highly-structured text, the nine key
// bytes are independent and each is recovered by scoring candidate
// decryptions against an English character model.  A caller who remembers
// the first few characters of the document can supply them as a crib to
// pin the key exactly (the first nine characters cover all nine key
// positions, giving a mathematically exact result).

export const SIBO_WORD_MAGIC = 'PSIONWPDATAFILE\0';

const HEADER_BYTES   = 0x28;   // records start here
const FLAG_OFFSET    = 0x10;   // u16 document flags
const KEY_OFFSET     = 0x14;   // 16-byte key field
const KEY_LEN        = 9;      // meaningful key bytes
const BLOCK_LEN      = 16;     // keystream block period
const BODY_RECORD    = 8;      // record type holding the text

// EPOC16 special text codes inside the body record.
const PARA_END   = 0x00;   // paragraph delimiter
const LINE_BREAK = 0x06;   // forced line break (rare)
const TAB        = 0x09;
const SOFT_NL    = 0x0a;   // secondary newline seen in wild samples
const VIS_SPACE  = 0x0f;   // "visible space" marker

// Psion SIBO extended character set is IBM code page 850 for 0x80-0xFF.
const PSION_HIGH: number[] = [
  0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
  0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
  0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
  0x00ff, 0x00d6, 0x00dc, 0x00f8, 0x00a3, 0x00d8, 0x00d7, 0x0192,
  0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
  0x00bf, 0x00ae, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
  0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00c1, 0x00c2, 0x00c0,
  0x00a9, 0x2563, 0x2551, 0x2557, 0x255d, 0x00a2, 0x00a5, 0x2510,
  0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x00e3, 0x00c3,
  0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x00a4,
  0x00f0, 0x00d0, 0x00ca, 0x00cb, 0x00c8, 0x0131, 0x00cd, 0x00ce,
  0x00cf, 0x2518, 0x250c, 0x2588, 0x2584, 0x00a6, 0x00cc, 0x2580,
  0x00d3, 0x00df, 0x00d4, 0x00d2, 0x00f5, 0x00d5, 0x00b5, 0x00fe,
  0x00de, 0x00da, 0x00db, 0x00d9, 0x00fd, 0x00dd, 0x00af, 0x00b4,
  0x00ad, 0x00b1, 0x2017, 0x00be, 0x00b6, 0x00a7, 0x00f7, 0x00b8,
  0x00b0, 0x00a8, 0x00b7, 0x00b9, 0x00b3, 0x00b2, 0x25a0, 0x00a0,
];

export interface SiboWordRecord { type: number; start: number; end: number; }

export interface SiboWordInfo {
  encrypted: boolean;
  records: SiboWordRecord[];
  body: SiboWordRecord | null;
}

// =============================================================================
// Detection + record scan
// =============================================================================

export function isSiboWord(bytes: Uint8Array): boolean {
  if (bytes.length < HEADER_BYTES) return false;
  for (let i = 0; i < SIBO_WORD_MAGIC.length; i++) {
    if (bytes[i] !== SIBO_WORD_MAGIC.charCodeAt(i)) return false;
  }
  return true;
}

// A file is password-protected when the document-flags word at 0x10 is
// non-plain.  Plain files carry 0x0001 there; protected files 0x0100
// with the key field at 0x14 programmed (rather than 0xEA filler).
export function isSiboWordEncrypted(bytes: Uint8Array): boolean {
  if (!isSiboWord(bytes)) return false;
  return bytes[FLAG_OFFSET] === 0x00 && bytes[FLAG_OFFSET + 1] === 0x01;
}

export function scanSiboWord(bytes: Uint8Array): SiboWordInfo | null {
  if (!isSiboWord(bytes)) return null;
  const records: SiboWordRecord[] = [];
  let off = HEADER_BYTES;
  while (off + 4 <= bytes.length) {
    const type = bytes[off] | (bytes[off + 1] << 8);
    const len  = bytes[off + 2] | (bytes[off + 3] << 8);
    const start = off + 4;
    const end = start + len;
    if (end > bytes.length) break;
    records.push({ type, start, end });
    off = end;
  }
  const body = records.find(r => r.type === BODY_RECORD) ?? null;
  return { encrypted: isSiboWordEncrypted(bytes), records, body };
}

// =============================================================================
// Password recovery
// =============================================================================

// Position -> key index for the additive keystream.
function keyIndex(pos: number): number {
  return (pos % BLOCK_LEN) % KEY_LEN;
}

export interface RecoveryResult {
  key: number[];          // the recovered 9-byte keystream
  plainBody: Uint8Array;  // decrypted body record bytes
  confidence: number;     // 0..1, share of body that reads as plain text
}

// Recover the 9-byte additive keystream for an encrypted body.  `crib`,
// if given, is the known leading plaintext of the document; each crib
// byte pins one key position exactly (the first nine cover them all).
// Positions not pinned by the crib are recovered statistically.
export function recoverBodyKey(cipher: Uint8Array, crib?: Uint8Array): RecoveryResult {
  const N = cipher.length;
  const model = englishModel();
  const key: (number | null)[] = new Array(KEY_LEN).fill(null);

  // Pin key bytes from any crib the caller supplied.
  const pinned = new Set<number>();
  if (crib) {
    for (let i = 0; i < crib.length && i < N; i++) {
      const r = keyIndex(i);
      key[r] = (cipher[i] - crib[i]) & 0xff;
      pinned.add(r);
    }
  }

  // Indices belonging to each key position.
  const classIdx: number[][] = Array.from({ length: KEY_LEN }, () => []);
  for (let i = 0; i < N; i++) classIdx[keyIndex(i)].push(i);

  // Unigram seed for the un-pinned positions.
  for (let r = 0; r < KEY_LEN; r++) {
    if (key[r] !== null) continue;
    key[r] = bestByte(classIdx[r], k =>
      classIdx[r].reduce((s, i) => s + model.uni[(cipher[i] - k) & 0xff], 0));
  }

  // Bigram hill-climb: adjacent body bytes fall in adjacent key
  // positions, so refining each key byte against its decoded neighbours
  // resolves the ambiguity a per-position unigram score leaves behind.
  for (let pass = 0; pass < 40; pass++) {
    let changed = false;
    for (let r = 0; r < KEY_LEN; r++) {
      if (pinned.has(r)) continue;
      const nk = bestByte(classIdx[r], k => {
        let s = 0;
        for (const i of classIdx[r]) {
          const p = (cipher[i] - k) & 0xff;
          if (i > 0)     s += model.bi[(cipher[i - 1] - (key[keyIndex(i - 1)] as number)) & 0xff][p];
          if (i + 1 < N) s += model.bi[p][(cipher[i + 1] - (key[keyIndex(i + 1)] as number)) & 0xff];
        }
        return s;
      });
      if (nk !== key[r]) { key[r] = nk; changed = true; }
    }
    if (!changed) break;
  }

  const finalKey = key.map(k => k ?? 0);
  const plainBody = decryptBody(cipher, finalKey);
  return { key: finalKey, plainBody, confidence: textConfidence(plainBody) };
}

function bestByte(idx: number[], score: (k: number) => number): number {
  if (idx.length === 0) return 0;
  let best = 0, bestS = -Infinity;
  for (let k = 0; k < 256; k++) {
    const s = score(k);
    if (s > bestS) { bestS = s; best = k; }
  }
  return best;
}

function decryptBody(cipher: Uint8Array, key: number[]): Uint8Array {
  const out = new Uint8Array(cipher.length);
  for (let i = 0; i < cipher.length; i++) out[i] = (cipher[i] - key[keyIndex(i)]) & 0xff;
  return out;
}

// Fraction of bytes that look like ordinary document text — used to
// report recovery confidence to the UI.
function textConfidence(body: Uint8Array): number {
  if (body.length === 0) return 0;
  let ok = 0;
  for (const b of body) {
    if (b === PARA_END || b === TAB || b === SOFT_NL) ok++;
    else if (b >= 0x20 && b <= 0x7e) ok++;
    else if (b >= 0x80) ok += 0.5;  // accented text is plausible but rarer
  }
  return ok / body.length;
}

// =============================================================================
// English character model (self-contained, built once from an embedded
// sample so the browser needs no external data).
// =============================================================================

interface Model { uni: Float64Array; bi: Float64Array[]; }
let MODEL: Model | null = null;

function englishModel(): Model {
  if (MODEL) return MODEL;
  const V = 256;
  const uniC = new Float64Array(V).fill(0.02);
  const biC: Float64Array[] = Array.from({ length: V }, () => new Float64Array(V).fill(0.02));
  // Paragraph breaks in the body are 0x00, so map sentence/line breaks in
  // the sample to 0x00 to teach the model that letters precede/follow it.
  const sample = ENGLISH_SAMPLE.replace(/[.\n]/g, '\0');
  let prev = -1;
  for (let i = 0; i < sample.length; i++) {
    const c = sample.charCodeAt(i) & 0xff;
    uniC[c]++;
    if (prev >= 0) biC[prev][c]++;
    prev = c;
  }
  const uni = new Float64Array(V);
  const uniSum = uniC.reduce((a, b) => a + b, 0);
  for (let i = 0; i < V; i++) uni[i] = Math.log(uniC[i] / uniSum);
  const bi: Float64Array[] = [];
  for (let a = 0; a < V; a++) {
    const rowSum = biC[a].reduce((s, x) => s + x, 0);
    const row = new Float64Array(V);
    for (let b = 0; b < V; b++) row[b] = Math.log(biC[a][b] / rowSum);
    bi.push(row);
  }
  MODEL = { uni, bi };
  return MODEL;
}

// A few KB of ordinary English prose.  Content is irrelevant beyond being
// representative of everyday letter/word statistics; it is only used to
// score candidate decryptions.
const ENGLISH_SAMPLE = `
The morning light came slowly across the fields and the town began to wake.
People walked to work along the quiet streets, talking about the weather and
the news of the day. In the small office by the river a woman opened her diary
and started to write a letter to an old friend. She wrote about her family, her
garden and the long summer that had passed, and she asked after his children and
his health. The words came easily because there was so much to say and so little
time to say it. When the letter was finished she read it through once more, made a
few small changes, and folded it carefully into an envelope. Outside the window a
boy was selling papers on the corner and a bus went past with its lights still on.
It is a simple thing to write down what you think and feel, but it is one of the
oldest and most useful things that people do. A note left on the kitchen table, a
list of jobs for the week, a report for the office, a story for a child at night:
all of these begin with a single word and grow from there. Good writing is clear
and honest. It says what it means and it does not waste the reader's time. The
best advice is to write the way you speak, to keep your sentences short, and to
read your work aloud so that you can hear where it stumbles. Every document, long
or short, is really just a conversation between the writer and the reader, carried
across time and distance by a handful of letters on a page. When you save your work
you keep that conversation safe, and years later you can open the file again and
find the same words waiting for you, exactly as you left them, ready to be read.
This is the writing of some words and the making of a plain and ordinary record.
`;

// =============================================================================
// Body text -> UTF-8 plain text
// =============================================================================

// Turn a (decrypted) body record into readable text.  Paragraph markers
// become newlines, tabs stay tabs, and the Psion 8-bit set is mapped to
// Unicode so accented characters survive.
export function bodyToText(body: Uint8Array): string {
  let out = '';
  for (const b of body) {
    if (b === PARA_END || b === SOFT_NL) out += '\n';
    else if (b === TAB) out += '\t';
    else if (b === LINE_BREAK) out += '\n';
    else if (b === VIS_SPACE) out += ' ';
    else if (b >= 0x20 && b <= 0x7e) out += String.fromCharCode(b);
    else if (b >= 0x80) out += String.fromCharCode(PSION_HIGH[b - 0x80]);
    // other control bytes are dropped
  }
  return out;
}

export interface SiboWordText {
  text: string;
  wasEncrypted: boolean;
  confidence: number;   // 1 for plain files; recovery confidence otherwise
}

// Top-level: extract the document text from a SIBO Word file, recovering
// the password automatically when the file is protected.  `crib` pins the
// key for protected files whose text is too short to recover confidently.
// Returns null if the file is not a SIBO Word document or has no body.
export function extractSiboWordText(bytes: Uint8Array, crib?: Uint8Array): SiboWordText | null {
  const info = scanSiboWord(bytes);
  if (!info || !info.body) return null;
  const raw = bytes.subarray(info.body.start, info.body.end);
  if (!info.encrypted) {
    return { text: bodyToText(raw), wasEncrypted: false, confidence: 1 };
  }
  const rec = recoverBodyKey(raw, crib);
  return { text: bodyToText(rec.plainBody), wasEncrypted: true, confidence: rec.confidence };
}

// Produce a decrypted copy of the whole file with the password removed —
// the body is replaced with its plaintext and the header flags reset to
// "plain", so the result opens on any Psion (or in our own converter)
// without a password.  Returns null if the file is not encrypted SIBO
// Word or has no body.
export function decryptSiboWordFile(bytes: Uint8Array, crib?: Uint8Array): Uint8Array | null {
  const info = scanSiboWord(bytes);
  if (!info || !info.encrypted || !info.body) return null;
  const raw = bytes.subarray(info.body.start, info.body.end);
  const { plainBody } = recoverBodyKey(raw, crib);

  const out = new Uint8Array(bytes);
  out.set(plainBody, info.body.start);
  // Reset the document flags to "plain" and blank the key field (0xEA is
  // the filler a genuine plain file carries there).
  out[FLAG_OFFSET] = 0x01; out[FLAG_OFFSET + 1] = 0x00;
  out[FLAG_OFFSET + 2] = 0x00; out[FLAG_OFFSET + 3] = 0x00;
  for (let i = 0; i < BLOCK_LEN; i++) out[KEY_OFFSET + i] = 0xea;
  return out;
}
