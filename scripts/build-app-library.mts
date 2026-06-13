// Builds the web app-library from the 3-Lib software collection in
// applib/ (the catalogue categories + HTML index pages preserved from
// Steve Litchfield's 3-Lib CD-ROM — see applib/README.md). For every
// catalogued app this emits:
//
//   <out>/manifest.json                  one entry per app (see AppEntry)
//   <out>/files/<category>/<slug>.zip    the app's files, zipped (download
//                                        artifact AND the source the
//                                        "Try it" flow unpacks in-browser)
//   <out>/icons/<category>/<slug>.png    real app icon where one could be
//                                        extracted from an EPOC .AIF
//
// The catalogue source of truth is the <PRE> block in each category's
// .htm page (lines of "NAME  DD/MM/YY  description"), cross-referenced
// against the category's subdirectories. Categories without a <PRE>
// index (PC tools, PsiWin, the DOS emulator) are skipped — see
// CATEGORIES below for the include list and per-category device map.
//
// Run (node 22+):
//   node --experimental-strip-types scripts/build-app-library.mts            # → frontend/public/apps
//   node --experimental-strip-types scripts/build-app-library.mts --out dist/apps
//   node --experimental-strip-types scripts/build-app-library.mts --category epocgames --limit 5
//
// CI runs this after the Vite build, writing straight into dist/apps
// (the output is generated, never committed — like dist/roms).

import * as fs from 'node:fs';
import * as path from 'node:path';
import { deflateRawSync } from 'node:zlib';
import { findEmbeddedBitmaps, renderBitmapAt } from '../frontend/src/lib/converters/sketch.ts';
import { encodePng } from '../frontend/src/lib/converters/png-encoder.ts';

const REPO = path.resolve(new URL('..', import.meta.url).pathname);
const LIB = path.join(REPO, 'applib');

// ── Category map ────────────────────────────────────────────────────
// platform drives the delivery mechanism; devices lists the emulated
// device ids the apps are expected to run on; tryDevice is the default
// "Try it" target (must support automated delivery: CF image, SSD pack
// or Remote Link).
interface CategoryMeta {
  id: string;
  htm: string;
  label: string;
  platform: 'sibo' | 'epoc32' | 'other';
  devices: string[];
  tryDevice: string | null;
  vault: boolean;
}

const SIBO_DEVICES = ['series3a', 'series3c', 'series3mx', 'pocketbk2', 'workabout'];
// 5mx Pro is listed but the library UI only enables it once the user
// has booted it before (it needs its OS card flow on first load); see
// BOOTLOADER_TRY_DEVICES in frontend/src/lib/appLibrary.ts. Same deal
// for the netBook in the s7games row.
const EPOC_DEVICES = ['series5', '5mx', '5mxpro', 'mc218', 'revo'];

