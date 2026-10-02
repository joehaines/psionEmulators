#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# lemmings.spec lists, one DLL to a line, "NAME[uid].DLL:ordinal=symbol,..."
# in ascending ordinal order. From it this writes the two include files
# start.S needs: one import thunk per symbol, and the import address table
# whose slots start out holding the ordinals the loader will replace.
import sys
spec, thunks, iat = sys.argv[1:4]
th, ia = [], []
for line in open(spec):
    line = line.strip()
    if not line:
        continue
    dll, rest = line.split(':', 1)
    for item in rest.split(','):
        o, n = item.split('=')
        th.append('    IMPORT_THUNK %s, i_%s' % (n, n))
        ia.append('i_%s: .word %s' % (n, o))
open(thunks, 'w').write('\n'.join(th) + '\n')
open(iat, 'w').write('\n'.join(ia) + '\n')
