// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// EPOC Word (.wrd) -> RTF (Rich Text Format).
//
// Word documents on Series 5 store text via Symbian's `CRichText`
// inside a Permanent File Store (UID1 = 0x10000039, UID3 = 0x1000007F).
// PFS holds multiple internal streams — text body, paragraph markup,
// character format runs, embedded pictures, named style sheet — keyed
// by a stream dictionary at the file offset stored in bytes 16-19.
//
// Stream UIDs we care about (psiconv data.h, verified across our
// reference samples):
//   0x10000106  Text Section          (the body characters)
//   0x10000143  Text Layout Section   (paragraph + character markup)
//   0x10000104  Word Styles Section   (named styles incl. headings)
//
// Markup encoding follows Frodo Looijaard's reverse-engineered specs in
// psiconv's formats/psion/ directory (Text_Layout_Section.psi +
// Layout_Codes.psi).  Parser below is a clean TypeScript reimplementation
// — we don't link any psiconv code (it's GPL and we don't need to).
//
// Special-character codes in the text body, from TXTETEXT.H in the EPOC
// SDK:
//    0x06 EParagraphDelimiter (end of paragraph)
//    0x07 ELineBreak          (Shift+Enter soft break)
//    0x08 EPageBreak
//    0x09 ETabCharacter
//    0x0B ENonBreakingHyphen  0x0C EPotentialHyphen
//    0x0E EPictureCharacter   (inline picture/object placeholder)
//    0x0F EVisibleSpace       0x10 ENonBreakingSpace
//
// Embedded pictures/charts are objects: each EPictureCharacter in the
// body pairs with a type-1 inline run in the layout section that points
// at an Embedded Object Section (icon + display size + the embedding
// app's document payload).  Sketch payloads contain a standard bitmap
// we render to PNG (via sketch.ts) and splice back at the right
// EPictureCharacter at the document's own display size.  Sheet payloads
// (charts) have no rasterised preview, so we extract the chart's cell
// data instead and emit it as a small table.  Structure decoded from
// the "Welcome to ..." samples in the EPOC device ROMs.

import { EPOC_HEADER_BYTES, readUint32LE } from './epoc-uids.ts';
import { findEmbeddedBitmaps, renderBitmapAt } from './sketch.ts';
import { scanForCellList, type Cell } from './sheet.ts';

// ----- Stream UIDs (psiconv data.h) -----
const UID_TEXT_SECTION         = 0x10000106;
const UID_LAYOUT_SECTION       = 0x10000143;
const UID_WORD_STYLES_SECTION  = 0x10000104;

// ----- Character codes in the text body (TXTETEXT.H) -----
const EParagraphDelimiter      = 0x06;
const ELineBreak               = 0x07;
const EPageBreak               = 0x08;
const ETabCharacter            = 0x09;
const ENonBreakingTab          = 0x0A;
const ENonBreakingHyphen       = 0x0B;
const EPotentialHyphen         = 0x0C;
const EPictureCharacter        = 0x0E;
const EVisibleSpace            = 0x0F;
const ENonBreakingSpace        = 0x10;

// ----- Layout code IDs (psiconv Layout_Codes.psi, plus TXTFRMAT.H) -----
// Paragraph layout codes.
const P_FILL_COLOR             = 0x01;  // 3B (RGB)
const P_LEFT_MARGIN            = 0x02;  // L  (twips)
const P_RIGHT_MARGIN           = 0x03;  // L
const P_FIRST_LINE_INDENT      = 0x04;  // L
const P_ALIGNMENT              = 0x05;  // u8: 0=left, 1=center, 2=right, 3=full
const P_LINE_SPACING           = 0x07;  // L  (twips)
const P_LINE_SPACING_EXACT     = 0x08;  // u8: 0=min, 1=exact
const P_SPACE_BEFORE           = 0x09;  // L  (twips)
const P_SPACE_AFTER            = 0x0A;  // L  (twips)
const P_KEEP_TOGETHER          = 0x0B;  // u8
const P_KEEP_WITH_NEXT         = 0x0C;  // u8
const P_BULLET                 = 0x15;  // BListB
// Character layout codes.
const C_COLOR                  = 0x19;  // 3B (RGB)
const C_FONT_HEIGHT            = 0x1C;  // u32 twips (1 pt = 20 twips)
const C_FONT_POSTURE           = 0x1D;  // u8: 0=upright, 1=italic
const C_FONT_WEIGHT            = 0x1E;  // u8: 0=normal,  1=bold
const C_FONT_UNDERLINE         = 0x20;  // u8: 0=off,     1=on
const C_FONT_STRIKE            = 0x21;  // u8
const C_FONT_TYPEFACE          = 0x22;  // length-byte + name + flag

// Style IDs in the Word Styles Section (psiconv Word_Styles_Section.psi).
// In the default Word template these are non-removable and reliable.
const STYLE_NORMAL             = 0x00;
const STYLE_HEADING_1          = 0xFF;
const STYLE_HEADING_2          = 0xFE;
const STYLE_HEADING_3          = 0xFD;

const MIN_BODY_CHARS = 16;

// ----- Default Word styles for documents without a Styles Section -----
// Per psiconv Word_Styles_Section.psi these are the non-removable
// defaults in the standard Word template.
const DEFAULT_STYLES = new Map<number, ResolvedStyle>([
  [STYLE_NORMAL,    { paraLayout: defaultParaLayout(), charLayout: defaultCharLayout(),
                      name: 'Normal' }],
  [STYLE_HEADING_1, { paraLayout: defaultParaLayout(),
                      charLayout: { ...defaultCharLayout(), bold: true,  fontHeight: 280 },
                      name: 'Heading 1' }],
  [STYLE_HEADING_2, { paraLayout: defaultParaLayout(),
                      charLayout: { ...defaultCharLayout(), bold: true,  fontHeight: 240 },
                      name: 'Heading 2' }],
  [STYLE_HEADING_3, { paraLayout: defaultParaLayout(),
                      charLayout: { ...defaultCharLayout(), italic: true, underline: true },
                      name: 'Heading 3' }],
]);

export function convertWordToRtf(input: Uint8Array): Uint8Array | null {
  const dict = parseStreamDictionary(input);
  // Real Word docs always have a PFS dictionary — use it both for body
  // location AND for the layout + styles parse.  Files without a dict
  // (broken / partial dumps / synthetic test fixtures) fall back to a
  // heuristic body finder and the built-in default styles.
  const body = (dict && findBody(input, dict)) ?? findBodyByHeuristic(input);
  if (!body) return null;

  const styles = (dict && parseWordStylesSection(input, dict)) ?? DEFAULT_STYLES;
  const layout = dict ? parseLayoutSection(input, dict, body.text.length, styles) : null;
  // Prefer the layout section's own object table (knows which picture
  // character maps to which object, the displayed size, and whether
  // the object is a picture or a chart).  Whole-file bitmap scan is
  // the fallback for documents without a parseable layout.
  const objectAt = layout ? resolveObjects(input, layout, body.text.length) : null;
  const images = objectAt ? [] : collectImages(input);

  return buildRtf(body.text, images, objectAt, layout, styles);
}

