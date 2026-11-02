#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

"""Builds the board's app store: every installable Siena app from the 3-Lib
collection (applib/siena) on one read-only Flash SSD pack, plus an index of
what is on it, as a single image for the siena_apps partition.

  build_app_store.py <applib dir> <out image>

The pack is FEFS24 as frontend/src/lib/fefs.ts writes it (the layout the
web emulator's "Try it on a device" packs have, which EPOC16 mounts), and
each app's files are placed the way frontend/src/lib/appLibrary.ts places
them (siboDirsFor): programs in \\APP\\, data where the app expects it.
With every app on one pack, the Siena keeps it in its slot (drive A:) and
installing an app is just the System screen's Install.

Image layout:
  0x000000  the 8 MiB pack
  0x800000  index: "SIENASTORE1 <pack CRC-32, hex>\\n", then one line per app,
            NAME \\t program path on the pack \\t description
"""

import json
import os
import re
import sys
import zlib

PACK_SIZE = 0x800000
INDEX_SIZE = 0x10000
# Room left for what apps save onto A: (the Siena writes there like any
# Flash pack; the board keeps the writes apart, see PsionSSD::attachInPlace).
FREE_MIN = 0x100000

NULL_PTR = 0xFFFFFF
ROOT_PTR_OFF = 0x0B
VOL_NAME_OFF = 0x0E
ROOT_DIR_OFF = 0x45
DIR_ENTRY_SIZE = 26
FILE_ENTRY_SIZE = 31
FILE_RECORD_SIZE = 17
DATA_CHUNK = 0x8000

FLAG_VALID = 1 << 0
FLAG_IS_FILE = 1 << 2
FLAG_NO_CHILD = 1 << 3
FLAG_LAST_SIBLING = 1 << 5
PROP_DIRECTORY = 1 << 4

ID_STRING = b'Copyright (c) Psion Plc 1991'
# A fixed timestamp (1 Jan 1997, 12:00) keeps the image reproducible.
TIME = 12 * 0x800
DATE = (1997 - 1980) * 0x200 + 1 * 0x20 + 1

README = re.compile(r'\.(txt|me|1st|2nd|asc|nfo|new)$', re.I)


