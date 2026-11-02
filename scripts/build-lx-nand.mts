// Build the Psion LX's internal NAND image: the netBook Pro's 64 MB x16
// NAND, with the OpenEmbedded "soho-image" root filesystem as JFFS2 in its
// first partition, exactly where the initrd's /linuxrc mounts it from
// (/dev/mtdblock/0).
//
//   node --experimental-strip-types scripts/build-lx-nand.mts [rootfs-dir] [out.img] [overlay-dir]
//
// Defaults: the rootfs tree in roms/netbook_LX_prototype/ and
// roms/netbook_LX_prototype/LX_boot/lx-nand.img.gz (gzipped, as the site
// serves it; an output name without .gz writes the raw image). The image is
// committed: a deterministic function of the tree and this script, so it only
// needs rebuilding if either changes.
//
// The image is the NAND as the emulator holds it (core/netbookpro.cpp):
// 131072 pages, each 512 data bytes followed by its 16 spare bytes. The
// kernel's partition table (drivers/mtd/nand/netbookpro.c) puts
// "NetbookPro nand 1" at 4 MB, 56 MB long, and "nand 2" (/home) at 60 MB.
//
// JFFS2 as mkfs.jffs2 makes it, with the options OE used for this machine
// (conf/netbook-pro.conf: "-e 32768 -p 512 -n"): 32 KB erase blocks (the
// kernel uses two 16 KB NAND blocks as one), no cleanmarkers. Each page's
// spare area carries the software ECC nand_base computes on read
// (nand_ecc.c: three bytes per 256, at offsets 0-2 and 3, 6, 7), so every
// page reads back clean.
//
// What the tree in git does not hold, and this puts back:
//  - empty directories (git keeps none): base-files' directory list, plus
//    /sys, /initrd (the pivot_root target /linuxrc insists on), /media/*,
//    /opt, the lx user's /home/lx, and the directories packages shipped
//    empty that the boot scripts and first-boot postinsts write into
//    (/etc/gtk-2.0 for gdk-pixbuf.loaders, /usr/lib/locale for the
//    locale archive, /etc/dropbear for the host keys, …);
//  - device nodes, from the image's own /etc/device_table;
//  - ownership: everything root's, /home/lx the lx user's (1000:1000), as
//    the image recipe's add_netbook_tweaks did. Permissions come from the
//    tree (git keeps the executable bit), directories 0755, /tmp and
//    /var/tmp sticky.
// Timestamps are fixed at the image's build time, so the output is
// reproducible.
import { lstatSync, readdirSync, readFileSync, readlinkSync, writeFileSync } from 'fs';
import { join } from 'path';
import { deflateSync, gzipSync } from 'zlib';

const [
  rootDir = 'roms/netbook_LX_prototype/Psion LX - Linux prototype/soho-image-netbook-pro-20050104074212.rootfs',
  outPath = 'roms/netbook_LX_prototype/LX_boot/lx-nand.img.gz',
  overlayDir,   // optional: files laid over the tree (debugging aid; not used for the shipped image)
] = process.argv.slice(2);

// ── Geometry ────────────────────────────────────────────────────────────
const PAGE = 512, OOB = 16, PAGES = (64 << 20) / PAGE;
const PART_OFFSET = 0x00400000, PART_SIZE = 0x03800000;
const ERASE = 32768;
const BUILD_TIME = 1104824532;            // 2005-01-04 07:42:12 UTC (soho-image-…20050104074212)

// ── CRC32 as JFFS2 uses it: crc32_le seeded with 0, no final inversion ──
const crcTable = new Uint32Array(256);
for (let n = 0; n < 256; n++) {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  crcTable[n] = c >>> 0;
}
function crc32(buf: Uint8Array, start = 0, end = buf.length): number {
  let c = 0;
  for (let i = start; i < end; i++) c = crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return c >>> 0;
}

// ── Filesystem model ────────────────────────────────────────────────────
const S_IFDIR = 0o040000, S_IFREG = 0o100000, S_IFLNK = 0o120000;
const S_IFCHR = 0o020000, S_IFBLK = 0o060000, S_IFIFO = 0o010000;
const DT = { dir: 4, reg: 8, lnk: 10, chr: 2, blk: 6, fifo: 1 } as const;