// =============================================================================
// PFS stream dictionary
// =============================================================================

interface StreamEntry { uid: number; start: number; end: number; }

function parseStreamDictionary(input: Uint8Array): StreamEntry[] | null {
  if (input.length < EPOC_HEADER_BYTES + 4) return null;
  const dictOffset = readUint32LE(input, EPOC_HEADER_BYTES);
  if (dictOffset < EPOC_HEADER_BYTES + 4 || dictOffset >= input.length) return null;

  // The dictionary count is a single-byte cardinal (low bit clear).
  const countByte = input[dictOffset];
  if ((countByte & 1) !== 0) return null;
  const count = countByte >> 1;
  if (count === 0 || count > 32) return null;

  const entriesStart = dictOffset + 1;
  if (entriesStart + count * 8 > input.length) return null;

  const raw: { uid: number; offset: number }[] = [];
  for (let i = 0; i < count; i++) {
    const uid = readUint32LE(input, entriesStart + i * 8);
    const off = readUint32LE(input, entriesStart + i * 8 + 4);
    if (off < EPOC_HEADER_BYTES + 4 || off >= dictOffset) return null;
    raw.push({ uid, offset: off });
  }
  raw.sort((a, b) => a.offset - b.offset);

  const entries: StreamEntry[] = [];
  for (let i = 0; i < raw.length; i++) {
    const end = i + 1 < raw.length ? raw[i + 1].offset : dictOffset;
    entries.push({ uid: raw[i].uid, start: raw[i].offset, end });
  }
  return entries;
}

function findStream(dict: StreamEntry[], uid: number): StreamEntry | null {
  return dict.find(s => s.uid === uid) ?? null;
}

// =============================================================================
// Body extraction
// =============================================================================

interface TextBody { text: Uint8Array; }

function findBody(input: Uint8Array, dict: StreamEntry[]): TextBody | null {
  // Prefer the well-known UID; fall back to a heuristic for foreign files
  // where the body stream UID isn't exactly 0x10000106 (older variants).
  const direct = findStream(dict, UID_TEXT_SECTION);
  if (direct) {
    const body = extractBodyFromStream(input, direct);
    if (body) return body;
  }
  return findBodyByContent(input, dict);
}

function extractBodyFromStream(input: Uint8Array, s: StreamEntry): TextBody | null {
  // The Text Section stream starts with a cardinal-encoded character
  // count followed by exactly that many characters.  The stream often
  // continues past the text — embedded-object metadata, etc. — so we
  // MUST stop at the cardinal length rather than reading to stream end.
  // (Welcome to Series 5mx has 7647 bytes in its body stream but only
  // 2393 characters of real text; the rest is object-section data that
  // confuses both the layout parser and the RTF output.)
  if (s.start >= s.end) return null;
  const c0 = input[s.start];
  let length: number;
  let pos = s.start;
  if ((c0 & 1) === 0) {
    length = c0 >>> 1;
    pos += 1;
  } else if ((c0 & 3) === 1) {
    if (s.start + 2 > s.end) return null;
    length = ((c0 | (input[s.start + 1] << 8)) >>> 2);
    pos += 2;
  } else if ((c0 & 7) === 3) {
    if (s.start + 4 > s.end) return null;
    length = readUint32LE(input, s.start) >>> 3;
    pos += 4;
  } else {
    return null;
  }
  const end = Math.min(pos + length, s.end);
  if (end - pos < MIN_BODY_CHARS) return null;
  return { text: input.subarray(pos, end) };
}

// Pick the stream with the highest valid-text *ratio* (not raw count).
// Tiny bodies (<100 chars) would otherwise lose to a verbose style sheet
// that happens to contain many font/style name strings.
function findBodyByContent(input: Uint8Array, dict: StreamEntry[]): TextBody | null {
  let best: StreamEntry | null = null;
  let bestRatio = -1;
  for (const s of dict) {
    const len = s.end - s.start;
    if (len < 4) continue;
    let valid = 0, paraBreaks = 0;
    for (let k = s.start; k < s.end; k++) {
      const b = input[k];
      if (isValidTextChar(b)) valid++;
      if (b === EParagraphDelimiter) paraBreaks++;
    }
    if (paraBreaks === 0) continue;
    const ratio = valid / len;
    if (ratio < 0.85) continue;
    if (ratio > bestRatio) { bestRatio = ratio; best = s; }
  }
  return best ? extractBodyFromStream(input, best) : null;
}

// Strip stale `extractBodyFromStream` callers below.  `findBodyByContent`
// uses the same routine so it naturally picks up the cardinal-length fix.

// Last-resort body finder for files whose PFS dictionary is missing or
// malformed.  Picks the longest contiguous run of valid EPOC text
// bytes, then trims off any leading PFS-metadata "paragraph" that
// doesn't look like real prose.
function findBodyByHeuristic(input: Uint8Array): TextBody | null {
  let runStart = -1, bestStart = 0, bestLen = 0;
  for (let i = EPOC_HEADER_BYTES; i < input.length; i++) {
    if (isValidTextChar(input[i])) {
      if (runStart === -1) runStart = i;
    } else if (runStart !== -1) {
      const len = i - runStart;
      if (len > bestLen) { bestLen = len; bestStart = runStart; }
      runStart = -1;
    }
  }
  if (runStart !== -1) {
    const len = input.length - runStart;
    if (len > bestLen) { bestLen = len; bestStart = runStart; }
  }
  if (bestLen < MIN_BODY_CHARS) return null;
  const trimmed = trimLeadingNoise(input.subarray(bestStart, bestStart + bestLen));
  if (!trimmed || !mostlyReadable(trimmed)) return null;
  return { text: trimmed };
}

function trimLeadingNoise(view: Uint8Array): Uint8Array | null {
  // Drop leading "paragraphs" that don't read like prose; the run-finder
  // sometimes picks up a few stray printable bytes from a stream header.
  // If NO paragraph reads like prose the candidate isn't a document
  // body at all (several ROMs carry non-document files under Word's
  // UID whose binary innards otherwise convert to pages of escaped
  // Latin-1 garbage) — reject rather than emit junk.
  let scanFrom = 0;
  while (scanFrom < view.length) {
    let paraEnd = scanFrom;
    while (paraEnd < view.length && view[paraEnd] !== EParagraphDelimiter) paraEnd++;
    const para = view.subarray(scanFrom, paraEnd);
    if (looksLikeProse(para)) return view.subarray(scanFrom);
    scanFrom = paraEnd + 1;
  }
  return null;
}

// Heuristic-extracted bodies must be predominantly readable text:
// real document prose is overwhelmingly ASCII even in accented
// European languages, while misidentified binary runs are mostly
// Latin-1 high bytes.
function mostlyReadable(view: Uint8Array): boolean {
  if (view.length === 0) return false;
  let readable = 0;
  for (let i = 0; i < view.length; i++) {
    const b = view[i];
    if ((b >= 0x20 && b <= 0x7E) || b === EParagraphDelimiter ||
        b === ELineBreak || b === ETabCharacter) readable++;
  }
  return readable / view.length >= 0.8;
}

