// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// EPOC32 password-protected Word documents (.wrd on the Series 5 / 5mx /
// 5mxPro / MC218 / Revo / Osaris / Series 7 / netBook / netpad) — detect
// them, and read the text back out without the password.
//
// Everything below was worked out by driving a real ROM in this
// emulator: the stock C:\Documents\Word file was opened on a 5mx, known
// text typed into it, "Password..." (Shift+Ctrl+Q) set, and the saved
// file pulled back off the machine over the Remote Link — first without
// a password, then with one, then with a different one, then over a much
// longer document.  Comparing those pairs gives the format exactly.
//
// ── What protection changes in the file ─────────────────────────────
//
// The 16-byte UID header is untouched (it is still UID3 = 0x1000007F,
// a Word document).  Two things happen inside:
//
//   * A new section, UID 0x100000CD, is prepended at offset 0x14 — 41
//     bytes: a one-byte length (0xA2 → 40) and 40 bytes of password
//     verification material.  Its content depends only on the password
//     (byte-identical across two different documents protected with the
//     same one) but it is not the key: it is what the app checks a typed
//     password against.  Everything else in the file shifts up by 41
//     bytes and the section table gains this entry.
//
//   * The *text stream* — the Text Section, UID 0x10000106, which holds
//     [cardinal character count][characters] — is enciphered in place,
//     padded out with 0x30 bytes to a whole multiple of 32.  Nothing
//     else is touched: the styles, page layout, text-layout (formatting
//     runs) and embedded-object sections all stay in clear text, and so
//     does any embedded-object data that follows the text inside the
//     same section.
//
// ── The cipher ──────────────────────────────────────────────────────
//
//     cipher[i] = (plain[i] + key[i mod 32]) mod 256
//
// A 32-byte additive keystream, repeated — no chaining.  The same
// password gives the same 32 key bytes in every Word document we made
// (three of them), though what sets the keystream's *phase* within a
// file is not settled: Sheet, which protects several streams the same
// way, phases them differently again.  None of that matters here,
// because the key is recovered relative to the enciphered run itself
// rather than derived from the password or a file position.
//
// ── Reading it without the password ─────────────────────────────────
//
// The scheme hands us two pieces of known plaintext, which is what makes
// this recoverable rather than merely weak:
//
//   1. The stream begins with EPOC's cardinal-encoded character count,
//      and the character count is also recorded — in clear text — in the
//      Text Layout Section's paragraph list.  So the first one or two
//      plaintext bytes are known exactly, and with them the length of
//      the encrypted run.
//   2. Everything from the end of the text to the next 32-byte boundary
//      is 0x30 padding, so the last (32 - bodyLength mod 32) key bytes
//      fall out directly.
//
// The remaining key bytes are recovered the way psion-word-sibo.ts
// recovers SIBO Word's 9-byte key: each key position enciphers its own
// share of the body (every 32nd byte), so the positions are independent
// and each one is chosen to make its share read like English — a
// unigram seed followed by a bigram hill-climb against the shared
// character model.  A document of a few hundred characters gives every
// key position enough samples to settle; a very short one does not, so
// callers can supply a crib (the document's first characters as the user
// remembers them), where each crib byte pins one key position exactly.

import { UID3_WORD, readEpocHeader, isValidEpocHeader } from './epoc-uids.ts';
import { readSectionTable, findSection, type SectionEntry } from './epoc-store.ts';
import { englishModel } from './english-model.ts';

// Section UIDs (psiconv formats/psion/*.psi, plus our own ROM captures).
export const UID_PASSWORD_SECTION = 0x100000CD;
const UID_TEXT_SECTION            = 0x10000106;
const UID_LAYOUT_SECTION          = 0x10000143;

const KEY_LEN  = 32;    // additive keystream period
const PAD_BYTE = 0x30;  // filler between the text and the 32-byte boundary

// Character codes in an EPOC Word body (TXTETEXT.H); shared with word.ts,
// repeated here because this module works on the raw body bytes.
const EParagraphDelimiter = 0x06;
const ELineBreak          = 0x07;
const ETabCharacter       = 0x09;

// =============================================================================
// Detection
// =============================================================================