const CATEGORIES: CategoryMeta[] = [
  { id: 's3games',    htm: 's3games.htm',    label: 'Series 3 — Games',          platform: 'sibo',   devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3util',     htm: 's3util.htm',     label: 'Series 3 — Utilities',      platform: 'sibo',   devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3units',    htm: 's3units.htm',    label: 'Series 3 — Conversions & time', platform: 'sibo', devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3money',    htm: 's3money.htm',    label: 'Series 3 — Money & finance', platform: 'sibo',  devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3misc',     htm: 's3misc.htm',     label: 'Series 3 — Miscellaneous',  platform: 'sibo',   devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3graphics', htm: 's3graphics.htm', label: 'Series 3 — Graphics & data', platform: 'sibo',  devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3prog',     htm: 's3prog.htm',     label: 'Series 3 — Programming',    platform: 'sibo',   devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3comms',    htm: 's3comms.htm',    label: 'Series 3 — Comms',          platform: 'sibo',   devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3mapping',  htm: 's3mapping.htm',  label: 'Series 3 — Mapping',        platform: 'sibo',   devices: SIBO_DEVICES, tryDevice: 'series3c', vault: false },
  { id: 's3vault',    htm: 's3vault.htm',    label: 'Series 3 — Vault',          platform: 'sibo',   devices: SIBO_DEVICES, tryDevice: 'series3c', vault: true  },
  { id: 'siena',      htm: 'siena.htm',      label: 'Siena',                     platform: 'sibo',   devices: ['siena'],    tryDevice: 'siena',    vault: false },
  { id: 'epocgames',  htm: 'epocgames.htm',  label: 'EPOC — Games',              platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'epocutil',   htm: 'epocutil.htm',   label: 'EPOC — Utilities',          platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'epocmisc',   htm: 'epocmisc.htm',   label: 'EPOC — Miscellaneous',      platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'epocmoney',  htm: 'epocmoney.htm',  label: 'EPOC — Money & finance',    platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'epocgraphics', htm: 'epocgraphics.htm', label: 'EPOC — Graphics',       platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'epocprog',   htm: 'epocprog.htm',   label: 'EPOC — Programming',        platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'epocmap',    htm: 'epocmap.htm',    label: 'EPOC — Mapping',            platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'epocvault',  htm: 'epocvault.htm',  label: 'EPOC — Vault',              platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: true  },
  { id: 'msgsuite',   htm: 'msgsuite.htm',   label: 'EPOC — Message Suite',      platform: 'epoc32', devices: EPOC_DEVICES, tryDevice: '5mx', vault: false },
  { id: 'revogames',  htm: 'revogames.htm',  label: 'Revo — Games',              platform: 'epoc32', devices: ['revo'],     tryDevice: 'revo', vault: false },
  { id: 's7games',    htm: 's7games.htm',    label: 'Series 7 — Games',          platform: 'epoc32', devices: ['series7', 'netbook'], tryDevice: 'series7', vault: false },
  // GeoFox One isn't an emulated device — keep the apps browsable and
  // downloadable, but with no "Try it" target.
  { id: 'geofox',     htm: 'geofox.htm',     label: 'GeoFox One',                platform: 'epoc32', devices: [],          tryDevice: null, vault: false },
];

// SSD flash packs top out at 8 MB and FEFS adds per-record overhead;
// SIBO apps bigger than this can't be delivered automatically.
const SIBO_TRY_LIMIT = 7 * 1024 * 1024;

// ── Manifest types (mirrored in frontend/src/lib/appLibrary.ts) ─────
interface AppEntry {
  id: string;
  name: string;
  category: string;
  section: string | null;
  date: string | null;          // ISO yyyy-mm-dd
  description: string;
  sizeBytes: number;
  fileCount: number;
  zip: string;                  // path under <out>
  icon: string | null;          // path under <out>
  devices: string[];
  tryDevice: string | null;
  installKind: 'sis' | 'sibo' | 'none';
  installFile: string | null;   // path within the zip
  readmes: string[];            // readme-like docs, paths within the zip
  vault: boolean;
}

// ── Small helpers ───────────────────────────────────────────────────

function decodeEntities(s: string): string {
  return s
    .replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&nbsp;/g, ' ');
}

function slugify(name: string): string {
  const s = name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
  return s || 'app';
}

function isoDate(ddmmyy: string): string | null {
  const m = /^(\d{2})\/(\d{2})\/(\d{2})$/.exec(ddmmyy);
  if (!m) return null;
  const yy = Number(m[3]);
  // The library spans 1991–2006: pivot two-digit years at 90.
  const year = yy >= 90 ? 1900 + yy : 2000 + yy;
  return `${year}-${m[2]}-${m[1]}`;
}

// ── Catalogue parsing ───────────────────────────────────────────────

interface CatalogueEntry { name: string; date: string | null; description: string; section: string | null }