function looksLikeProse(para: Uint8Array): boolean {
  let alpha = 0, other = 0;
  for (let i = 0; i < para.length; i++) {
    const b = para[i];
    if ((b >= 0x41 && b <= 0x5A) || (b >= 0x61 && b <= 0x7A)) alpha++;
    else if (b === 0x20 || (b >= 0x30 && b <= 0x39) ||
             b === 0x2E || b === 0x2C || b === 0x27 || b === 0x21 ||
             b === 0x3F || b === 0x3A || b === 0x3B || b === 0x2D ||
             b === ETabCharacter || b === ELineBreak ||
             b === ENonBreakingSpace || b === EVisibleSpace ||
             b === ENonBreakingHyphen || b === EPotentialHyphen) {
      // benign filler
    } else {
      other++;
    }
  }
  return alpha >= 2 && other <= alpha;
}

// =============================================================================
// Word Styles Section parser
// =============================================================================
//
// Spec: psiconv formats/psion/Word_Styles_Section.psi
//
//   Normal Style:
//     Paragraph Layout List   (LListB)
//     Character Layout List   (LListB)
//     L                       Hotkey (ASCII char in low byte)
//
//   Style Hotkeys (BListL):  one u32 hotkey per "other" style.
//
//   Other Styles (BListE):
//     String                  Style name (SListB)
//     ID                      Style kind (0x1000004C non-removable,
//                                          0x1000004F removable)
//     L                       Outline level (5MX only, 0 elsewhere)
//     Character Layout List
//     Paragraph Layout List
//
//   Style Trailer:  one 0xFF byte per "other" style (purpose unknown).
//
// Style IDs are assigned from 0xFF *down* in the order styles appear in
// the Other Styles list.  The standard Word template defines exactly
// three non-removable styles (Heading 1/2/3 at 0xFF/0xFE/0xFD) plus
// Normal at 0x00.

function parseWordStylesSection(
  input: Uint8Array, dict: StreamEntry[],
): Map<number, ResolvedStyle> | null {
  const stream = findStream(dict, UID_WORD_STYLES_SECTION);
  if (!stream) return null;
  try {
    return parseWordStylesSectionInner(input, stream.start, stream.end);
  } catch {
    return null;
  }
}

function parseWordStylesSectionInner(
  input: Uint8Array, start: number, end: number,
): Map<number, ResolvedStyle> {
  const styles = new Map<number, ResolvedStyle>();
  const cur: Cursor = { pos: start };

  // Normal style.
  const normalPara = parseParagraphLayoutList(input, cur, end);
  const normalChar = parseCharacterLayoutList(input, cur, end);
  readU32(input, cur, end);  // hotkey (unused by RTF output)
  styles.set(STYLE_NORMAL, {
    paraLayout: normalPara,
    charLayout: normalChar,
    name: 'Normal',
  });

  // Hotkeys (BListL).  We don't use them but must skip past.
  const hotkeyCount = readU8(input, cur, end);
  skipBytes(cur, hotkeyCount * 4, end);

  // Other styles (BListE).  Each entry's char layout INHERITS from
  // Normal — overrides are layered on top.
  const otherCount = readU8(input, cur, end);
  for (let i = 0; i < otherCount; i++) {
    const styleId = (0xFF - i) & 0xFF;
    const name    = readSpecialString(input, cur, end);
    skipBytes(cur, 4, end);                        // style kind ID
    skipBytes(cur, 4, end);                        // outline level
    const charLay = parseCharacterLayoutList(input, cur, end, normalChar);
    const paraLay = parseParagraphLayoutList(input, cur, end, normalPara);
    styles.set(styleId, { paraLayout: paraLay, charLayout: charLay, name });
  }
  return styles;
}

// SListB string: special-encoded length, then `length` bytes of ASCII.
// Special encoding (Basic_Elements.html):
//   byte & 3 == 2 : single-byte length = byte >> 2  (0..63)
//   byte & 7 == 5 : two-byte length = (word >> 3) - ??  (handled below)
function readSpecialString(input: Uint8Array, cur: Cursor, end: number): string {
  if (cur.pos >= end) throw new Error('string OOB');
  const b0 = input[cur.pos];
  let length: number;
  if ((b0 & 3) === 2) {
    length = b0 >> 2;
    cur.pos += 1;
  } else if ((b0 & 7) === 5) {
    if (cur.pos + 2 > end) throw new Error('string OOB');
    length = ((b0 | (input[cur.pos + 1] << 8)) >> 3);
    cur.pos += 2;
  } else {
    throw new Error('bad string length');
  }
  if (cur.pos + length > end) throw new Error('string body OOB');
  const bytes = input.subarray(cur.pos, cur.pos + length);
  cur.pos += length;
  return decodeAscii(bytes);
}

function isValidTextChar(b: number): boolean {
  if (b === EParagraphDelimiter)  return true;
  if (b === ELineBreak)           return true;
  if (b === ETabCharacter)        return true;
  if (b === ENonBreakingTab)      return true;
  if (b === ENonBreakingHyphen)   return true;
  if (b === EPotentialHyphen)     return true;
  if (b === EPictureCharacter)    return true;
  if (b === EVisibleSpace)        return true;
  if (b === ENonBreakingSpace)    return true;
  if (b >= 0x20 && b <= 0x7E)     return true;
  if (b >= 0xA0 && b <= 0xFF)     return true;
  return false;
}

// =============================================================================
// Text Layout Section parser
// =============================================================================
//
// Spec: psiconv formats/psion/Text_Layout_Section.psi
//
//   W                       Section flag: 0x0001 = with styles, 0x0000 = without
//   Paragraph Type List     BListE of paragraph types (templates)
//   Paragraph Element List  LListE, one per body paragraph
//   Inline List             LListE of character-level layout runs
//
// Paragraph type element (BListE entry):
//   L                       Type number (1, 2, 3, ...)
//   Paragraph Layout List   LListB of paragraph layout codes
//   B (only with-styles)    Word Style ID
//   Character Layout List   LListB of character layout codes
//
// Paragraph element (LListE entry):
//   L                       Number of characters in this paragraph
//   B                       Paragraph type (0 = own layout, N = reference type N)
//  if type == 0:
//   Paragraph Layout List   LListB
//   B (only with-styles)    Word Style ID
//   L                       Number of inline elements for this paragraph
//
// Inline element (LListE entry):
//   B                       Type (0 = normal layout, 1 = object)
//   L                       Number of characters this layout applies to
//   Character Layout List   LListB
//   ID + offset + 2*Length  (only if type == 1, object reference)

export interface RGB { r: number; g: number; b: number; }

export interface ParaLayout {
  alignment:        number | null;   // 0=left 1=center 2=right 3=justified
  hasBullet:        boolean;
  spaceBeforeTwips: number | null;
  spaceAfterTwips:  number | null;
  lineSpacingTwips: number | null;
  leftMarginTwips:  number | null;
  rightMarginTwips: number | null;
  firstIndentTwips: number | null;   // first-line indent (relative to left margin)
}