// True when this is an EPOC document carrying a password section.  Any
// document type can carry one — Sheet protects its workbook the same way
// (epoc-password-sheet.ts), and so do Data files, which nothing here
// decodes.  `isProtectedEpocWord` is the narrower test for this module.
export function isEpocPasswordProtected(bytes: Uint8Array): boolean {
  const header = readEpocHeader(bytes);
  if (!header || !isValidEpocHeader(header)) return false;
  const table = readSectionTable(bytes);
  return table !== null && findSection(table, UID_PASSWORD_SECTION) !== null;
}

export function isProtectedEpocWord(bytes: Uint8Array): boolean {
  const header = readEpocHeader(bytes);
  if (!header || !isValidEpocHeader(header) || header.uid3 !== UID3_WORD) return false;
  return isEpocPasswordProtected(bytes);
}

// =============================================================================
// Recovery
// =============================================================================

export interface EpocRecovery {
  // The whole file with the text stream decrypted in place. Offsets are
  // unchanged (the 0x30 padding is left where it is, decrypted, and the
  // stream's own character count stops readers at the real text), so the
  // result goes straight into the ordinary Word converter.
  file: Uint8Array;
  // Just the decrypted text stream: [cardinal count][characters].
  body: Uint8Array;
  key: Uint8Array;         // the recovered 32-byte keystream
  confidence: number;      // 0..1, share of the body that reads as text
  // How many of the 32 key bytes came out of known plaintext rather than
  // the language model, and how many body characters each modelled byte
  // was chosen from.  A document with only a handful of samples per key
  // position is under-determined — the caller should offer the crib.
  pinnedKeyBytes: number;
  samplesPerKeyByte: number;
}

// Recover a password-protected EPOC Word document.  `crib`, if given, is
// the document's leading text as the user remembers it (mapped to body
// bytes: paragraph breaks 0x06, tabs 0x09); each crib byte pins one key
// position exactly, and the first 32 pin the whole key.
//
// Returns null when the file isn't a protected EPOC Word document, or
// when its text stream can't be located.
export function recoverEpocWord(bytes: Uint8Array, crib?: Uint8Array): EpocRecovery | null {
  if (!isProtectedEpocWord(bytes)) return null;
  const table = readSectionTable(bytes);
  if (!table) return null;
  const text = findSection(table, UID_TEXT_SECTION);
  if (!text || text.end - text.start < 2) return null;

  const region = bytes.subarray(text.start, text.end);
  const bodyLengths = candidateBodyLengths(bytes, table, region.length);
  if (bodyLengths.length === 0) return null;

  // Pick the body length.  When the layout section gave us the character
  // count there is only one candidate; otherwise every candidate is
  // weighed on how much of what it leaves over decrypts to 0x30 filler.
  const chosen = bodyLengths.length === 1
    ? { bodyLen: bodyLengths[0], filler: FILLER_TRUSTED }
    : chooseBodyLength(region, bodyLengths, crib);
  if (!chosen) return null;

  // Now recover the key for real.  The filler is worth using as known
  // plaintext when it checked out above; when it didn't (a document
  // whose padding isn't the 0x30 we've always seen, or one too short to
  // place at all), fall back to the purely modelled key.
  const { bodyLen } = chosen;
  const attempt = recoverKey(region, bodyLen,
    { usePadding: chosen.filler >= FILLER_TRUSTED, useTrailingDelimiter: true, crib });
  if (!attempt) return null;

  const { key, pinned } = attempt;
  const body = decryptRun(region, key, bodyLen);
  const file = new Uint8Array(bytes);
  for (let i = 0; i < paddedLength(bodyLen) && text.start + i < file.length; i++) {
    file[text.start + i] = (region[i] - key[i % KEY_LEN]) & 0xff;
  }
  return {
    file, body, key,
    confidence: bodyConfidence(body),
    pinnedKeyBytes: pinned,
    samplesPerKeyByte: Math.floor(bodyLen / KEY_LEN),
  };
}

