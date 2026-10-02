#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Rewrites the instructions an ARMv3 (the Series 5's ARM710a) does not have
# out of clang's assembly output, so Doom - which is full of 16-bit data -
# can be compiled with clang (which cannot target ARMv3) and still run on
# the real chip. Reads assembly on stdin (or a file argument), writes it on
# stdout.
#
#   ldrh  rd, [addr]   ->  ldr rd, [addr]; zero-extend the low 16 bits
#   ldrsh rd, [addr]   ->  ldr rd, [addr]; sign-extend the low 16 bits
#   ldrsb rd, [addr]   ->  ldrb rd, [addr]; sign-extend the low 8 bits
#   strh  rd, [addr]   ->  strb rd, [addr]; ror rd by 8; strb rd, [addr+1]; ror rd by 24
#
# The load works because an ARMv3 LDR from an address that is even but not a
# multiple of four returns the aligned word rotated right by 16, which
# puts the two bytes asked for in the low half; the rotate is the defined
# behaviour of every ARM that has not turned alignment checking on. The
# store needs no scratch register: rotating rd by 8 and then by 24 gives it
# back unchanged.
#
# A store that cannot be rewritten without a scratch register (the value
# register is also the base or index) stops the build rather than guess.

import re
import sys

CC = r'(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?'
INSN = re.compile(r'^(\s*)(ldrh|ldrsh|ldrsb|strh)(' + CC + r')\s+(\w+),\s*(\[.*)$')
MEM = re.compile(r'^\[(\w+)(?:,\s*(#-?\d+|-?\w+))?\](!?)(?:,\s*(#-?\d+|-?\w+))?\s*(?:@.*)?$')


def isreg(t):
    return re.match(r'^-?(r\d+|sp|lr|ip|fp|sl|sb)$', t) is not None


def fail(msg, line):
    sys.stderr.write('armv3fix: %s: %s\n' % (msg, line.strip()))
    sys.exit(1)


def addimm(rn, imm, cc):
    if imm == 0:
        return []
    op = 'add' if imm > 0 else 'sub'
    return ['\t%s%s\t%s, %s, #%d' % (op, cc, rn, rn, abs(imm))]


def rewrite(m, line):
    ind, op, cc, rd, rest = m.groups()
    mm = MEM.match(rest.strip())
    if not mm:
        fail('cannot parse address', line)
    rn, off, wb, post = mm.groups()
    out = []
    if op != 'strh':
        base = 'ldrb' if op == 'ldrsb' else 'ldr'
        out.append('%s%s%s\t%s, %s' % (ind, base, cc, rd, rest.strip()))
        if op == 'ldrh':
            out.append('\tmov%s\t%s, %s, lsl #16' % (cc, rd, rd))
            out.append('\tmov%s\t%s, %s, lsr #16' % (cc, rd, rd))
        elif op == 'ldrsh':
            out.append('\tmov%s\t%s, %s, lsl #16' % (cc, rd, rd))
            out.append('\tmov%s\t%s, %s, asr #16' % (cc, rd, rd))
        else:
            out.append('\tmov%s\t%s, %s, lsl #24' % (cc, rd, rd))
            out.append('\tmov%s\t%s, %s, asr #24' % (cc, rd, rd))
        return out

    # strh
    if rd == rn:
        fail('strh with the value register as the base', line)

    def pair(addr_lo, addr_hi):
        return ['%sstrb%s\t%s, %s' % (ind, cc, rd, addr_lo),
                '\tmov%s\t%s, %s, ror #8' % (cc, rd, rd),
                '\tstrb%s\t%s, %s' % (cc, rd, addr_hi),
                '\tmov%s\t%s, %s, ror #24' % (cc, rd, rd)]

    if post is not None:                    # [rn], #imm  or  [rn], rm
        if post.startswith('#'):
            out += pair('[%s]' % rn, '[%s, #1]' % rn)
            out += addimm(rn, int(post[1:]), cc)
        else:
            if post.lstrip('-') == rd:
                fail('strh with the value register as the index', line)
            out += pair('[%s]' % rn, '[%s, #1]' % rn)
            out.append('\t%s%s\t%s, %s, %s' % ('sub' if post.startswith('-') else 'add', cc, rn, rn, post.lstrip('-')))
        return out
    if off is None:                         # [rn]
        return pair('[%s]' % rn, '[%s, #1]' % rn)
    if off.startswith('#'):
        imm = int(off[1:])
        if wb:                              # [rn, #imm]!
            return pair('[%s, #%d]!' % (rn, imm), '[%s, #1]' % rn)
        return pair('[%s, #%d]' % (rn, imm), '[%s, #%d]' % (rn, imm + 1))
    # register offset
    neg = off.startswith('-')
    rm = off.lstrip('-')
    if rm == rd or rm == rn:
        fail('strh with an index register it cannot spare', line)
    first, undo = ('sub', 'add') if neg else ('add', 'sub')
    out.append('%s%s%s\t%s, %s, %s' % (ind, first, cc, rn, rn, rm))
    out += pair('[%s]' % rn, '[%s, #1]' % rn)
    if not wb:
        out.append('\t%s%s\t%s, %s, %s' % (undo, cc, rn, rn, rm))
    return out


def main():
    src = open(sys.argv[1]).read() if len(sys.argv) > 1 else sys.stdin.read()
    res = []
    for line in src.split('\n'):
        m = INSN.match(line)
        if m:
            res.extend(rewrite(m, line))
        else:
            res.append(line)
    sys.stdout.write('\n'.join(res))


main()