export interface CharLayout {
  bold:          boolean;
  italic:        boolean;
  underline:     boolean;
  strikethrough: boolean;
  fontHeight:    number | null;      // twips; null = inherit
  fontName:      string | null;
  color:         RGB | null;
}

export interface ResolvedStyle {
  name?:       string;
  paraLayout:  ParaLayout;
  charLayout:  CharLayout;
}

// Reference to an embedded object (picture, Sheet chart, ...) carried
// by an inline run of type 1.  `offset` points at the object's
// Embedded Object Section; width/height are the displayed size in
// twips (how big the object is drawn in the document).
export interface ObjectRef {
  offset:      number;
  widthTwips:  number;
  heightTwips: number;
}

export interface InlineRun {
  length: number;
  layout: CharLayout;
  object?: ObjectRef;
}

export interface ParaInfo {
  chars: number;
  styleId: number;            // resolved style ID (0=Normal, 0xFF=H1, 0xFE=H2, 0xFD=H3)
  paraLayout: ParaLayout;
  inlines: InlineRun[];       // chars in `inlines` should sum to `chars`
}

interface Cursor { pos: number; }

function parseLayoutSection(
  input: Uint8Array, dict: StreamEntry[], bodyChars: number,
  styles: Map<number, ResolvedStyle>,
): ParaInfo[] | null {
  const stream = findStream(dict, UID_LAYOUT_SECTION);
  if (!stream) return null;

  try {
    return parseLayoutSectionInner(input, stream.start, stream.end, bodyChars, styles);
  } catch {
    return null;
  }
}

function parseLayoutSectionInner(
  input: Uint8Array, start: number, end: number, bodyChars: number,
  styles: Map<number, ResolvedStyle>,
): ParaInfo[] | null {
  if (start + 2 > end) return null;
  const sectionFlag = input[start] | (input[start + 1] << 8);
  const withStyles = sectionFlag === 0x0001;
  const cur: Cursor = { pos: start + 2 };

  // Paragraph Type List (BListE).  Each type is a template that
  // references a base style; its own layout codes override the style.
  if (cur.pos >= end) return null;
  const typeCount = input[cur.pos++];
  interface TypeDef { paraLayout: ParaLayout; charLayout: CharLayout; styleId: number; }
  const types = new Map<number, TypeDef>();
  for (let i = 0; i < typeCount; i++) {
    const typeNum  = readU32(input, cur, end);
    // Base style is peeked at offset+len+4+temp per psiconv source:
    // read it before parsing the layout list so the layout codes can
    // inherit from the right style.
    let baseStyle = STYLE_NORMAL;
    if (withStyles) {
      const peekTemp = readUint32LE(input, cur.pos);
      if (cur.pos + 4 + peekTemp < end) baseStyle = input[cur.pos + 4 + peekTemp];
    }
    const baseStyleDef = styles.get(baseStyle) ?? styles.get(STYLE_NORMAL);
    const basePara = baseStyleDef ? baseStyleDef.paraLayout : defaultParaLayout();
    const baseChar = baseStyleDef ? baseStyleDef.charLayout : defaultCharLayout();
    const paraLay  = parseParagraphLayoutList(input, cur, end, basePara);
    if (withStyles) skipBytes(cur, 1, end);  // skip the base-style byte
    const charLay  = parseCharacterLayoutList(input, cur, end, baseChar);
    types.set(typeNum, { paraLayout: paraLay, charLayout: charLay, styleId: baseStyle });
  }

  // Paragraph Element List (LListE).
  const paraCount = readU32(input, cur, end);
  if (paraCount === 0 || paraCount > bodyChars) return null;

  const paragraphs: ParaInfo[] = [];
  const inlineCounts: number[] = [];
  const baseChars: CharLayout[] = [];  // per-paragraph base character layout for inline inheritance

  for (let i = 0; i < paraCount; i++) {
    const chars   = readU32(input, cur, end);
    const typeRef = readU8(input, cur, end);

    let paraLay: ParaLayout = defaultParaLayout();
    let baseChar: CharLayout = defaultCharLayout();
    let styleId = STYLE_NORMAL;
    let nInline = 1;

    if (typeRef === 0) {
      // Own layout: peek the base style first so the layout codes
      // inherit from the right style.
      let ownStyle = STYLE_NORMAL;
      if (withStyles) {
        const peekTemp = readUint32LE(input, cur.pos);
        if (cur.pos + 4 + peekTemp < end) ownStyle = input[cur.pos + 4 + peekTemp];
      }
      const styleDef = styles.get(ownStyle) ?? styles.get(STYLE_NORMAL);
      const basePara = styleDef ? styleDef.paraLayout : defaultParaLayout();
      baseChar       = styleDef ? styleDef.charLayout : defaultCharLayout();
      paraLay  = parseParagraphLayoutList(input, cur, end, basePara);
      styleId  = ownStyle;
      if (withStyles) skipBytes(cur, 1, end);
      nInline  = readU32(input, cur, end);
    } else {
      const t = types.get(typeRef);
      if (t) {
        paraLay  = t.paraLayout;
        styleId  = t.styleId;
        baseChar = t.charLayout;
      }
      nInline = 0;
    }

    paragraphs.push({ chars, styleId, paraLayout: paraLay, inlines: [] });
    inlineCounts.push(nInline);
    baseChars.push(baseChar);
  }

  // Inline List (LListE).  Total count is across all paragraphs.
  readU32(input, cur, end);  // not strictly needed for parsing

  for (let i = 0; i < paraCount; i++) {
    const para = paragraphs[i];
    const want = inlineCounts[i];
    const baseChar = baseChars[i];

    if (want === 0) {
      // Type-referenced paragraph: synthesize an inline run that
      // matches the paragraph's base character layout exactly, so the
      // delta-computing inline-open logic emits no overrides (the
      // paragraph-level \b / \fs / \f already convey the style).
      para.inlines.push({ length: para.chars, layout: baseChar });
      continue;
    }

    for (let j = 0; j < want; j++) {
      const inlineType = readU8(input, cur, end);
      const inlineLen  = readU32(input, cur, end);
      const layout     = parseCharacterLayoutList(input, cur, end, baseChar);
      let object: ObjectRef | undefined;
      if (inlineType === 1) {
        // Object run: ID (object marker, 0x10000051), L offset of the
        // Embedded Object Section, L displayed width, L displayed
        // height (twips).  Decoded from the ROM "Welcome to" files —
        // the displayed sizes match the picture aspect ratios exactly.
        readU32(input, cur, end);                      // object marker UID
        const offset      = readU32(input, cur, end);
        const widthTwips  = readU32(input, cur, end);
        const heightTwips = readU32(input, cur, end);
        object = { offset, widthTwips, heightTwips };
      }
      para.inlines.push({ length: inlineLen, layout, object });
    }
  }

  // Validate sums.
  let total = 0;
  for (const p of paragraphs) total += p.chars;
  if (total !== bodyChars) return null;
  return paragraphs;
}

// ----- Reader primitives -----