// Plaintext length of the text stream: the cardinal character count plus
// that many characters.  The count is in the clear in the Text Layout
// Section, which is where we read it from.  Documents too plain to carry
// a layout section (a single unformatted paragraph) have no encrypted
// object data after the text either, so there the padded run fills the
// section exactly and the length is one of the 32 values that rounds up
// to it.
function candidateBodyLengths(
  bytes: Uint8Array, table: SectionEntry[], regionLen: number,
): number[] {
  const layout = findSection(table, UID_LAYOUT_SECTION);
  if (layout) {
    const chars = readLayoutCharCount(bytes, layout.start, layout.end);
    if (chars !== null) {
      const bodyLen = cardinalSize(chars) + chars;
      if (bodyLen >= 1 && paddedLength(bodyLen) <= regionLen) return [bodyLen];
    }
  }
  const out: number[] = [];
  const end = regionLen - (regionLen % KEY_LEN);   // the run ends on a boundary
  for (let bodyLen = Math.max(1, end - KEY_LEN + 1); bodyLen <= end; bodyLen++) out.push(bodyLen);
  return out;
}

// Weighing a candidate body length: decrypt with a key built from the
// cardinal (and any crib) alone, then look at what the candidate leaves
// over.  A candidate that is too short leaves real text where the filler
// should be; one that is too long swallows filler into the body, which
// costs it the filler bytes it could have counted.  A leftover byte that
// decrypts to 0x30 is therefore evidence for the candidate and one that
// doesn't is evidence against, weighing double so that a candidate three
// bytes short of the truth (28 filler bytes right, three bytes of real
// text wrong) loses to the truth (28 right, none wrong).
function fillerEvidence(region: Uint8Array, key: Uint8Array, bodyLen: number): number {
  const padLen  = Math.min(paddedLength(bodyLen), region.length) - bodyLen;
  const matches = countFiller(region, key, bodyLen);
  return matches - 2 * (padLen - matches);
}

// Enough filler evidence to treat the 0x30 assumption as confirmed, and
// so to use the padding as known plaintext in the real recovery.
const FILLER_TRUSTED = 4;

function chooseBodyLength(
  region: Uint8Array, candidates: number[], crib?: Uint8Array,
): { bodyLen: number; filler: number } | null {
  let best: { bodyLen: number; filler: number; score: number } | null = null;
  for (const bodyLen of [...candidates].sort((a, b) => a - b)) {
    // The search key deliberately assumes neither the padding nor the
    // closing paragraph mark: assuming either would make the checks
    // below come out true whatever the candidate.
    const attempt = recoverKey(region, bodyLen,
      { usePadding: false, useTrailingDelimiter: false, crib });
    if (!attempt) continue;
    const filler = fillerEvidence(region, attempt.key, bodyLen);
    const score  = modelScore(decryptRun(region, attempt.key, bodyLen));
    if (!best || filler > best.filler || (filler === best.filler && score > best.score)) {
      best = { bodyLen, filler, score };
    }
  }
  return best;
}

function paddedLength(bodyLen: number): number {
  return Math.ceil(bodyLen / KEY_LEN) * KEY_LEN;
}

function cardinalSize(value: number): number {
  if (value < 0x80)   return 1;
  if (value < 0x4000) return 2;
  return 4;
}

function encodeCardinal(value: number): number[] {
  if (value < 0x80)   return [value << 1];
  if (value < 0x4000) {
    const v = (value << 2) | 1;
    return [v & 0xff, (v >> 8) & 0xff];
  }
  const v = ((value << 3) | 3) >>> 0;
  return [v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff];
}

// =============================================================================
// Keystream recovery
// =============================================================================

interface KeyAttempt { key: Uint8Array; pinned: number; }

interface KeyOptions {
  usePadding: boolean;            // trust the 0x30 filler
  useTrailingDelimiter: boolean;  // trust the closing paragraph mark
  crib?: Uint8Array;
}

// Samples per key position the hill-climb looks at.  Sixty-four
// characters is far more than the statistics need to settle, and the cap
// keeps the search on a long document from costing what a 32-candidate
// sweep over every one of its bytes would.
const MAX_SAMPLES = 64;