interface Entry {
  name: string;
  mode: number;
  uid: number;
  gid: number;
  data?: Uint8Array;         // regular file contents / symlink target / device number
  children?: Map<string, Entry>;
  ino?: number;
}

function dir(mode = 0o755, uid = 0, gid = 0): Entry {
  return { name: '', mode: S_IFDIR | mode, uid, gid, children: new Map() };
}

const root = dir();

function walk(fsPath: string, node: Entry) {
  const names = readdirSync(fsPath).sort();
  for (const name of names) {
    const p = join(fsPath, name);
    const st = lstatSync(p);
    let e: Entry;
    if (st.isSymbolicLink()) {
      e = { name, mode: S_IFLNK | 0o777, uid: 0, gid: 0, data: Buffer.from(readlinkSync(p)) };
    } else if (st.isDirectory()) {
      const existing = node.children!.get(name);
      if (existing?.children) { walk(p, existing); continue; }
      e = dir();
      e.name = name;
      walk(p, e);
    } else if (st.isFile()) {
      const exec = (st.mode & 0o111) !== 0;
      e = { name, mode: S_IFREG | (exec ? 0o755 : 0o644), uid: 0, gid: 0, data: readFileSync(p) };
    } else {
      continue;
    }
    node.children!.set(name, e);
  }
}
walk(rootDir, root);
if (overlayDir) walk(overlayDir, root);

function ensureDir(path: string, mode = 0o755, uid = 0, gid = 0): Entry {
  let cur = root;
  for (const part of path.split('/').filter(Boolean)) {
    let next = cur.children!.get(part);
    if (!next) {
      next = dir(mode, uid, gid);
      next.name = part;
      cur.children!.set(part, next);
    }
    cur = next;
  }
  cur.mode = S_IFDIR | mode;
  cur.uid = uid;
  cur.gid = gid;
  return cur;
}

// base-files' debian/directory-list, and the directories its recipe adds.
for (const d of ['bin', 'boot', 'dev', 'etc', 'etc/default', 'etc/skel', 'home', 'lib', 'mnt',
                 'proc', 'root', 'sbin', 'usr', 'usr/bin', 'usr/doc', 'usr/games', 'usr/include',
                 'usr/info', 'usr/lib', 'usr/sbin', 'usr/share', 'usr/share/common-licenses',
                 'usr/share/dict', 'usr/share/info', 'usr/share/man', 'usr/share/misc', 'usr/src',
                 'var', 'sys', 'initrd', 'opt', 'media', 'media/cf', 'media/mmc', 'media/usb',
                 'media/usb2', 'media/pc', 'media/card', 'etc/gtk-2.0',
                 // Directories packages shipped empty, which the boot scripts and
                 // first-boot postinsts write into.
                 'etc/dropbear', 'etc/dhcpc', 'etc/network/if-up.d', 'etc/network/if-down.d',
                 'etc/network/if-post-down.d', 'etc/hotplug/usb', 'usr/lib/locale',
                 'usr/lib/hotplug/firmware', 'usr/local/bin', 'usr/local/lib']) {
  ensureDir(d);
}
ensureDir('tmp', 0o1777);
ensureDir('home/lx', 0o755, 1000, 1000);
ensureDir('root', 0o700);