function readU8(input: Uint8Array, cur: Cursor, end: number): number {
  if (cur.pos >= end) throw new Error('readU8 OOB');
  return input[cur.pos++];
}

function readU32(input: Uint8Array, cur: Cursor, end: number): number {
  if (cur.pos + 4 > end) throw new Error('readU32 OOB');
  const v = readUint32LE(input, cur.pos);
  cur.pos += 4;
  return v;
}

// Two's-complement 32-bit read.  EPOC Length / Size / Indent values are
// stored as signed longs per psiconv parse_simple.c.
function readS32(input: Uint8Array, cur: Cursor, end: number): number {
  const v = readU32(input, cur, end);
  return v | 0;                                   // force signed-32 interpretation
}

function skipBytes(cur: Cursor, n: number, end: number): void {
  if (cur.pos + n > end) throw new Error('skip OOB');
  cur.pos += n;
}

// ----- Layout-list parsers -----

function parseParagraphLayoutList(
  input: Uint8Array, cur: Cursor, end: number,
  base: ParaLayout = defaultParaLayout(),
): ParaLayout {
  const layout = cloneParaLayout(base);
  const byteCount = readU32(input, cur, end);
  const stop = cur.pos + byteCount;
  if (stop > end) throw new Error('para layout OOB');
  while (cur.pos < stop) {
    const code = readU8(input, cur, end);
    switch (code) {
      case P_ALIGNMENT:        layout.alignment        = readU8(input, cur, end); break;
      // Length values are stored as two's-complement signed (psiconv
      // parse_simple.c reads with `(psiconv_s32) psiconv_read_u32`).
      // Most are non-negative in practice, but first-line indent
      // routinely goes negative for hanging-bullet paragraphs.
      case P_SPACE_BEFORE:     layout.spaceBeforeTwips = readS32(input, cur, end); break;
      case P_SPACE_AFTER:      layout.spaceAfterTwips  = readS32(input, cur, end); break;
      case P_LINE_SPACING:     layout.lineSpacingTwips = readS32(input, cur, end); break;
      case P_LINE_SPACING_EXACT: skipBytes(cur, 1, end); break;
      case P_LEFT_MARGIN:      layout.leftMarginTwips  = readS32(input, cur, end); break;
      case P_RIGHT_MARGIN:     layout.rightMarginTwips = readS32(input, cur, end); break;
      case P_FIRST_LINE_INDENT:layout.firstIndentTwips = readS32(input, cur, end); break;
      case P_KEEP_TOGETHER:
      case P_KEEP_WITH_NEXT:   skipBytes(cur, 1, end); break;
      case P_FILL_COLOR:       skipBytes(cur, 3, end); break;
      case P_BULLET: {
        // BListB: byte count, then count bytes of bullet description.
        const sub = readU8(input, cur, end);
        skipBytes(cur, sub, end);
        layout.hasBullet = true;
        break;
      }
      default: skipParagraphLayoutCode(input, cur, end, code);
    }
  }
  if (cur.pos !== stop) cur.pos = stop;
  return layout;
}

function skipParagraphLayoutCode(
  _input: Uint8Array, cur: Cursor, end: number, code: number,
): void {
  // Sizes from psiconv Layout_Codes.psi.  Codes we don't support but
  // know the size of are skipped; unknown codes are skipped as u8 to
  // limit blast radius (most cause an "abort and inherit" cleanly).
  switch (code) {
    case 0x01: skipBytes(cur, 3, end); break;            // FillColor: 3B
    case 0x02:                                            // LeftMargin: L
    case 0x03:                                            // RightMargin: L
    case 0x04:                                            // Indent: L
    case 0x07:                                            // LineSpacing: L
    case 0x09:                                            // SpaceBefore: L
    case 0x0A:                                            // SpaceAfter: L
    case 0x10:                                            // BorderDistance: L
    case 0x16:                                            // TabInterval: L
      skipBytes(cur, 4, end); break;
    case 0x06: case 0x08: case 0x0B: case 0x0C:
    case 0x0D: case 0x0E: case 0x0F:                      // various B flags
      skipBytes(cur, 1, end); break;
    case 0x11: case 0x12: case 0x13: case 0x14:           // Borders: 9B
      skipBytes(cur, 9, end); break;
    case 0x17: skipBytes(cur, 5, end); break;            // ExtraTab: 5B
    default: skipBytes(cur, 1, end); break;
  }
}

function parseCharacterLayoutList(
  input: Uint8Array, cur: Cursor, end: number,
  base: CharLayout = defaultCharLayout(),
): CharLayout {
  const layout = cloneCharLayout(base);
  const byteCount = readU32(input, cur, end);
  const stop = cur.pos + byteCount;
  if (stop > end) throw new Error('char layout OOB');
  while (cur.pos < stop) {
    const code = readU8(input, cur, end);
    switch (code) {
      case C_FONT_HEIGHT:    layout.fontHeight    = readU32(input, cur, end); break;
      case C_FONT_POSTURE:   layout.italic        = readU8(input, cur, end) !== 0; break;
      case C_FONT_WEIGHT:    layout.bold          = readU8(input, cur, end) !== 0; break;
      case C_FONT_UNDERLINE: layout.underline     = readU8(input, cur, end) !== 0; break;
      case C_FONT_STRIKE:    layout.strikethrough = readU8(input, cur, end) !== 0; break;
      case C_COLOR: {
        const r = readU8(input, cur, end);
        const g = readU8(input, cur, end);
        const b = readU8(input, cur, end);
        // Series 5 is greyscale-only — RGB triples are always r==g==b.
        // We carry the full RGB anyway so 5mx Color renders correctly.
        layout.color = { r, g, b };
        break;
      }
      case C_FONT_TYPEFACE: {
        // Length byte = name-length + 1 (the +1 is the trailing flag).
        // Name is ASCII; flag byte indicates screen font number.
        const totalLen = readU8(input, cur, end);
        if (totalLen >= 1) {
          const nameLen = totalLen - 1;
          if (cur.pos + nameLen + 1 > end) throw new Error('typeface OOB');
          const nameBytes = input.subarray(cur.pos, cur.pos + nameLen);
          layout.fontName = decodeAscii(nameBytes);
          cur.pos += nameLen + 1;  // name + flag byte
        }
        break;
      }
      default: skipCharacterLayoutCode(input, cur, end, code);
    }
  }
  if (cur.pos !== stop) cur.pos = stop;
  return layout;
}

function decodeAscii(b: Uint8Array): string {
  let s = '';
  for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]);
  return s;
}

function skipCharacterLayoutCode(_input: Uint8Array, cur: Cursor, end: number, code: number): void {
  switch (code) {
    case 0x18: case 0x1B: case 0x1F: case 0x23: case 0x24:
      skipBytes(cur, 1, end); break;
    case 0x19: case 0x1A:                                 // text color, hi-color: 3B
      skipBytes(cur, 3, end); break;
    default: skipBytes(cur, 1, end); break;
  }
}

// ----- Style + layout defaults -----

function defaultParaLayout(): ParaLayout {
  return {
    alignment: null,
    hasBullet: false,
    spaceBeforeTwips: null,
    spaceAfterTwips:  null,
    lineSpacingTwips: null,
    leftMarginTwips:  null,
    rightMarginTwips: null,
    firstIndentTwips: null,
  };
}

