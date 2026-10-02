#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# A ROM reader for working out which EUser / EFSRV ordinals and which
# executive calls a given machine's ROM has, for the Doom port. It reads the
# ROM image layouts this repository has: R1 (Series 5; names are 8 bits),
# ER5 and ER5u (5mx, Revo, netpad, ...; names 16 bits), with the ROM header
# found wherever it sits in the file (the netpad's image starts with a boot
# partition).
#
#   romx.py ROM ls [SUBSTRING]
#   romx.py ROM exports FILE [ORDINAL...]       FILE is a path or a base name
#   romx.py ROM dis ADDRESS [BYTES]             disassemble ROM code (needs llvm-objdump)
#   romx.py ROM follow FILE ORDINAL             chase the export's branch thunks
#   romx.py ROM find-word WORD                  every place a 32-bit word occurs
#
# As a module: Rom(path).files, .image(file), .export(file, ordinal), ...

import struct
import subprocess
import sys
import tempfile
import os


class Rom:
    def __init__(self, path):
        self.path = path
        self.data = open(path, 'rb').read()
        self.files = []
        self.wide = None
        try:
            self.hdr = self._find_header()
            self.base = self.u32(self.hdr + 0x8c)
            self.size = self.u32(self.hdr + 0x90)
            self._walk_root()
        except SystemExit:
            self._headerless()

    def _headerless(self):
        # A ROM with no usable TRomHeader (the Series 7's): the image is linked at
        # 0x50000000 and loaded at file offset 0; find the root directory by the
        # "System" entry in it - { size, address, att=0x10, namelen=6, "System" }.
        self.hdr = 0
        self.base = 0x50000000
        self.size = len(self.data)
        i = self.data.find(b'\x10\x06System')
        if i < 0:
            raise SystemExit('could not read the ROM directory')
        entry = i - 8
        for start in range(entry - 4, max(0, entry - 0x400), -4):
            size = self.u32(start)
            if start + 4 + 8 <= entry + 8 <= start + size <= len(self.data) and 0x10 <= size <= 0x400:
                self.files = []
                self._walk_dir(self.base + start, 'Z:', 0, False)
                if len(self.files) > 100:
                    self.wide = False
                    return
        raise SystemExit('could not find the root directory')

    def u32(self, off):
        return struct.unpack_from('<I', self.data, off)[0]

    def rd(self, addr, n=4):
        o = self.hdr + (addr - self.base)
        return self.data[o:o + n]

    def rd32(self, addr):
        return struct.unpack('<I', self.rd(addr))[0]

    def _find_header(self):
        # TRomHeader: iRomBase (+0x8c) = 0x5xxxxxxx, iRomSize (+0x90)
        for off in range(0, min(len(self.data), 0x400000), 0x100):
            if off + 0x98 > len(self.data):
                break
            base = struct.unpack_from('<I', self.data, off + 0x8c)[0]
            size = struct.unpack_from('<I', self.data, off + 0x90)[0]
            root = struct.unpack_from('<I', self.data, off + 0x94)[0]
            if base & 0xf0000000 == 0x50000000 and 0x100000 <= size <= 0x2000000 and base <= root < base + size:
                return off
        raise SystemExit('no ROM header found')

    def _walk_dir(self, addr, prefix, depth, wide):
        if depth > 10:
            return
        o = self.hdr + addr - self.base
        if o < 0 or o + 4 > len(self.data):
            return
        end = o + self.u32(o)
        p = o + 4
        while p + 10 <= end:
            esize = self.u32(p)
            eaddr = self.u32(p + 4)
            att = self.data[p + 8]
            nlen = self.data[p + 9]
            if nlen == 0:
                break
            if wide:
                name = self.data[p + 10:p + 10 + nlen * 2].decode('utf-16le', 'replace')
                step = (10 + nlen * 2 + 3) & ~3
            else:
                name = self.data[p + 10:p + 10 + nlen].decode('latin1')
                step = (10 + nlen + 3) & ~3
            path = prefix + '\\' + name
            if att & 0x10:
                self._walk_dir(eaddr, path, depth + 1, wide)
            else:
                self.files.append((path, eaddr, esize, p))
            p += step

    def _walk_root(self):
        # The root is either a list { count, variant, dir address, ... } or the
        # directory itself, and names are either 8-bit or UCS-2; try the four
        # combinations and keep the one that reads as paths.
        root = self.u32(self.hdr + 0x94)
        o = self.hdr + root - self.base
        best = None
        for as_list in (True, False):
            for wide in (False, True):
                self.files = []
                try:
                    if as_list:
                        count = self.u32(o)
                        if not 0 < count < 8:
                            continue
                        for i in range(count):
                            self._walk_dir(self.u32(o + 8 + i * 8), 'Z:', 0, wide)
                    else:
                        self._walk_dir(root, 'Z:', 0, wide)
                except Exception:
                    continue
                ok = sum(1 for f in self.files if all(32 <= ord(c) < 127 for c in f[0]))
                if self.files and ok >= 0.95 * len(self.files) and (best is None or len(self.files) > len(best[0])):
                    best = (self.files, wide)
        if best is None:
            raise SystemExit('could not read the ROM directory')
        self.files, self.wide = best

    def find(self, name):
        n = name.lower()
        for f in self.files:
            if f[0].lower() == n or f[0].lower().endswith('\\' + n.lower().lstrip('\\')):
                return f
        raise SystemExit('no such file: ' + name)

    # TRomImageHeader: the leading fields are the same in R1 and later
    def image(self, name):
        f = self.find(name)
        a = f[1]
        u = lambda o: self.rd32(a + o)
        return dict(path=f[0], addr=a, size=f[2], uid1=u(0), uid2=u(4), uid3=u(8), entry=u(0x10),
                    code=u(0x14), data=u(0x18), codesize=u(0x1c), textsize=u(0x20), datasize=u(0x24),
                    bss=u(0x28), nexp=u(0x3c), expdir=u(0x40))

    def exports(self, name):
        im = self.image(name)
        return [self.rd32(im['expdir'] + 4 * i) for i in range(im['nexp'])]

    def export(self, name, ordinal):
        return self.exports(name)[ordinal - 1]

    def follow(self, addr, limit=6):
        # chase "b target" thunks
        for _ in range(limit):
            w = self.rd32(addr & ~1)
            if (w >> 24) & 0xff == 0xea:
                off = w & 0xffffff
                if off & 0x800000:
                    off -= 1 << 24
                addr = (addr & ~1) + 8 + off * 4
            else:
                break
        return addr

    def find_word(self, word):
        w = struct.pack('<I', word)
        out = []
        i = self.data.find(w)
        while i >= 0:
            out.append(self.base + i - self.hdr)
            i = self.data.find(w, i + 1)
        return out

    def replace(self, path, src, out, rename=None, donor=None):
        """Swap a file's bytes for another's: in place if it fits, else into a
        donor file's space, else past the end of the image (if the declared
        ROM size leaves room). Returns a description."""
        f = self.find(path)
        data = bytearray(self.data)
        room = (f[2] + 3) & ~3
        at = self.hdr + f[1] - self.base
        e = f[3]
        if len(src) <= room:
            data[at:at + room] = bytes(room)
            data[at:at + len(src)] = src
            where = 'in place, %d of %d bytes' % (len(src), room)
        elif donor:
            # one or more adjacent files, named with commas: their space is one region
            ds = [self.find(n) for n in donor.split(',')]
            names = set(d[0] for d in ds)
            lo = ds[0][1]; hi = ds[-1][1] + ds[-1][2]
            for f in self.files:
                if f[0] not in names and f[1] < hi and f[1] + f[2] > lo:
                    raise SystemExit('%s lies inside the donor span' % f[0])
            start = ds[0][1]
            span = ds[-1][1] + ds[-1][2] - start
            if len(src) > span:
                raise SystemExit('%d bytes is too big for the donor space (%d)' % (len(src), span))
            do = self.hdr + start - self.base
            data[do:do + span] = bytes(span)
            data[do:do + len(src)] = src
            data[at:at + room] = bytes(room)
            struct.pack_into('<I', data, e + 4, start)
            where = 'over %s at %#x, %d of %d bytes' % (donor, start, len(src), span)
        else:
            app = (len(data) + 0xfff) & ~0xfff
            if app + len(src) > self.size + self.hdr:
                raise SystemExit('no room past the end of the image (declared size %#x); use --donor' % self.size)
            data.extend(bytes(app - len(data)))
            data.extend(src)
            data[at:at + room] = bytes(room)
            struct.pack_into('<I', data, e + 4, self.base + app - self.hdr)
            where = 'appended at %#x, %d bytes' % (self.base + app - self.hdr, len(src))
        struct.pack_into('<I', data, e, len(src))
        if rename is not None:
            nlen = data[e + 9]
            if rename == 'AUTO':                      # DOOM...X.EXE, as long as the old name
                rename = ('DOOM' + 'X' * nlen)[:nlen - 4] + '.EXE'
            if len(rename) != nlen:
                raise SystemExit('--rename must be %d characters' % nlen)
            if self.wide:
                data[e + 10:e + 10 + 2 * nlen] = rename.encode('utf-16le')
            else:
                data[e + 10:e + 10 + nlen] = rename.encode('latin1')
        open(out, 'wb').write(data)
        return where

    def dis(self, addr, n=64):
        code = self.rd(addr, n)
        with tempfile.TemporaryDirectory() as d:
            b = os.path.join(d, 'c.bin')
            o = os.path.join(d, 'c.o')
            open(b, 'wb').write(code)
            subprocess.run(['llvm-objcopy', '-I', 'binary', '-O', 'elf32-littlearm',
                            '--rename-section', '.data=.text,alloc,load,readonly,code', b, o], check=True)
            out = subprocess.run(['llvm-objdump', '-d', '--triple=armv4', o], capture_output=True, text=True).stdout
        res = []
        for line in out.split('\n'):
            line = line.strip()
            if ':' in line and line[0].isalnum():
                off, rest = line.split(':', 1)
                try:
                    res.append('%08x:%s' % (addr + int(off, 16), rest))
                except ValueError:
                    pass
        return res


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return
    rom = Rom(sys.argv[1])
    cmd = sys.argv[2]
    args = sys.argv[3:]
    if cmd == 'ls':
        print('# base %08x size %08x header at file offset %#x, %d files, %s names' %
              (rom.base, rom.size, rom.hdr, len(rom.files), 'UCS-2' if rom.wide else '8-bit'))
        for p, a, s, _ in rom.files:
            if not args or args[0].lower() in p.lower():
                print('%s\t%08x\t%d' % (p, a, s))
    elif cmd == 'exports':
        ex = rom.exports(args[0])
        want = [int(x) for x in args[1:]]
        for i, a in enumerate(ex, 1):
            if not want or i in want:
                print(i, '%08x' % a, '-> %08x' % rom.follow(a))
    elif cmd == 'follow':
        a = rom.export(args[0], int(args[1]))
        print('%08x -> %08x' % (a, rom.follow(a)))
    elif cmd == 'dis':
        for l in rom.dis(int(args[0], 16), int(args[1], 16) if len(args) > 1 else 64):
            print(l)
    elif cmd == 'replace':
        # replace PATH SRC OUT [--rename NAME] [--donor PATH]
        path, src, out = args[:3]
        rn = args[args.index('--rename') + 1] if '--rename' in args else None
        dn = args[args.index('--donor') + 1] if '--donor' in args else None
        print(rom.replace(path, open(src, 'rb').read(), out, rn, dn))
    elif cmd == 'find-word':
        for a in rom.find_word(int(args[0], 0)):
            print('%08x' % a)


if __name__ == '__main__':
    main()