// The category pages use three line dialects:
//   most categories: "NAME  DD/MM/YY  description"   (date-anchored)
//   s3prog / epocvault: "NAME - description"
//   s3vault: "NAME  description"                      (two-space, no date)
// The undated forms are ambiguous against prose/headers, so those only
// count as entries when NAME matches an actual app directory.
function parseCatalogue(htmPath: string, dirNames: Set<string>): CatalogueEntry[] {
  const html = fs.readFileSync(htmPath, 'latin1');
  const m = /<PRE>([\s\S]*?)<\/PRE>/i.exec(html);
  if (!m) return [];
  const out: CatalogueEntry[] = [];
  let section: string | null = null;
  // The name column can contain single spaces (e.g. "P-Plus Bridge"),
  // so the dated form anchors on the date.
  const datedRe = /^(\S.*?)\s\s+(\d{2}\/\d{2}\/\d{2})\s+(.*)$/;
  const dashRe  = /^(\S+)\s+-\s+(.*)$/;
  const plainRe = /^(\S(?:.*?\S)?)\s\s+(.*)$/;
  for (const rawLine of m[1].split('\n')) {
    const line = decodeEntities(rawLine.replace(/<[^>]+>/g, '')).trimEnd();
    if (line.trim() === '') { continue; }
    const dm = datedRe.exec(line);
    if (dm) {
      out.push({ name: dm[1].trim(), date: dm[2], description: dm[3].trim(), section });
      continue;
    }
    const um = dashRe.exec(line) ?? plainRe.exec(line);
    if (um && dirNames.has(um[1].trim().toLowerCase())) {
      out.push({ name: um[1].trim(), date: null, description: um[2].trim(), section });
      continue;
    }
    if (/^\s/.test(rawLine) && out.length > 0) {
      // Indented continuation of the previous description.
      out[out.length - 1].description += ' ' + line.trim();
    } else {
      // Unindented line that isn't an entry: a section header.
      section = line.trim() || section;
    }
  }
  return out;
}

// ── ZIP writer (stored/deflate, no zip64) ───────────────────────────

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c >>> 0;
  }
  return t;
})();