function defaultCharLayout(): CharLayout {
  return {
    bold:          false,
    italic:        false,
    underline:     false,
    strikethrough: false,
    fontHeight:    null,
    fontName:      null,
    color:         null,
  };
}

function cloneParaLayout(p: ParaLayout): ParaLayout { return { ...p }; }
function cloneCharLayout(c: CharLayout): CharLayout {
  return { ...c, color: c.color ? { ...c.color } : null };
}

// =============================================================================
// Embedded objects
// =============================================================================
//
// Each EPictureCharacter (0x0E) in the body has a matching inline run
// of type 1 in the layout section, pointing at an Embedded Object
// Section.  That section is a small BListE of (UID, offset) pairs:
//
//   0x10000146  Object Display Section  (B flag, L width, L height twips)
//   0x1000012A  Object Icon Section     (SListB app name, L w, L h)
//   0x10000144  Embedded File Section   (L length, then the payload)
//
// The payload is the embedding app's document body without a UID
// header.  For Sketch objects it contains a standard bitmap (header +
// RLE data) we can render to PNG.  For Sheet objects (charts/graphs)
// it contains the worksheet — there is no rasterised preview to
// extract, so we can't reproduce the chart itself; instead we pull the
// chart's cell data out and emit it as a small table, which preserves
// the information if not the picture.
//
// Decoded from the "Welcome to ..." documents in the EPOC device ROMs
// (Series 5/5mx/Revo/Osaris/Series 7/netBook/MC218), which between
// them embed both Sketch pictures and Sheet graphs.

const UID_OBJECT_ICON_SECTION = 0x1000012A;
const UID_OBJECT_FILE_SECTION = 0x10000144;

interface ImagePng { png: Uint8Array; widthPx: number; heightPx: number; }

interface ResolvedObject {
  image:       ImagePng | null;    // renderable picture, if any
  iconName:    string | null;      // embedding app, e.g. "Sketch", "Sheet"
  chartRows:   string[][] | null;  // cell data for non-picture objects
  widthTwips:  number;             // displayed size from the inline run
  heightTwips: number;
}

function resolveObject(input: Uint8Array, ref: ObjectRef): ResolvedObject {
  const out: ResolvedObject = {
    image: null, iconName: null, chartRows: null,
    widthTwips: ref.widthTwips, heightTwips: ref.heightTwips,
  };

  // Embedded Object Section: single-byte-cardinal count, then
  // (u32 UID, u32 offset) entries.  Offsets are absolute.
  const off = ref.offset;
  if (off < EPOC_HEADER_BYTES || off + 1 > input.length) return out;
  const countByte = input[off];
  if ((countByte & 1) !== 0) return out;
  const count = countByte >> 1;
  if (count === 0 || count > 8) return out;
  if (off + 1 + count * 8 > input.length) return out;

  let iconOff = -1, fileOff = -1;
  for (let i = 0; i < count; i++) {
    const uid    = readUint32LE(input, off + 1 + i * 8);
    const target = readUint32LE(input, off + 1 + i * 8 + 4);
    if (target < EPOC_HEADER_BYTES || target >= input.length) continue;
    if (uid === UID_OBJECT_ICON_SECTION) iconOff = target;
    if (uid === UID_OBJECT_FILE_SECTION) fileOff = target;
  }

  if (iconOff >= 0) {
    try {
      const cur: Cursor = { pos: iconOff };
      out.iconName = readSpecialString(input, cur, input.length);
    } catch { /* icon name is best-effort */ }
  }

  if (fileOff >= 0 && fileOff + 4 <= input.length) {
    const payloadLen   = readUint32LE(input, fileOff);
    const payloadStart = fileOff + 4;
    const payloadEnd   = Math.min(payloadStart + payloadLen, input.length);
    if (payloadLen > 0 && payloadStart < payloadEnd) {
      // Picture-bearing payload (Sketch, clip art): render the first
      // bitmap inside the payload extent.
      const bitmaps = findEmbeddedBitmaps(input, payloadStart, payloadEnd);
      if (bitmaps.length > 0) {
        const png = renderBitmapAt(input, bitmaps[0]);
        if (png) {
          const dv = new DataView(png.buffer, png.byteOffset, png.byteLength);
          out.image = {
            png,
            widthPx:  dv.getUint32(16, false),
            heightPx: dv.getUint32(20, false),
          };
        }
      }
      // Chart payload (Sheet): no bitmap to render, but the cell data
      // is in there as a standard Sheet Cell List.
      if (!out.image) {
        const cells = scanForCellList(input, payloadStart, payloadEnd);
        if (cells) out.chartRows = cellsToRows(cells);
      }
    }
  }
  return out;
}

function cellsToRows(cells: Cell[]): string[][] {
  const byRow = new Map<number, Map<number, string>>();
  let maxCol = 0;
  for (const c of cells) {
    let r = byRow.get(c.row);
    if (!r) { r = new Map(); byRow.set(c.row, r); }
    r.set(c.col, c.text);
    if (c.col > maxCol) maxCol = c.col;
  }
  const rows: string[][] = [];
  for (const rowNum of Array.from(byRow.keys()).sort((a, b) => a - b)) {
    const r = byRow.get(rowNum)!;
    const cols: string[] = [];
    for (let c = 0; c <= maxCol; c++) cols.push(r.get(c) ?? '');
    rows.push(cols);
  }
  return rows;
}

// Map each body position covered by an object run to its resolved
// object, so buildRtf can emit the right thing at each
// EPictureCharacter.  Returns null when the layout carried no objects
// (the caller then falls back to a whole-file bitmap scan).
function resolveObjects(
  input: Uint8Array, paragraphs: ParaInfo[], bodyLen: number,
): (ResolvedObject | null)[] | null {
  const objectAt: (ResolvedObject | null)[] = new Array(bodyLen).fill(null);
  let any = false;
  let pos = 0;
  for (const p of paragraphs) {
    let runStart = pos;
    for (const r of p.inlines) {
      if (r.object) {
        any = true;
        const resolved = resolveObject(input, r.object);
        for (let i = 0; i < r.length && runStart + i < bodyLen; i++) {
          objectAt[runStart + i] = resolved;
        }
      }
      runStart += r.length;
    }
    pos += p.chars;
  }
  return any ? objectAt : null;
}

// Whole-file bitmap scan — fallback for documents whose layout section
// is missing or unparseable.  Images are then spliced at picture
// characters in file order, which is right whenever every embedded
// object is a picture.
function collectImages(input: Uint8Array): ImagePng[] {
  const out: ImagePng[] = [];
  for (const offset of findEmbeddedBitmaps(input)) {
    const png = renderBitmapAt(input, offset);
    if (!png) continue;
    const dv = new DataView(png.buffer, png.byteOffset, png.byteLength);
    out.push({ png, widthPx: dv.getUint32(16, false), heightPx: dv.getUint32(20, false) });
  }
  return out;
}

// =============================================================================
// RTF generation
// =============================================================================