// Device nodes, from /etc/device_table:
//   <path> <type> <mode> <uid> <gid> <major> <minor> <start> <inc> <count>
const table = root.children!.get('etc')?.children!.get('device_table')?.data;
if (!table) throw new Error('no /etc/device_table in the rootfs');
for (const raw of Buffer.from(table).toString('latin1').split('\n')) {
  const line = raw.trim();
  if (!line || line.startsWith('#')) continue;
  const f = line.split(/\s+/);
  const [path, type] = f;
  const mode = parseInt(f[2], 8), uid = +f[3], gid = +f[4];
  if (type === 'd') { ensureDir(path.replace(/^\//, ''), mode, uid, gid); continue; }
  const parent = ensureDir(path.replace(/^\//, '').split('/').slice(0, -1).join('/'));
  const base = path.split('/').pop()!;
  const ifmt = type === 'c' ? S_IFCHR : type === 'b' ? S_IFBLK : type === 'p' ? S_IFIFO : S_IFREG;
  const make = (name: string, major: number, minor: number) => {
    const e: Entry = { name, mode: ifmt | mode, uid, gid };
    if (ifmt === S_IFCHR || ifmt === S_IFBLK) {
      // JFFS2's old 16-bit device encoding (major << 8 | minor).
      e.data = new Uint8Array([minor & 0xff, major & 0xff]);
    } else if (ifmt === S_IFREG) {
      e.data = new Uint8Array(0);
    }
    parent.children!.set(name, e);
  };
  const major = f[5] === '-' ? 0 : +f[5], minor = f[6] === '-' ? 0 : +f[6];
  if (f[9] && f[9] !== '-') {
    const start = +f[7], inc = +f[8], count = +f[9];
    for (let i = start; i < start + count; i++) make(`${base}${i}`, major, minor + (i - start) * inc);
  } else {
    make(base, major, minor);
  }
}

// ── JFFS2 nodes ─────────────────────────────────────────────────────────
const nodes: Uint8Array[] = [];
let nextIno = 2;
root.ino = 1;

function hdr(buf: Buffer, type: number, totlen: number) {
  buf.writeUInt16LE(0x1985, 0);
  buf.writeUInt16LE(type, 2);
  buf.writeUInt32LE(totlen, 4);
  buf.writeUInt32LE(crc32(buf, 0, 8), 8);
}

function inodeNode(e: Entry, version: number, offset: number, isize: number,
                   dsize: number, payload: Uint8Array, compr: number) {
  const buf = Buffer.alloc(68 + payload.length);
  hdr(buf, 0xe002, buf.length);
  buf.writeUInt32LE(e.ino!, 12);
  buf.writeUInt32LE(version, 16);
  buf.writeUInt32LE(e.mode, 20);
  buf.writeUInt16LE(e.uid, 24);
  buf.writeUInt16LE(e.gid, 26);
  buf.writeUInt32LE(isize, 28);
  buf.writeUInt32LE(BUILD_TIME, 32);   // atime
  buf.writeUInt32LE(BUILD_TIME, 36);   // mtime
  buf.writeUInt32LE(BUILD_TIME, 40);   // ctime
  buf.writeUInt32LE(offset, 44);
  buf.writeUInt32LE(payload.length, 48);
  buf.writeUInt32LE(dsize, 52);
  buf[56] = compr;
  buf[57] = 0;
  buf.writeUInt16LE(0, 58);
  buf.writeUInt32LE(crc32(payload), 60);
  buf.writeUInt32LE(crc32(buf, 0, 60), 64);
  buf.set(payload, 68);
  nodes.push(buf);
}

function direntNode(parent: Entry, version: number, child: Entry, type: number) {
  const name = Buffer.from(child.name, 'latin1');
  const buf = Buffer.alloc(40 + name.length);
  hdr(buf, 0xe001, buf.length);
  buf.writeUInt32LE(parent.ino!, 12);
  buf.writeUInt32LE(version, 16);
  buf.writeUInt32LE(child.ino!, 20);
  buf.writeUInt32LE(BUILD_TIME, 24);
  buf[28] = name.length;
  buf[29] = type;
  buf.writeUInt32LE(crc32(buf, 0, 32), 32);
  buf.writeUInt32LE(crc32(name), 36);
  buf.set(name, 40);
  nodes.push(buf);
}

const COMPR_NONE = 0x00, COMPR_ZLIB = 0x06;
let rawBytes = 0;

function emit(e: Entry) {
  const fmt = e.mode & 0o170000;
  if (fmt === S_IFREG) {
    const data = e.data!;
    rawBytes += data.length;
    if (data.length === 0) {
      inodeNode(e, 1, 0, 0, 0, new Uint8Array(0), COMPR_NONE);
      return;
    }
    let version = 1;
    for (let off = 0; off < data.length; off += 4096) {
      const chunk = data.subarray(off, Math.min(off + 4096, data.length));
      const z = deflateSync(chunk, { level: 9 });
      if (z.length < chunk.length) inodeNode(e, version++, off, data.length, chunk.length, z, COMPR_ZLIB);
      else inodeNode(e, version++, off, data.length, chunk.length, chunk, COMPR_NONE);
    }
  } else if (fmt === S_IFLNK || fmt === S_IFCHR || fmt === S_IFBLK) {
    const d = e.data!;
    inodeNode(e, 1, 0, d.length, d.length, d, COMPR_NONE);
  } else {
    inodeNode(e, 1, 0, 0, 0, new Uint8Array(0), COMPR_NONE);   // directory, FIFO
  }
}

function dtype(e: Entry): number {
  switch (e.mode & 0o170000) {
  case S_IFDIR: return DT.dir;
  case S_IFLNK: return DT.lnk;
  case S_IFCHR: return DT.chr;
  case S_IFBLK: return DT.blk;
  case S_IFIFO: return DT.fifo;
  default: return DT.reg;
  }
}

function build(d: Entry) {
  emit(d);
  let version = 1;
  const kids = [...d.children!.values()].sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
  for (const c of kids) {
    c.ino = nextIno++;
    if (c.children) build(c); else emit(c);
    direntNode(d, version++, c, dtype(c));
  }
}
build(root);

// ── Lay the nodes into 32 KB erase blocks ───────────────────────────────
const fs = new Uint8Array(PART_SIZE).fill(0xff);
let pos = 0;
for (const n of nodes) {
  const len = (n.length + 3) & ~3;
  if (len > ERASE) throw new Error('node larger than an erase block');
  if (Math.floor(pos / ERASE) !== Math.floor((pos + len - 1) / ERASE)) pos = Math.ceil(pos / ERASE) * ERASE;
  if (pos + len > PART_SIZE) throw new Error(`root filesystem does not fit: ${pos + len} > ${PART_SIZE}`);
  fs.set(n, pos);
  pos += len;
}

// ── NAND image with ECC ─────────────────────────────────────────────────
// drivers/mtd/nand/nand_ecc.c: per 256 bytes, column parities from a table
// and line parities from the byte index, packed into three bytes.
const parity = (v: number) => { v ^= v >> 4; v ^= v >> 2; v ^= v >> 1; return v & 1; };
const eccTable = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
  eccTable[i] = (parity(i) << 6) | (parity(i & 0xf0) << 5) | (parity(i & 0x0f) << 4) |
                (parity(i & 0xcc) << 3) | (parity(i & 0x33) << 2) | (parity(i & 0xaa) << 1) | parity(i & 0x55);
}
function ecc256(d: Uint8Array, o: number): [number, number, number] {
  let reg1 = 0, reg2 = 0, reg3 = 0;
  for (let j = 0; j < 256; j++) {
    const idx = eccTable[d[o + j]];
    reg1 ^= idx & 0x3f;
    if (idx & 0x40) { reg3 ^= j; reg2 ^= ~j & 0xff; }
  }
  let a = 0x80, b = 0x80, t1 = 0, t2 = 0;
  for (let i = 0; i < 4; i++) {
    if (reg3 & a) t1 |= b; b >>= 1;
    if (reg2 & a) t1 |= b; b >>= 1;
    a >>= 1;
  }
  b = 0x80;
  for (let i = 0; i < 4; i++) {
    if (reg3 & a) t2 |= b; b >>= 1;
    if (reg2 & a) t2 |= b; b >>= 1;
    a >>= 1;
  }
  return [~t1 & 0xff, ~t2 & 0xff, ((~reg1 << 2) | 3) & 0xff];
}

const image = new Uint8Array(PAGES * (PAGE + OOB)).fill(0xff);
const firstPage = PART_OFFSET / PAGE;
const usedPages = Math.ceil(pos / PAGE);
for (let p = 0; p < usedPages; p++) {
  const src = p * PAGE;
  const dst = (firstPage + p) * (PAGE + OOB);
  image.set(fs.subarray(src, src + PAGE), dst);
  const [a0, a1, a2] = ecc256(fs, src);
  const [b0, b1, b2] = ecc256(fs, src + 256);
  const oob = dst + PAGE;
  image[oob + 0] = a0; image[oob + 1] = a1; image[oob + 2] = a2;
  image[oob + 3] = b0; image[oob + 6] = b1; image[oob + 7] = b2;
}

writeFileSync(outPath, outPath.endsWith('.gz') ? gzipSync(image, { level: 9 }) : image);
console.log(`${nodes.length} nodes, ${nextIno - 1} inodes, ${(rawBytes / 1048576).toFixed(1)} MB of files ` +
            `in ${(pos / 1048576).toFixed(1)} MB of JFFS2 (partition ${PART_SIZE / 1048576} MB) -> ${outPath}`);