function crc32(data: Uint8Array): number {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < data.length; i++) c = CRC_TABLE[(c ^ data[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

interface ZipEntryIn { name: string; data: Buffer }

function buildZip(entries: ZipEntryIn[]): Buffer {
  const chunks: Buffer[] = [];
  const central: Buffer[] = [];
  let offset = 0;
  // Fixed timestamp (the CD's vintage) so rebuilds are deterministic.
  const dosTime = 0;             // 00:00:00
  const dosDate = ((2000 - 1980) << 9) | (1 << 5) | 1;  // 2000-01-01

  for (const e of entries) {
    const nameBytes = Buffer.from(e.name, 'latin1');
    const crc = crc32(e.data);
    const deflated = deflateRawSync(e.data, { level: 9 });
    const useDeflate = deflated.length < e.data.length;
    const payload = useDeflate ? deflated : e.data;
    const method = useDeflate ? 8 : 0;

    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);            // version needed
    local.writeUInt16LE(0, 6);             // flags
    local.writeUInt16LE(method, 8);
    local.writeUInt16LE(dosTime, 10);
    local.writeUInt16LE(dosDate, 12);
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(payload.length, 18);
    local.writeUInt32LE(e.data.length, 22);
    local.writeUInt16LE(nameBytes.length, 26);
    local.writeUInt16LE(0, 28);            // extra len

    const cen = Buffer.alloc(46);
    cen.writeUInt32LE(0x02014b50, 0);
    cen.writeUInt16LE(20, 4);              // version made by
    cen.writeUInt16LE(20, 6);              // version needed
    cen.writeUInt16LE(0, 8);
    cen.writeUInt16LE(method, 10);
    cen.writeUInt16LE(dosTime, 12);
    cen.writeUInt16LE(dosDate, 14);
    cen.writeUInt32LE(crc, 16);
    cen.writeUInt32LE(payload.length, 20);
    cen.writeUInt32LE(e.data.length, 24);
    cen.writeUInt16LE(nameBytes.length, 28);
    // extra/comment/disk/attrs all zero
    cen.writeUInt32LE(offset, 42);

    chunks.push(local, nameBytes, payload);
    central.push(cen, nameBytes);
    offset += local.length + nameBytes.length + payload.length;
  }

  const centralStart = offset;
  let centralSize = 0;
  for (const c of central) centralSize += c.length;
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(entries.length, 8);
  eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(centralSize, 12);
  eocd.writeUInt32LE(centralStart, 16);

  return Buffer.concat([...chunks, ...central, eocd]);
}

// ── App-dir scanning ────────────────────────────────────────────────

interface AppFile { rel: string; abs: string; size: number }

function walkDir(root: string): AppFile[] {
  const out: AppFile[] = [];
  const visit = (dir: string, prefix: string) => {
    for (const name of fs.readdirSync(dir).sort()) {
      const abs = path.join(dir, name);
      const st = fs.statSync(abs);
      if (st.isDirectory()) visit(abs, `${prefix}${name}/`);
      else out.push({ rel: `${prefix}${name}`, abs, size: st.size });
    }
  };
  visit(root, '');
  return out;
}

// Readme-like docs surfaced as tabs in the details popup. Only plain-text
// formats the browser can render in a <pre> qualify — Word/RTF docs
// (.doc, .wrd, .word, .rtf) are binary on these platforms and would show
// as mojibake. Most app docs are .txt, frequently named after the app
// (BLADES.TXT, PACMAN.TXT) rather than "readme", so the whole text-doc
// set is included, ranked so genuine readmes lead.
const README_TEXT_EXT = /\.(txt|me|1st|2nd|asc|nfo|new)$/i;

function isReadmeLike(rel: string): boolean {
  const base = rel.split('/').pop() ?? rel;
  if (README_TEXT_EXT.test(base)) return true;
  // Extension-less READMEs (README, ReadMe, READ.ME → "read" + .me above).
  return !base.includes('.') && /^read.?me$|^read.?1st$|^lisez|^liesmich/i.test(base);
}

function readmeRank(rel: string): number {
  const base = (rel.split('/').pop() ?? rel).toLowerCase();
  if (/read.?me|read.?1st/.test(base))                  return 0;
  if (/manual|instruct|gameinfo|getstart|^docs?\b/.test(base)) return 1;
  if (/changes|history|whatsnew|^new\b|release/.test(base))    return 2;
  if (/register|licen[cs]e|order|payment/.test(base))   return 4; // least useful
  return 3;
}

// Rank readme-like docs (readmes first, then guides, then everything
// else), shallowest path winning ties; cap the list so the tab bar stays
// sane. Empty and oversized files are skipped — a readme isn't 1 MB.
function findReadmes(files: AppFile[]): string[] {
  return files
    .filter(f => isReadmeLike(f.rel) && f.size > 0 && f.size <= 512 * 1024)
    .sort((a, b) =>
      readmeRank(a.rel) - readmeRank(b.rel) ||
      (a.rel.split('/').length - b.rel.split('/').length) ||
      a.rel.localeCompare(b.rel))
    .slice(0, 12)
    .map(f => f.rel);
}

function pickInstaller(
  files: AppFile[], platform: CategoryMeta['platform'], dirName: string,
): { kind: 'sis' | 'sibo' | 'none'; file: string | null } {
  const lcDir = dirName.toLowerCase().replace(/[^a-z0-9]/g, '');
  if (platform === 'epoc32') {
    const sis = files.filter(f => f.rel.toLowerCase().endsWith('.sis'));
    if (sis.length === 0) return { kind: 'none', file: null };
    // Prefer shallow paths, then a name resembling the app dir, then size
    // (the biggest SIS is usually the app; smaller ones are add-ons).
    sis.sort((a, b) =>
      (a.rel.split('/').length - b.rel.split('/').length) ||
      (Number(b.rel.toLowerCase().includes(lcDir.slice(0, 6))) -
       Number(a.rel.toLowerCase().includes(lcDir.slice(0, 6)))) ||
      (b.size - a.size));
    return { kind: 'sis', file: sis[0].rel };
  }
  if (platform === 'sibo') {
    const apps = files.filter(f => /\.(opa|app)$/i.test(f.rel));
    if (apps.length === 0) return { kind: 'none', file: null };
    apps.sort((a, b) =>
      (a.rel.split('/').length - b.rel.split('/').length) || (b.size - a.size));
    return { kind: 'sibo', file: apps[0].rel };
  }
  return { kind: 'none', file: null };
}

// EPOC AIF resource signature: UID1 0x10000037 (direct file store) +
// UID2 0x1000006A (application info file). Distinguishes the icon AIF
// from the other direct-file-store payloads (MBM, HLP) embedded in a
// SIS archive.
const AIF_SIG = [0x37, 0x00, 0x00, 0x10, 0x6a, 0x00, 0x00, 0x10];

function findAifOffsets(bytes: Uint8Array): number[] {
  const out: number[] = [];
  outer:
  for (let i = 0; i + AIF_SIG.length <= bytes.length; i++) {
    for (let k = 0; k < AIF_SIG.length; k++) {
      if (bytes[i + k] !== AIF_SIG[k]) continue outer;
    }
    out.push(i);
  }
  return out;
}

function iconFromAifAt(bytes: Uint8Array, offset: number, end: number): Uint8Array | null {
  for (const off of findEmbeddedBitmaps(bytes, offset + 16, end)) {
    const png = renderBitmapAt(bytes, off);
    if (png && png.length > 100) return png;
  }
  return null;
}

// ── SIBO icons ──────────────────────────────────────────────────────
// OPL apps embed their icon as the first PIC resource in the .OPA/.APP
// binary ("PIC\xDC" magic). Layout (reverse-engineered against the
// library and verified visually): 8-byte header (magic, version "00",
// u16 bitmap count) then 12-byte descriptors — u16 crc, u16 width,
// u16 height, u16 byteSize, u32 data offset counted from the END of
// that descriptor. Bitmaps are 1bpp, rows byte-padded, LSB = leftmost
// pixel. Icons are two same-sized planes: black + grey.

const PIC_MAGIC = [0x50, 0x49, 0x43, 0xdc];

function decodeSiboPicIcon(bytes: Uint8Array): Uint8Array | null {
  let off = -1;
  outer:
  for (let i = 0; i + 8 <= bytes.length; i++) {
    for (let k = 0; k < 4; k++) if (bytes[i + k] !== PIC_MAGIC[k]) continue outer;
    off = i; break;
  }
  if (off < 0) return null;
  const u16 = (p: number) => bytes[p] | (bytes[p + 1] << 8);
  const u32 = (p: number) =>
    ((bytes[p] | (bytes[p+1] << 8) | (bytes[p+2] << 16) | (bytes[p+3] << 24)) >>> 0);
  const count = u16(off + 6);
  if (count === 0 || count > 256) return null;
  interface Desc { w: number; h: number; size: number; data: number }
  const descs: Desc[] = [];
  for (let i = 0; i < count; i++) {
    const d = off + 8 + i * 12;
    if (d + 12 > bytes.length) break;
    const desc = { w: u16(d + 2), h: u16(d + 4), size: u16(d + 6), data: d + 12 + u32(d + 8) };
    if (desc.data + desc.size > bytes.length) continue;
    if (desc.size < Math.ceil(desc.w / 8) * desc.h) continue;
    descs.push(desc);
  }
  // The icon is the first plausibly icon-sized bitmap (S3 icons are
  // 24×24, 3a-era 48×48); a same-sized neighbour is its grey plane.
  const icon = descs.find(d => d.w >= 16 && d.w <= 64 && d.h >= 16 && d.h <= 64);
  if (!icon) return null;
  const grey = descs.find(d => d !== icon && d.w === icon.w && d.h === icon.h);
  const stride = Math.ceil(icon.w / 8);
  const rgba = new Uint8Array(icon.w * icon.h * 4);
  for (let y = 0; y < icon.h; y++) {
    for (let x = 0; x < icon.w; x++) {
      const bit = x & 7;
      const black = (bytes[icon.data + y * stride + (x >> 3)] >> bit) & 1;
      const g = grey ? (bytes[grey.data + y * stride + (x >> 3)] >> bit) & 1 : 0;
      const v = black ? 0x00 : g ? 0xaa : 0xff;
      const p = (y * icon.w + x) * 4;
      rgba[p] = rgba[p + 1] = rgba[p + 2] = v;
      rgba[p + 3] = 255;
    }
  }
  return encodePng(rgba, icon.w, icon.h);
}

function extractSiboIcon(files: AppFile[]): Uint8Array | null {
  // The program binaries carry the real icon; loose .PICs are a
  // fallback (most are full-screen art, which the size gate rejects).
  const candidates = files
    .filter(f => /\.(opa|app)$/i.test(f.rel) && f.size <= 2 * 1024 * 1024)
    .sort((a, b) => (a.rel.split('/').length - b.rel.split('/').length) || (b.size - a.size))
    .concat(files
      .filter(f => f.rel.toLowerCase().endsWith('.pic') && f.size <= 64 * 1024)
      .sort((a, b) => a.size - b.size));
  for (const f of candidates) {
    try {
      const png = decodeSiboPicIcon(new Uint8Array(fs.readFileSync(f.abs)));
      if (png) return png;
    } catch { /* unreadable — keep looking */ }
  }
  return null;
}

function extractIcon(files: AppFile[]): Uint8Array | null {
  // Loose .aif files first — the icon resource by itself.
  const aifs = files
    .filter(f => f.rel.toLowerCase().endsWith('.aif') && f.size <= 256 * 1024)
    .sort((a, b) => a.rel.split('/').length - b.rel.split('/').length);
  for (const f of aifs) {
    try {
      const bytes = new Uint8Array(fs.readFileSync(f.abs));
      const png = iconFromAifAt(bytes, 0, bytes.length);
      if (png) return png;
    } catch { /* malformed AIF — keep looking */ }
  }
  // SIS installers second: ER5 SIS archives store file payloads
  // verbatim (no compression), so the app's .aif is embedded intact —
  // anchor on its signature and decode the bitmaps right there. This is
  // where most "Try it"-able apps keep their only icon.
  const sises = files
    .filter(f => f.rel.toLowerCase().endsWith('.sis') && f.size <= 8 * 1024 * 1024)
    .sort((a, b) => a.rel.split('/').length - b.rel.split('/').length);
  for (const f of sises) {
    try {
      const bytes = new Uint8Array(fs.readFileSync(f.abs));
      const offsets = findAifOffsets(bytes);
      for (let i = 0; i < offsets.length; i++) {
        // Scan from this AIF up to the next embedded file (or 64 KB).
        const end = Math.min(bytes.length,
                             offsets[i + 1] ?? offsets[i] + 64 * 1024,
                             offsets[i] + 64 * 1024);
        const png = iconFromAifAt(bytes, offsets[i], end);
        if (png) return png;
      }
    } catch { /* unreadable SIS — keep looking */ }
  }
  return null;
}

// ── Main ────────────────────────────────────────────────────────────

function main(): void {
  const args = process.argv.slice(2);
  const getArg = (flag: string): string | null => {
    const i = args.indexOf(flag);
    return i >= 0 && i + 1 < args.length ? args[i + 1] : null;
  };
  const outDir = path.resolve(getArg('--out') ?? path.join(REPO, 'frontend', 'public', 'apps'));
  const onlyCategory = getArg('--category');
  const limit = Number(getArg('--limit') ?? Infinity);

  if (!fs.existsSync(LIB)) {
    console.error(`3-Lib library not found at ${LIB}`);
    process.exit(2);
  }
  fs.mkdirSync(outDir, { recursive: true });

  const apps: AppEntry[] = [];
  let skippedNoDir = 0, skippedTooBig = 0, icons = 0;

  for (const cat of CATEGORIES) {
    if (onlyCategory && cat.id !== onlyCategory) continue;
    const catDir = path.join(LIB, cat.id);
    const htmPath = path.join(LIB, cat.htm);
    if (!fs.existsSync(catDir) || !fs.existsSync(htmPath)) {
      console.error(`skip category ${cat.id}: missing dir or catalogue page`);
      continue;
    }
    // Case-insensitive directory lookup.
    const dirs = new Map<string, string>();
    for (const d of fs.readdirSync(catDir)) {
      if (fs.statSync(path.join(catDir, d)).isDirectory()) dirs.set(d.toLowerCase(), d);
    }
    const entries = parseCatalogue(htmPath, new Set(dirs.keys()));

    let count = 0;
    const seenSlugs = new Set<string>();
    for (const entry of entries) {
      if (count >= limit) break;
      const dirName = dirs.get(entry.name.toLowerCase());
      if (!dirName) { skippedNoDir++; continue; }

      const appDir = path.join(catDir, dirName);
      const files = walkDir(appDir);
      if (files.length === 0) { skippedNoDir++; continue; }
      const sizeBytes = files.reduce((s, f) => s + f.size, 0);

      let slug = slugify(dirName);
      while (seenSlugs.has(slug)) slug += '-2';
      seenSlugs.add(slug);
      const id = `${cat.id}/${slug}`;

      const installer = pickInstaller(files, cat.platform, dirName);
      let tryDevice = cat.tryDevice;
      let installKind = installer.kind;
      if (installKind === 'none') tryDevice = null;
      if (cat.platform === 'sibo' && sizeBytes > SIBO_TRY_LIMIT) {
        installKind = 'none'; tryDevice = null; skippedTooBig++;
      }

      // Zip the app directory.
      const zipRel = `files/${cat.id}/${slug}.zip`;
      const zipAbs = path.join(outDir, zipRel);
      fs.mkdirSync(path.dirname(zipAbs), { recursive: true });
      fs.writeFileSync(zipAbs, buildZip(
        files.map(f => ({ name: f.rel, data: fs.readFileSync(f.abs) }))));

      // Icon: SIBO apps embed theirs as a PIC resource in the .OPA/.APP
      // binary; EPOC apps in a loose .AIF or inside the SIS installer.
      // Each platform falls back to the other's format — both appear in
      // the wild, and a real icon beats a placeholder.
      let iconRel: string | null = null;
      {
        const png = cat.platform === 'sibo'
          ? (extractSiboIcon(files) ?? extractIcon(files))
          : (extractIcon(files) ?? extractSiboIcon(files));
        if (png) {
          iconRel = `icons/${cat.id}/${slug}.png`;
          const iconAbs = path.join(outDir, iconRel);
          fs.mkdirSync(path.dirname(iconAbs), { recursive: true });
          fs.writeFileSync(iconAbs, png);
          icons++;
        }
      }

      apps.push({
        id,
        name: dirName.length <= 2 ? dirName.toUpperCase()
          : dirName[0].toUpperCase() + dirName.slice(1),
        category: cat.id,
        section: entry.section,
        date: entry.date ? isoDate(entry.date) : null,
        description: entry.description,
        sizeBytes,
        fileCount: files.length,
        zip: zipRel,
        icon: iconRel,
        devices: cat.devices,
        tryDevice,
        installKind,
        installFile: installer.file,
        readmes: findReadmes(files),
        vault: cat.vault,
      });
      count++;
    }
    console.error(`${cat.id}: ${count} apps`);
  }

  const manifest = {
    version: 1,
    generatedAt: new Date().toISOString(),
    categories: CATEGORIES
      .filter(c => !onlyCategory || c.id === onlyCategory)
      .map(c => ({ id: c.id, label: c.label, platform: c.platform, vault: c.vault })),
    apps,
  };
  fs.writeFileSync(path.join(outDir, 'manifest.json'), JSON.stringify(manifest));

  const totalZipBytes = apps.reduce((s, a) => {
    try { return s + fs.statSync(path.join(outDir, a.zip)).size; } catch { return s; }
  }, 0);
  console.error(`\n${apps.length} apps → ${outDir}`);
  console.error(`zips: ${(totalZipBytes / 1024 / 1024).toFixed(1)} MB, icons: ${icons}, ` +
                `skipped (no dir match): ${skippedNoDir}, sibo-too-big: ${skippedTooBig}`);
}

main();