function buildRtf(
  body: Uint8Array,
  images: ImagePng[],
  objectAt: (ResolvedObject | null)[] | null,
  paragraphs: ParaInfo[] | null,
  styles: Map<number, ResolvedStyle>,
): Uint8Array {
  // Build font + color tables from everything referenced anywhere in
  // the doc (styles + inline runs).  We need them up-front because
  // RTF wants the tables declared in the document header.
  const fonts  = collectFonts(paragraphs, styles);
  const colors = collectColors(paragraphs, styles);

  let rtf = '{\\rtf1\\ansi\\ansicpg1252\\deff0';
  rtf += '{\\fonttbl';
  for (let i = 0; i < fonts.length; i++) rtf += `{\\f${i} ${fonts[i].rtfFamily} ${escapeForTable(fonts[i].name)};}`;
  rtf += '}';
  if (colors.length > 0) {
    rtf += '{\\colortbl;';                          // index 0 = auto/none
    for (const c of colors) rtf += `\\red${c.r}\\green${c.g}\\blue${c.b};`;
    rtf += '}';
  }
  rtf += '\\fs24 ';

  const idx = paragraphs ? buildIndex(paragraphs, body.length) : null;

  // Currently-open inline formatting group + which paragraph signature
  // we last opened paragraph properties for.
  let curInline: CharLayout | null = null;
  let lastParaSig = '';
  let imageIndex = 0;

  // We need to know the paragraph-level "base" formatting at every
  // point so we only emit inline overrides where they DIFFER.  For
  // headings the paragraph-level \fs28\b conveys it; we shouldn't then
  // wrap each char in `{\b \fs28 ...}` too.
  const openParagraph = (p: ParaInfo): { text: string; base: CharLayout } => {
    // \pard resets paragraph properties but NOT character state — a
    // heading's \b\fs48 would otherwise bleed into every following
    // paragraph.  \plain resets character state to document defaults
    // before we apply this paragraph's own base formatting.
    const props: string[] = ['\\pard\\plain'];
    const align = p.paraLayout.alignment;
    if      (align === 1) props.push('\\qc');
    else if (align === 2) props.push('\\qr');
    else if (align === 3) props.push('\\qj');
    else                  props.push('\\ql');
    if (p.paraLayout.leftMarginTwips  !== null) props.push(`\\li${p.paraLayout.leftMarginTwips}`);
    if (p.paraLayout.rightMarginTwips !== null) props.push(`\\ri${p.paraLayout.rightMarginTwips}`);
    if (p.paraLayout.firstIndentTwips !== null) props.push(`\\fi${p.paraLayout.firstIndentTwips}`);
    if (p.paraLayout.spaceBeforeTwips !== null) props.push(`\\sb${p.paraLayout.spaceBeforeTwips}`);
    if (p.paraLayout.spaceAfterTwips  !== null) props.push(`\\sa${p.paraLayout.spaceAfterTwips}`);
    if (p.paraLayout.lineSpacingTwips !== null) props.push(`\\sl${p.paraLayout.lineSpacingTwips}`);
    if (p.paraLayout.hasBullet)                 props.push('\\bullet ');

    // Resolve the paragraph's base character formatting (from the named
    // style) and emit those character properties at paragraph level too
    // — RTF readers paint them throughout the paragraph unless a more
    // specific inline group overrides.
    const style = styles.get(p.styleId) ?? styles.get(STYLE_NORMAL);
    const base = style ? style.charLayout : defaultCharLayout();
    if (base.bold)          props.push('\\b');
    if (base.italic)        props.push('\\i');
    if (base.underline)     props.push('\\ul');
    if (base.strikethrough) props.push('\\strike');
    if (base.fontHeight !== null) props.push(`\\fs${twipsToHalfPoints(base.fontHeight)}`);
    if (base.fontName) {
      const idx = fonts.findIndex(f => f.name === base.fontName);
      if (idx >= 0) props.push(`\\f${idx}`);
    }
    if (base.color) {
      const idx = colorIndex(colors, base.color);
      if (idx > 0) props.push(`\\cf${idx}`);
    }
    return { text: props.join(''), base };
  };

  // Emit an inline-formatting group that opens only those attributes
  // that DIFFER from the paragraph's base.  Returns the open string and
  // whether anything actually differs (so we know whether to close
  // the group later).
  const openInline = (c: CharLayout, base: CharLayout): { text: string; formatted: boolean } => {
    const parts: string[] = [];
    if (c.bold          !== base.bold)          parts.push(c.bold          ? '\\b'      : '\\b0');
    if (c.italic        !== base.italic)        parts.push(c.italic        ? '\\i'      : '\\i0');
    if (c.underline     !== base.underline)     parts.push(c.underline     ? '\\ul'     : '\\ul0');
    if (c.strikethrough !== base.strikethrough) parts.push(c.strikethrough ? '\\strike' : '\\strike0');
    if (c.fontHeight !== null && c.fontHeight !== base.fontHeight) {
      parts.push(`\\fs${twipsToHalfPoints(c.fontHeight)}`);
    }
    if (c.fontName && c.fontName !== base.fontName) {
      const i = fonts.findIndex(f => f.name === c.fontName);
      if (i >= 0) parts.push(`\\f${i}`);
    }
    if (c.color && !sameColor(c.color, base.color)) {
      const i = colorIndex(colors, c.color);
      if (i > 0) parts.push(`\\cf${i}`);
    }
    if (parts.length === 0) return { text: '', formatted: false };
    return { text: '{' + parts.join('') + ' ', formatted: true };
  };

  // Active paragraph's base character layout — used by openInline to
  // compute deltas — and whether we have an inline group currently open.
  let curBase: CharLayout = defaultCharLayout();
  let inlineGroupOpen = false;

  const closeInlineIfOpen = () => {
    if (inlineGroupOpen) {
      rtf += '}';
      inlineGroupOpen = false;
    }
  };

  if (idx && idx.paraAt[0]) {
    const op = openParagraph(idx.paraAt[0]);
    rtf += op.text + ' ';
    curBase = op.base;
    lastParaSig = paragraphSig(idx.paraAt[0]);
  }

  for (let i = 0; i < body.length; i++) {
    if (idx) {
      const p = idx.paraAt[i];
      const sig = p ? paragraphSig(p) : '';
      if (p && sig !== lastParaSig) {
        closeInlineIfOpen();
        curInline = null;
        const op = openParagraph(p);
        rtf += op.text + ' ';
        curBase = op.base;
        lastParaSig = sig;
      }
      const next = idx.runAt[i];
      if (next !== curInline) {
        closeInlineIfOpen();
        curInline = next;
        if (curInline) {
          const oi = openInline(curInline, curBase);
          if (oi.formatted) {
            rtf += oi.text;
            inlineGroupOpen = true;
          }
        }
      }
    }

    const b = body[i];
    switch (b) {
      case EParagraphDelimiter:
        closeInlineIfOpen();
        curInline = null;
        rtf += '\\par\n';
        lastParaSig = '';
        break;
      case ELineBreak:        rtf += '\\line\n'; break;
      case EPageBreak:        rtf += '\\page\n'; break;
      case ETabCharacter:
      case ENonBreakingTab:   rtf += '\\tab ';   break;
      case ENonBreakingHyphen:
      case EPotentialHyphen:  rtf += '-';        break;
      case ENonBreakingSpace: rtf += '\\~';      break;
      case EVisibleSpace:     rtf += ' ';        break;
      case EPictureCharacter: {
        const obj = objectAt ? objectAt[i] : null;
        if (obj) rtf += emitObject(obj);
        else if (imageIndex < images.length) rtf += emitPicture(images[imageIndex++]);
        break;
      }
      default: rtf += escapeChar(b);
    }
  }
  closeInlineIfOpen();
  rtf += '}\n';
  return new TextEncoder().encode(rtf);
}