// Build the 32-byte keystream for one candidate body length.  Positions
// fixed by known plaintext (the leading cardinal, the closing paragraph
// mark, the trailing padding, any crib) are pinned; the rest are chosen
// to make the body read like English.  The two "use" flags exist because
// the padding and the closing paragraph mark are the only things here
// that can't be checked against the file itself: the body-length search
// leaves both out precisely so it can test them.
function recoverKey(
  region: Uint8Array, bodyLen: number, opts: KeyOptions,
): KeyAttempt | null {
  const padded = paddedLength(bodyLen);
  if (padded > region.length || bodyLen < 1) return null;

  const key = new Uint8Array(KEY_LEN);
  const pinned = new Array<boolean>(KEY_LEN).fill(false);
  const pin = (plain: number, at: number) => {
    const r = at % KEY_LEN;
    if (pinned[r]) return;
    key[r] = (region[at] - plain) & 0xff;
    pinned[r] = true;
  };

  // 1. The leading cardinal — we know the character count, so we know
  //    the bytes that encode it.
  const cardSize = cardinalSizeForBody(bodyLen);
  const chars = bodyLen - cardSize;
  for (const [i, b] of encodeCardinal(chars).entries()) {
    if (i < region.length) pin(b, i);
  }
  // 2. A caller-supplied crib is the document's own first characters,
  //    which start after the cardinal.
  if (opts.crib) {
    for (let i = 0; i < opts.crib.length && cardSize + i < bodyLen; i++) {
      pin(opts.crib[i], cardSize + i);
    }
  }
  // 3. Every Word body ends with a paragraph delimiter.
  if (opts.useTrailingDelimiter) pin(EParagraphDelimiter, bodyLen - 1);
  // 4. The 0x30 padding between the text and the 32-byte boundary.
  if (opts.usePadding) {
    for (let at = bodyLen; at < padded; at++) pin(PAD_BYTE, at);
  }

  // Body indices (never the padding) each key position is scored over.
  const classIdx: number[][] = Array.from({ length: KEY_LEN }, () => []);
  for (let i = 0; i < bodyLen; i++) {
    const cls = classIdx[i % KEY_LEN];
    if (cls.length < MAX_SAMPLES) cls.push(i);
  }

  const model = englishModel(EParagraphDelimiter);

  // Unigram seed for every position we don't already know.
  for (let r = 0; r < KEY_LEN; r++) {
    if (pinned[r]) continue;
    key[r] = bestByte(k => {
      let s = 0;
      for (const i of classIdx[r]) s += model.uni[(region[i] - k) & 0xff];
      return s;
    });
  }

  // Bigram hill-climb: neighbouring body bytes sit in neighbouring key
  // positions, so scoring each candidate against its already-decoded
  // neighbours resolves what a per-position unigram score cannot.
  for (let pass = 0; pass < 40; pass++) {
    let changed = false;
    for (let r = 0; r < KEY_LEN; r++) {
      if (pinned[r]) continue;
      const next = bestByte(k => {
        let s = 0;
        for (const i of classIdx[r]) {
          const p = (region[i] - k) & 0xff;
          if (i > 0)          s += model.bi[(region[i - 1] - key[(i - 1) % KEY_LEN]) & 0xff][p];
          if (i + 1 < bodyLen) s += model.bi[p][(region[i + 1] - key[(i + 1) % KEY_LEN]) & 0xff];
        }
        return s;
      });
      if (next !== key[r]) { key[r] = next; changed = true; }
    }
    if (!changed) break;
  }
  return { key, pinned: pinned.filter(Boolean).length };
}

// Average bigram log-likelihood of a candidate decryption — how much the
// language model believes it.  Used to pick between candidate body
// lengths and known-plaintext assumptions; unlike a printable-character
// count it separates real prose from the plausible-looking repetition a
// wrong key produces.
function modelScore(body: Uint8Array): number {
  if (body.length < 2) return -Infinity;
  const model = englishModel(EParagraphDelimiter);
  let total = 0;
  for (let i = 1; i < body.length; i++) total += model.bi[body[i - 1]][body[i]];
  return total / (body.length - 1);
}