class Pack:
    """An append-only FEFS24 writer: the same records, pointers and order
    fefs.ts's createFlashPack + addFileToPack produce, without re-walking
    the image for every file."""

    def __init__(self, size, volume):
        self.img = bytearray(b'\xff' * size)
        img = self.img
        img[0:2] = b'\xa5\xf1'
        img[4:6] = b'SI'                     # unique id (programmed half)
        img[6:10] = b'\x01\x00\x01\x00'
        img[0x0A] = 0                        # FEFS24
        self._u24(ROOT_PTR_OFF, ROOT_DIR_OFF)
        self._str(VOL_NAME_OFF, volume, 8)
        self._str(VOL_NAME_OFF + 8, '', 3)
        img[0x1D:0x1D + len(ID_STRING)] = ID_STRING
        self._dir_entry(ROOT_DIR_OFF, 'ROOT')
        self.end = ROOT_DIR_OFF + DIR_ENTRY_SIZE
        # path tuple -> (entry offset, last child offset or None)
        self.dirs = {(): [ROOT_DIR_OFF, None]}
        self.files = set()

    def _u24(self, off, v):
        self.img[off:off + 3] = bytes((v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF))

    def _u16(self, off, v):
        self.img[off:off + 2] = bytes((v & 0xFF, (v >> 8) & 0xFF))

    def _str(self, off, s, pad):
        self.img[off:off + pad] = s.encode('ascii').ljust(pad, b' ')[:pad]

    def _dir_entry(self, off, name):
        self._u24(off, NULL_PTR)
        self._str(off + 3, name, 8)
        self._str(off + 11, '', 3)
        self.img[off + 14] = 0xFF & ~FLAG_IS_FILE
        self._u24(off + 15, NULL_PTR)
        self._u24(off + 18, NULL_PTR)
        self.img[off + 21] = PROP_DIRECTORY
        self._u16(off + 22, TIME)
        self._u16(off + 24, DATE)

    def _alloc(self, size):
        off = self.end
        if off + size > len(self.img):
            raise OverflowError('pack full')
        self.end += size
        return off

    def _chain(self, parent, new_off):
        entry = self.dirs[parent]
        if entry[1] is None:
            self.img[entry[0] + 14] &= ~FLAG_NO_CHILD & 0xFF
            self._u24(entry[0] + 15, new_off)
        else:
            self.img[entry[1] + 14] &= ~FLAG_LAST_SIBLING & 0xFF
            self._u24(entry[1], new_off)
        entry[1] = new_off

    def _dir(self, parts):
        for i in range(1, len(parts) + 1):
            key = tuple(parts[:i])
            if key not in self.dirs:
                off = self._alloc(DIR_ENTRY_SIZE)
                self._dir_entry(off, key[-1])
                self._chain(key[:-1], off)
                self.dirs[key] = [off, None]
        return tuple(parts)

    def add(self, dir_path, name, data):
        """False (and nothing written) if that path is already taken."""
        parts = [p for p in dir_path.split('\\') if p]
        full = '\\'.join(parts + [name])
        if full in self.files:
            return False
        stem, _, ext = name.partition('.')
        n = max(1, -(-len(data) // DATA_CHUNK))
        need = FILE_ENTRY_SIZE + len(data) + (n - 1) * FILE_RECORD_SIZE
        if self.end + need + DIR_ENTRY_SIZE * len(parts) > len(self.img):
            raise OverflowError('pack full')
        parent = self._dir(parts)
        base = self._alloc(need)
        chunks = []
        at = base + FILE_ENTRY_SIZE
        for i in range(n):
            piece = data[i * DATA_CHUNK:(i + 1) * DATA_CHUNK]
            self.img[at:at + len(piece)] = piece
            chunks.append((at, len(piece)))
            at += len(piece)
            if i < n - 1:
                at += FILE_RECORD_SIZE
        img = self.img
        self._u24(base, NULL_PTR)
        self._str(base + 3, stem, 8)
        self._str(base + 11, ext, 3)
        img[base + 14] = 0xFF if n == 1 else (0xFF & ~FLAG_NO_CHILD)
        self._u24(base + 15, chunks[0][0] + chunks[0][1] if n > 1 else NULL_PTR)
        self._u24(base + 18, NULL_PTR)
        img[base + 21] = 0
        self._u16(base + 22, TIME)
        self._u16(base + 24, DATE)
        self._u24(base + 26, chunks[0][0])
        self._u16(base + 29, chunks[0][1])
        for i in range(1, n):
            rec = chunks[i - 1][0] + chunks[i - 1][1]
            img[rec] = 0xFF if i == n - 1 else (0xFF & ~FLAG_NO_CHILD)
            self._u24(rec + 1, chunks[i][0] + chunks[i][1] if i < n - 1 else NULL_PTR)
            self._u24(rec + 4, NULL_PTR)
            self._u24(rec + 7, chunks[i][0])
            self._u16(rec + 10, chunks[i][1])
            img[rec + 12] = 0
            self._u16(rec + 13, TIME)
            self._u16(rec + 15, DATE)
        self._chain(parent, base)
        self.files.add(full)
        return True


# ── Placement, as frontend/src/lib/appLibrary.ts ─────────────────────

def to83(name):
    base = name.split('/')[-1]
    dot = base.rfind('.')
    stem = re.sub(r'[^A-Za-z0-9_]', '', base[:dot] if dot > 0 else base).upper()
    ext = re.sub(r'[^A-Za-z0-9_]', '', base[dot + 1:] if dot > 0 else '').upper()
    stem = stem or 'APP'
    return f'{stem[:8]}.{ext[:3]}' if ext else stem[:8]


def dir_component(part):
    return re.sub(r'[^A-Za-z0-9_]', '', part).upper()[:8] or 'DIR'


def sibo_dirs_for(app_dir, rel):
    parts = rel.split('/')
    subs = [dir_component(p) for p in parts[:-1]]
    base = parts[-1]
    if subs and subs[0] == 'APP':
        in_app = '\\'.join(subs)
        at_root = '\\'.join(subs[1:])
        return [in_app, at_root] if at_root else [in_app]
    if not subs:
        if re.search(r'\.(opa|app)$', base, re.I):
            return ['APP']
        return ['APP', f'APP\\{app_dir}', app_dir]
    return ['\\'.join(['APP'] + subs), '\\'.join(subs)]


def app_files(app_root):
    out = []
    for dirpath, dirnames, filenames in os.walk(app_root):
        dirnames.sort()
        for f in sorted(filenames):
            abs_path = os.path.join(dirpath, f)
            out.append((os.path.relpath(abs_path, app_root).replace(os.sep, '/'), abs_path))
    return out


def main():
    lib, out_path = sys.argv[1], sys.argv[2]
    catalogue = json.load(open(os.path.join(lib, 'catalogue.json')))['categories']['siena']
    cat_dir = os.path.join(lib, 'siena')
    dirs = {d.lower(): d for d in os.listdir(cat_dir) if os.path.isdir(os.path.join(cat_dir, d))}

    apps = []
    for entry in catalogue:
        d = dirs.get((entry.get('dir') or entry['name']).lower())
        if not d:
            continue
        files = app_files(os.path.join(cat_dir, d))
        programs = [f for f in files if re.search(r'\.(opa|app)$', f[0], re.I)]
        if not programs:
            continue   # nothing the System screen could install
        # As pickInstaller: the shallowest program, the biggest at a depth;
        # but a bundle of variants for several machines gives the Siena's.
        programs.sort(key=lambda f: ('sien' not in f[0].lower().rpartition('/')[0],
                                     f[0].count('/'), -os.path.getsize(f[1])))
        program = programs[0][0]
        # A variant ("Fullsien/App/X.OPA") is laid out from its own App
        # folder, without the other variants.
        parts = program.split('/')
        k = next((i for i, p in enumerate(parts) if p.lower() == 'app'), 0)
        if k > 0:
            prefix = '/'.join(parts[:k]) + '/'
            files = [(rel[len(prefix):], a) for rel, a in files if rel.startswith(prefix)] + \
                    [(rel, a) for rel, a in files if '/' not in rel]
            program = program[len(prefix):]
        apps.append((entry, d, files, program))
    apps.sort(key=lambda a: a[0]['name'].upper())

    # Readmes are for people; the store says what each app is. They go on
    # only if everything fits with them.
    for with_readmes in (True, False):
        pack = Pack(PACK_SIZE, 'APPS')
        index, clashes = [], 0
        try:
            for entry, d, files, program in apps:
                app_dir = dir_component(entry['name'])
                for rel, abs_path in files:
                    if not with_readmes and README.search(rel) and rel != program:
                        continue
                    data = open(abs_path, 'rb').read()
                    for target in sibo_dirs_for(app_dir, rel):
                        if not pack.add(target, to83(rel), data):
                            clashes += 1
                path = sibo_dirs_for(app_dir, program)[0] + '\\' + to83(program)
                if path.count('\\') > 1:
                    # Install only lists \APP\ itself: the program goes
                    # there too, its own folder kept as it was.
                    pack.add('APP', to83(program), open(dict(files)[program], 'rb').read())
                    path = 'APP\\' + to83(program)
                desc = (entry.get('description') or '').replace('\t', ' ').replace('\n', ' ')
                index.append(f"{entry['name']}\t{path}\t{desc}")
            if len(pack.img) - pack.end < FREE_MIN:
                raise OverflowError()
        except OverflowError:
            continue
        break
    else:
        sys.exit('the Siena apps do not fit on an 8 MiB pack')

    crc = zlib.crc32(bytes(pack.img))
    text = (f'SIENASTORE1 {crc:08x}\n' + '\n'.join(index) + '\n').encode('latin-1', 'replace')
    if len(text) > INDEX_SIZE:
        sys.exit('app store index too big')
    with open(out_path, 'wb') as f:
        f.write(pack.img)
        f.write(text.ljust(INDEX_SIZE, b'\0'))
    print(f'app store: {len(apps)} apps, pack {pack.end // 1024} KiB of {PACK_SIZE // 1024}'
          f'{"" if with_readmes else " (readmes left off)"}, {clashes} duplicate paths skipped')


if __name__ == '__main__':
    main()