function twipsToHalfPoints(twips: number): number {
  // RTF \fsN = half-points; EPOC stores twips; 1 pt = 20 twips so
  // half-points = twips / 10.
  return Math.max(2, Math.round(twips / 10));
}

function sameColor(a: RGB | null, b: RGB | null): boolean {
  if (a === b) return true;
  if (!a || !b) return false;
  return a.r === b.r && a.g === b.g && a.b === b.b;
}

interface FontEntry { name: string; rtfFamily: string; }

function collectFonts(
  paragraphs: ParaInfo[] | null,
  styles: Map<number, ResolvedStyle>,
): FontEntry[] {
  const seen = new Set<string>();
  const out: FontEntry[] = [];
  const add = (name: string | null | undefined) => {
    if (!name) return;
    if (seen.has(name)) return;
    seen.add(name);
    out.push({ name, rtfFamily: fontFamilyOf(name) });
  };
  // Always include Times New Roman as \f0 (the default).
  add('Times New Roman');
  // Then any styled or per-run fonts.
  for (const s of styles.values()) add(s.charLayout.fontName);
  if (paragraphs) {
    for (const p of paragraphs) for (const r of p.inlines) add(r.layout.fontName);
  }
  return out;
}

function fontFamilyOf(name: string): string {
  const lower = name.toLowerCase();
  if (lower.includes('times') || lower.includes('serif')) return '\\froman';
  if (lower.includes('courier') || lower.includes('mono')) return '\\fmodern';
  if (lower.includes('arial') || lower.includes('helvet') || lower.includes('swiss')
   || lower.includes('sans')) return '\\fswiss';
  return '\\fnil';
}

function collectColors(
  paragraphs: ParaInfo[] | null,
  styles: Map<number, ResolvedStyle>,
): RGB[] {
  const seen = new Map<string, number>();
  const out: RGB[] = [];
  const add = (c: RGB | null | undefined) => {
    if (!c) return;
    // Skip pure black — RTF's default is auto/black so saying \cf
    // for black is wasteful.
    if (c.r === 0 && c.g === 0 && c.b === 0) return;
    const key = `${c.r},${c.g},${c.b}`;
    if (seen.has(key)) return;
    seen.set(key, out.length);
    out.push({ ...c });
  };
  for (const s of styles.values()) add(s.charLayout.color);
  if (paragraphs) {
    for (const p of paragraphs) for (const r of p.inlines) add(r.layout.color);
  }
  return out;
}

function colorIndex(colors: RGB[], c: RGB): number {
  if (c.r === 0 && c.g === 0 && c.b === 0) return 0;          // auto/black
  for (let i = 0; i < colors.length; i++) {
    if (sameColor(colors[i], c)) return i + 1;                // 1-based (0 = auto)
  }
  return 0;
}

function escapeForTable(s: string): string {
  return s.replace(/[\\{}]/g, m => '\\' + m);
}

interface Index {
  paraAt: (ParaInfo | null)[];
  runAt:  (CharLayout | null)[];
}

function buildIndex(paragraphs: ParaInfo[], bodyLen: number): Index {
  const paraAt: (ParaInfo | null)[] = new Array(bodyLen).fill(null);
  const runAt:  (CharLayout | null)[] = new Array(bodyLen).fill(null);
  let pos = 0;
  for (const p of paragraphs) {
    for (let i = 0; i < p.chars && pos < bodyLen; i++) paraAt[pos + i] = p;
    let inlinePos = pos;
    for (const r of p.inlines) {
      for (let i = 0; i < r.length && inlinePos < bodyLen; i++) {
        runAt[inlinePos++] = r.layout;
      }
    }
    pos += p.chars;
  }
  return { paraAt, runAt };
}

// Cheap signature so we only re-emit paragraph properties when they
// actually change between paragraphs.
function paragraphSig(p: ParaInfo): string {
  return `${p.styleId}:${p.paraLayout.alignment}:${p.paraLayout.hasBullet}`;
}

function escapeChar(b: number): string {
  if (b === 0x5C) return '\\\\';
  if (b === 0x7B) return '\\{';
  if (b === 0x7D) return '\\}';
  if (b >= 0x20 && b <= 0x7E) return String.fromCharCode(b);
  if (b >= 0xA0 && b <= 0xFF) return "\\'" + b.toString(16).padStart(2, '0');
  return '';
}

function emitObject(obj: ResolvedObject): string {
  if (obj.image) {
    return emitPicture(obj.image, obj.widthTwips, obj.heightTwips);
  }
  // Non-picture object (a Sheet chart in every real document we've
  // seen).  We can't reproduce the drawing, but if we got the cell
  // data out we can at least show what the chart plotted.
  const name = obj.iconName ? escapeText(obj.iconName) : 'embedded';
  if (obj.chartRows) {
    let s = `{\\i [${name} chart \\endash  data:]}{\\fs18 \\line `;
    for (const row of obj.chartRows) {
      s += row.map(escapeText).join('\\tab ') + '\\line ';
    }
    s += '}';
    return s;
  }
  return `{\\i [${name} object]}`;
}

function escapeText(t: string): string {
  let s = '';
  for (let i = 0; i < t.length; i++) s += escapeChar(t.charCodeAt(i));
  return s;
}

function emitPicture(img: ImagePng, goalWTwips = 0, goalHTwips = 0): string {
  // Displayed size: use the document's own object-display size when we
  // have it (the inline-run width/height); otherwise assume 96 dpi
  // (15 twips per pixel).
  const wTwips = goalWTwips > 0 ? goalWTwips : img.widthPx  * 15;
  const hTwips = goalHTwips > 0 ? goalHTwips : img.heightPx * 15;
  let s = `{\\pict\\pngblip\\picw${img.widthPx}\\pich${img.heightPx}`;
  s += `\\picwgoal${wTwips}\\pichgoal${hTwips}\n`;
  s += bytesToHex(img.png);
  s += '}';
  return s;
}

function bytesToHex(b: Uint8Array): string {
  const chunks: string[] = [];
  let line = '';
  for (let i = 0; i < b.length; i++) {
    line += (b[i] >>> 4).toString(16) + (b[i] & 0xF).toString(16);
    if (line.length >= 64) { chunks.push(line); line = ''; }
  }
  if (line) chunks.push(line);
  return chunks.join('\n');
}