// The cardinal at the head of the stream is 1, 2 or 4 bytes depending on
// the count it encodes, and the count is (bodyLen - that size) — so the
// size is whichever choice is self-consistent.
function cardinalSizeForBody(bodyLen: number): number {
  for (const size of [1, 2, 4]) {
    const chars = bodyLen - size;
    if (chars >= 0 && cardinalSize(chars) === size) return size;
  }
  return 1;
}

function bestByte(score: (k: number) => number): number {
  let best = 0, bestScore = -Infinity;
  for (let k = 0; k < 256; k++) {
    const s = score(k);
    if (s > bestScore) { bestScore = s; best = k; }
  }
  return best;
}

// How many of the bytes a candidate body length leaves over decrypt to
// the 0x30 filler.  For the right length that is all of them; for a
// wrong one, real text (or another document's bytes) sits there instead.
function countFiller(region: Uint8Array, key: Uint8Array, bodyLen: number): number {
  let matches = 0;
  for (let i = bodyLen; i < paddedLength(bodyLen) && i < region.length; i++) {
    if (((region[i] - key[i % KEY_LEN]) & 0xff) === PAD_BYTE) matches++;
  }
  return matches;
}

function decryptRun(region: Uint8Array, key: Uint8Array, bodyLen: number): Uint8Array {
  const out = new Uint8Array(Math.min(bodyLen, region.length));
  for (let i = 0; i < out.length; i++) out[i] = (region[i] - key[i % KEY_LEN]) & 0xff;
  return out;
}

// Share of the decrypted body that reads as ordinary document text —
// reported to the UI, and used to choose between candidate recoveries.
function bodyConfidence(body: Uint8Array): number {
  if (body.length === 0) return 0;
  let ok = 0;
  for (const b of body) {
    if (b === EParagraphDelimiter || b === ELineBreak || b === ETabCharacter) ok++;
    else if (b >= 0x20 && b <= 0x7e) ok++;
    else if (b >= 0xa0)              ok += 0.5;   // accented text: plausible, rarer
  }
  return ok / body.length;
}

// =============================================================================
// Text Layout Section: how many characters the body holds
// =============================================================================
//
// Spec: psiconv formats/psion/Text_Layout_Section.psi (the same structure
// word.ts parses for formatting).  We only need the paragraph lengths, so
// this walks the section and sums them — every layout list is byte-count
// prefixed, so the codes inside can be skipped wholesale.
//
//   W    section flag (0x0001 = with styles)
//   Paragraph Type List    B count, then per type:
//                            L type number
//                            L byte count + paragraph layout bytes
//                            B style id            (with styles only)
//                            L byte count + character layout bytes
//   Paragraph Element List L count, then per paragraph:
//                            L characters in this paragraph   <- summed
//                            B type reference (0 = its own layout)
//                          if 0: paragraph layout list,
//                                B style id (with styles only),
//                                L inline-element count
function readLayoutCharCount(input: Uint8Array, start: number, end: number): number | null {
  try {
    let pos = start;
    const u8 = () => {
      if (pos >= end) throw new Error('OOB');
      return input[pos++];
    };
    const u32 = () => {
      if (pos + 4 > end) throw new Error('OOB');
      const v = (input[pos] | (input[pos + 1] << 8) |
                 (input[pos + 2] << 16) | (input[pos + 3] << 24)) >>> 0;
      pos += 4;
      return v;
    };
    const skipList = () => {
      const n = u32();
      if (pos + n > end) throw new Error('OOB');
      pos += n;
    };

    if (start + 2 > end) return null;
    const withStyles = (input[start] | (input[start + 1] << 8)) === 0x0001;
    pos = start + 2;

    const typeCount = u8();
    for (let i = 0; i < typeCount; i++) {
      u32();                       // type number
      skipList();                  // paragraph layout
      if (withStyles) u8();        // base style id
      skipList();                  // character layout
    }

    const paraCount = u32();
    if (paraCount === 0 || paraCount > end - start) return null;
    let total = 0;
    for (let i = 0; i < paraCount; i++) {
      total += u32();
      const typeRef = u8();
      if (typeRef === 0) {
        skipList();                // this paragraph's own layout
        if (withStyles) u8();
        u32();                     // inline-element count
      }
    }
    return total > 0 ? total : null;
  } catch {
    return null;                   // unparseable — fall back to the scan
  }
}
