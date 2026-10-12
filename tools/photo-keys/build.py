"""Measures the clickable keys on each device-skin photo and writes
frontend/src/lib/photoKeyLayouts.ts.

    python3 tools/photo-keys/build.py [--check DIR]

--check writes one annotated PNG per photo into DIR, every key outlined
and labelled, which is how each layout below was verified by eye.
"""
import os, re, sys, json
sys.path.insert(0, os.path.dirname(__file__))
from kb import detect, flat, tiles, warp, draw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SKINS = os.path.join(ROOT, 'frontend', 'public', 'device-skins')
OUT = os.path.join(ROOT, 'frontend', 'src', 'lib', 'photoKeyLayouts.ts')

# EPOC scan codes (EStdKey*, core/emubase.h). A one-character key name is
# its own upper-case ASCII code, which is what every matrix takes for
# letters, digits and the SIBO punctuation keys.
CODES = {
    'Bksp': 1, 'Tab': 2, 'Enter': 3, 'Esc': 4, 'Space': 5,
    'Home': 8, 'End': 9, 'PgUp': 10, 'PgDn': 11, 'Del': 13,
    'Left': 14, 'Right': 15, 'Up': 16, 'Down': 17,
    'ShiftL': 18, 'ShiftR': 19, 'Psion': 20, 'AltR': 21, 'Ctrl': 22, 'Fn': 24, 'Caps': 26,
    'Comma': 121, 'Stop': 122, 'Slash': 123, 'Backslash': 124, 'Semi': 125, 'Quote': 126,
    'Hash': 127, 'LBracket': 128, 'RBracket': 129, 'Minus': 130, 'Equals': 131,
    'Nkp/': 132, 'Nkp*': 133, 'Nkp-': 134, 'Nkp+': 135, 'Nkp1': 137, 'Nkp2': 138,
    'Nkp3': 139, 'Nkp4': 140, 'Nkp5': 141, 'Nkp6': 142, 'Nkp7': 143, 'Nkp8': 144,
    'Nkp9': 145, 'Nkp0': 146, 'Nkp.': 147,
    'Menu': 148, 'Backlight': 151, 'Help': 159, 'Off': 160,
}
MODIFIERS = {'ShiftL', 'ShiftR', 'Ctrl', 'Fn', 'Psion', 'AltR'}
# What the tooltip calls a key, where its code name isn't already that.
LABELS = {
    'Bksp': 'Delete', 'ShiftL': 'Shift', 'ShiftR': 'Shift', 'Caps': 'Caps Lock',
    'Left': '←', 'Right': '→', 'Up': '↑', 'Down': '↓', 'PgUp': 'Page Up', 'PgDn': 'Page Down',
    'Comma': ',', 'Stop': '.', 'Slash': '/', 'Backslash': '\\', 'Semi': ';', 'Quote': "'",
    'Hash': '#', 'Minus': '-', 'Equals': '=', 'LBracket': '[', 'RBracket': ']',
}

def code_of(name):
    base = re.sub(r'#\d+$', '', name)   # 'Enter#2' = a second zone for Enter
    if len(base) == 1:
        return ord(base.upper())
    if base.startswith('0x'):
        return int(base, 16)
    return CODES[base]

# Key names are either a CODES name / single character, or 'Label=Name'
# where the printed legend differs from the key the matrix knows it as —
# a German keyboard's Z sits where the UK Y does and sends Y's code, which
# the German ROM then reads as Z. A name of '-' marks a detected box that
# is not a key and is dropped.
def keyset(rects, names, grow=False):
    """Pair detected boxes with names in reading order. With grow, the
    surviving boxes are then widened into tiles (see kb.tiles) — after the
    non-keys are dropped, so stray artwork doesn't shrink a key's tile."""
    assert len(rects) == len(names), (len(rects), len(names), names)
    kept = []
    for r, n in zip(rects, names):
        if n == '-': continue
        # A key a printed legend cut in two is named twice: one zone, the
        # union of both pieces.
        for i, (r0, n0) in enumerate(kept):
            if n0 == n:
                kept[i] = ((min(r0[0], r[0]), min(r0[1], r[1]), max(r0[2], r[2]), max(r0[3], r[3])), n)
                break
        else:
            kept.append((tuple(r), n))
    rects = [r for r, _ in kept]
    if grow:
        rects = tiles(rects)
    keys = []
    for r, (_, n) in zip(rects, kept):
        if n == '=':
            label = name = '='
        elif n.startswith('=='):                  # '==0x3d': an '=' key by code
            label, name = '=', n[2:]
        else:
            label, _, name = n.partition('=')
            name = name or label
        label = label.replace('_', ' ')
        base = re.sub(r'#\d+$', '', name)
        label = re.sub(r'#\d+$', '', label)
        keys.append({'spec': n, 'label': LABELS.get(label, label), 'code': code_of(name),
                     'rect': [round(v, 1) for v in r], 'modifier': base in MODIFIERS})
    return keys

def found(photo, roi, **kw):
    return flat(detect(os.path.join(SKINS, photo), roi, **kw))

def split(s):
    return s.split()

# Hand-measured layouts, for photos where the keys can't be found: a row
# of (name, left, right) keys sharing a top and bottom, several rows
# chained, or a grid of rows x columns.
def row(y0, y1, keys):
    return [(x0, y0, x1, y1) for _, x0, x1 in keys], [n for n, _, _ in keys]

def rows(*specs):
    rects, names = [], []
    for r, n in specs:
        rects += r; names += n
    return rects, names

def grid(cols, rows_, names):
    rects, out = [], []
    for r, line in zip(rows_, names):
        for c, n in zip(cols, line):
            if n == '-': continue
            rects.append((c[0], r[0], c[1], r[1])); out.append(n)
    return rects, out

# The four outer corners of a key block — top-left of key tl, top-right of
# tr, bottom-right of br, bottom-left of bl — for warp().
def corners(rects, tl, tr, br, bl):
    return [(rects[tl][0], rects[tl][1]), (rects[tr][2], rects[tr][1]),
            (rects[br][2], rects[br][3]), (rects[bl][0], rects[bl][3])]

PHOTOS = {}

# ── Series 5 (artwork) ──────────────────────────────────────────────────
S5_NAMES = split('''
  Esc 1 2 3 4 5 6 7 8 9 0 Bksp
  Q W E R T Y U I O P Enter
  Tab A S D F G H J K L Quote
  ShiftL Z X C V B N M Stop Up ShiftR
  Ctrl Fn Menu Space Comma Left Down Right
''')
PHOTOS['5.png'] = keyset(found('5.png', (80, 460, 1110, 880), lo=75, minarea=1500, erode=1),
                         S5_NAMES, grow=True)
s5_rects = [k['rect'] for k in PHOTOS['5.png']]

# ── Series 5mx (photo) ──────────────────────────────────────────────────
# Same keyboard as the Series 5, too dark to find, so the Series 5 layout
# is carried across through the four outer corners of the key block (Esc's
# top-left, Delete's top-right, →'s bottom-right, Ctrl's bottom-left),
# measured on this photo.
S5_QUAD = corners(s5_rects, 0, 11, -1, -8)
PHOTOS['5mx.png'] = keyset(warp(s5_rects, S5_QUAD,
                                [(62, 459), (1103, 459), (1142, 878), (38, 879)]), S5_NAMES)

# ── Series 5mx Pro (German artwork) ─────────────────────────────────────
# The Series 5 keyboard again, with German legends on it: each key sends
# the code of the UK key in the same place, and the German ROM maps it.
S5_DE_NAMES = split('''
  Esc 1 2 3 4 5 6 7 8 9 0 Entf=Bksp
  Q W E R T Z=Y U I O P Enter
  Tab A S D F G H J K L #=Quote
  Shift=ShiftL Y=Z X C V B N M ,=Stop Up Shift=ShiftR
  Strg=Ctrl Fn Menü=Menu Space .=Comma Left Down Right
''')
PHOTOS['5mxPro.png'] = keyset(found('5mxPro.png', (15, 345, 805, 665),
                                    lo=48, minarea=600, maxbox=40000, erode=1),
                              S5_DE_NAMES, grow=True)

# ── Ericsson MC218 (photo) ──────────────────────────────────────────────
# A rebadged 5mx: the same again.
PHOTOS['MC218.png'] = keyset(warp(s5_rects, S5_QUAD,
                                  [(50, 408), (962, 408), (985, 740), (35, 740)]), S5_NAMES)

# ── Oregon Scientific Osaris (artwork) ──────────────────────────────────
PHOTOS['osaris.png'] = keyset(found('osaris.png', (15, 455, 835, 760),
                                    lo=80, minarea=600, maxbox=60000, erode=1),
                              S5_NAMES + ['-'], grow=True)

# ── Psion Revo (photo) ──────────────────────────────────────────────────
# Light keys on a dark deck, bright enough to find directly. The three
# extra finds are the hinge strip, the charge LED and the deck's right edge.
PHOTOS['revo.png'] = keyset(found('revo.png', (70, 395, 925, 760),
                                  lo=150, minarea=600, maxbox=60000, erode=1),
                            split('''
  -   Esc 1 2 3 4 5 6 7 8 9 0 Bksp
  -   Q W E R T Y U I O P
  Tab A S D F G H J K L Quote Enter
  ShiftL Z X C V B N M Stop Up ShiftR -
  Ctrl Fn Menu Space Comma Left Down Right
'''), grow=True)

# ── Psion Revo (Conan) (artwork) ────────────────────────────────────────
# The Revo's keyboard, so the Revo layout is carried across by its corners.
_revo = [k['rect'] for k in PHOTOS['revo.png']]
_revo_at = {k['spec']: i for i, k in enumerate(PHOTOS['revo.png'])}
REVO_QUAD = corners(_revo, _revo_at['Esc'], _revo_at['Bksp'], _revo_at['Right'], _revo_at['Ctrl'])
PHOTOS['CONAN.png'] = keyset(warp(_revo, REVO_QUAD,
                                  [(192, 1082), (2000, 1082), (1972, 1825), (221, 1825)]),
                             [k['spec'] for k in PHOTOS['revo.png']])

# ── Series 7 / netBook (photo) ──────────────────────────────────────────
# Too dark and too small to find the keys automatically, so the edges were
# read off contrast-stretched crops. The codes are the machine's own scan
# table (core/sa1100.cpp kKeyMatrix); the bottom row's second and third
# keys are Menu and Fn (the Fn key carries the ▽ every EPOC Fn key does).
_digits = [(d, 132 + 51.2 * i, 132 + 51.2 * i + 48) for i, d in enumerate('1234567890')]
_s7 = rows(
    row(570, 598, [('Esc', 80, 130)] + _digits + [('Minus', 640, 687), ('Delete=Bksp', 692, 767)]),
    row(600, 633, [('Tab', 80, 118), ('Q', 120, 173), ('W', 175, 228), ('E', 230, 283),
                   ('R', 285, 336), ('T', 338, 390), ('Y', 393, 445), ('U', 448, 500),
                   ('I', 503, 555), ('O', 558, 608), ('P', 612, 662), ('Equals', 665, 722),
                   ('Enter', 725, 780)]),
    row(635, 667, [('Backslash', 75, 127), ('A', 130, 183), ('S', 185, 238), ('D', 240, 295),
                   ('F', 297, 350), ('G', 352, 403), ('H', 407, 460), ('J', 463, 515),
                   ('K', 518, 568), ('L', 572, 625), ('Semi', 628, 680), ('Quote', 683, 735),
                   ('Enter#2', 737, 780)]),
    row(668, 700, [('ShiftL', 82, 152), ('Z', 155, 207), ('X', 210, 263), ('C', 265, 320),
                   ('V', 323, 375), ('B', 378, 432), ('N', 433, 488), ('M', 492, 543),
                   ('Comma', 546, 600), ('Stop', 603, 655), ('Up', 658, 712), ('ShiftR', 715, 768)]),
    row(703, 738, [('Ctrl', 80, 148), ('Menu', 152, 203), ('Fn', 207, 262), ('Space', 265, 547),
                   ('Slash', 550, 600), ('Left', 603, 657), ('Down', 660, 715), ('Right', 718, 772)]),
)
PHOTOS['7_netbook.png'] = keyset(*_s7, grow=True)

# ── Geofox One (photo) ──────────────────────────────────────────────────
# The QWERTY board and the calculator pad beside the trackpad. The deck's
# etched application keys and soft keys are already zones of their own
# (GEOFOX_BUTTONS in EmulatorView), so they are not repeated here. Codes
# are the ROM's own matrix table (core/geofox.h kMatrix). The light keys
# are found; the dark-grey ones (Delete, Tab, the pad's operator column
# and its =, AC and C) are too close to the deck to find and were read off
# a magnified crop.
#
# AC and C are the only two keys on the case left once every other code in
# the table has a key, and the table's only two codes left once every key
# has one — 0xA2 and 0xA3. Which is which is not established; this ROM's
# applications do nothing visible with either.
_gf_board = found('geofox_one.png', (12, 470, 515, 635), lo=75, minarea=80, maxbox=6000, erode=1)
_gf_board.insert(12, (464, 495, 509, 516))     # Delete
_gf_board.insert(27, (20, 549, 51, 570))       # Tab
_gf_pad = found('geofox_one.png', (360, 365, 515, 470), lo=75, minarea=40, maxbox=6000, erode=1)
_gf_pad += [(441, 440, 469, 452),                                   # =
            (478, 376, 504, 390), (478, 397, 504, 411), (478, 419, 504, 432),
            (478, 440, 504, 480),                                   # ÷ × − +
            (404, 464, 435, 480), (441, 464, 470, 480)]             # AC C
PHOTOS['geofox_one.png'] = (
    keyset(_gf_board, split('''
      1 2 3 4 5 6 7 8 9 0 Minus Equals Bksp
      Hash Q W E R T Y U I O P LBracket RBracket Backslash
      Tab A S D F G H J K L Semi Quote Enter
      ShiftL Z X C V B N M Comma Stop Slash Up ShiftR
      Esc Fn Ctrl Space Left Down Right
    '''), grow=True)
    + keyset(_gf_pad, split('''
      7=Nkp7 8=Nkp8 9=Nkp9 4=Nkp4 5=Nkp5 6=Nkp6 1=Nkp1 2=Nkp2 3=Nkp3 0=Nkp0 .=Nkp.
      Equals ÷=Nkp/ ×=Nkp* −=Nkp- +=Nkp+ AC=0xA2 C=0xA3
    '''), grow=True))

# ── SIBO clamshells: Series 3, 3a, 3c, 3mx, Pocket Book, Pocket Book II ─
# One keyboard throughout, give or take the bottom row's third key (Caps
# Lock on the Series 3 and Pocket Book, ◆ — the same code — on the rest).
# The codes are the ones core/series3*.cpp put on the matrix: letters,
# digits and the four symbol keys as their ASCII, Psion as EStdKeyLeftAlt.
# '-' alone means "not a key", so the minus key is spelled by its code.
# Menu's legend cuts it in two on most of these, so it is named twice and
# the two pieces become one zone.
SIBO_NAMES = split('''
  Esc 1 2 3 4 5 6 7 8 9 0 + Bksp
  Tab Q W E R T Y U I O P -=0x2d Enter
  Ctrl A S D F G H J K L * /
  ShiftL Z X C V B N M Comma Stop Up ShiftR
''')

_s3_faces = found('3.png', (60, 530, 1135, 815), lo=150, minarea=300, maxbox=40000, erode=2)
PHOTOS['3.png'] = keyset(_s3_faces, SIBO_NAMES + split('Psion Menu Caps Space Help Left Down Right'),
                         grow=True)

PHOTOS['3a.png'] = keyset(found('3a.png', (55, 395, 765, 660), lo=120, minarea=300, maxbox=40000, erode=1),
                          SIBO_NAMES + split('Psion Menu Menu ◆=Caps Space Help Left Down Right'),
                          grow=True)

# The Series 3c in the photo has a French (AZERTY) keyboard, but the 3c ROM
# here is English — so its letters send the letter printed on them, and
# the symbol keys keep the codes of the UK keys in the same places. The
# green Help key is too dark to find.
_s3c = found('3c.png', (40, 480, 920, 817), lo=150, minarea=300, maxbox=40000, erode=2)
PHOTOS['3c.png'] = keyset(_s3c + [(634, 735, 680, 778)], split('''
  Esc 1 2 3 4 5 6 7 8 9 9 0 0 + Bksp
  Tab A Z E R T Y U I O P -=0x2d Enter
  Ctrl Q S D F G H J K L * /
  ShiftL W X C V B N M Comma Stop Up ShiftR
  Menu Psion ◆=Caps Space Left Down Right Menu Help
'''), grow=True)

PHOTOS['3mx.png'] = keyset(found('3mx.png', (40, 500, 1110, 944), lo=150, minarea=300, maxbox=40000, erode=2),
                           SIBO_NAMES + split('Menu Psion ◆=Caps Space Help Left Down Right Menu'),
                           grow=True)

PHOTOS['acornPB2.png'] = keyset(found('acornPB2.png', (40, 490, 1090, 903), lo=150, minarea=300,
                                      maxbox=40000, erode=2),
                                SIBO_NAMES + split('Menu Psion ◆=Caps Space Help Left Down Right Menu'),
                                grow=True)

# The first Pocket Book is the Series 3's keyboard under Acorn's badge, in
# darker greys than the detector can separate from the deck — so the
# Series 3 layout is carried across through the corners of the key block
# (the outer corners of Esc, Delete, → and the Psion key's faces).
_s3_quad = corners(_s3_faces, 0, 12, -1, -8)
PHOTOS['acornPB.png'] = keyset(warp([k['rect'] for k in PHOTOS['3.png']], _s3_quad,
                                    [(69, 567), (1056, 564), (1034, 889), (105, 892)]),
                               [k['spec'] for k in PHOTOS['3.png']])

# ── Siena (photo) ───────────────────────────────────────────────────────
# A keyboard with no digit row, and a calculator pad beside the screen that
# carries the digits instead. Codes from the Siena branch of
# core/series3c.cpp setKeyboardKey. Two keys have no position on the matrix
# the emulator models, so they get no zone: the bottom row's Fn, and the
# pad's OFF. The two finds in the top row between Esc and Delete are the
# Calc and Sheet icons of the app bar, which has zones of its own.
PHOTOS['siena.png'] = (
    keyset(found('siena.png', (80, 470, 1000, 830), lo=110, minarea=300, maxbox=40000, erode=2),
           split('''
      Esc - - Bksp
      Tab Q W E R T Y U I O P -=0x2d Enter
      Ctrl A S D F G H J K L ; /
      ShiftL Z X C V B N M Comma Stop Up ShiftR
      Psion - Menu ◆=Caps Space Help Left Down Right
    '''), grow=True)
    + keyset(found('siena.png', (600, 150, 960, 385), lo=110, minarea=200, maxbox=40000, erode=2),
             split('''
      7 8 9 ÷=/ On/CE=Esc
      4 5 6 ×=* -
      1 2 3 −=0x2d IR_send=PgUp
      0 . = + IR_receive=PgDn
    '''), grow=True))

# ── HC120 (photo) ───────────────────────────────────────────────────────
# Six columns by nine rows, every key on the same grid, so the grid itself
# is measured (each row's top and bottom, each column's left and right)
# rather than each key. Codes from core/series3.cpp hc120KeyMatrix. Of the
# three case buttons above the screen, ON/OFF and the backlight have codes;
# LCD (contrast) has none on the modelled matrix and gets no zone.
_hc_cols = [(60, 110), (122, 174), (187, 239), (252, 303), (316, 368), (380, 434)]
_hc_rows = [(425, 474), (487, 534), (545, 596), (606, 660), (677, 726), (740, 791),
            (805, 858), (871, 924), (937, 991)]
_hc = grid(_hc_cols, _hc_rows, [line.split() for line in '''
  Esc Menu PgUp PgDn Left Info=Right
  A B C D E F
  G H I J K L
  M N O P Q R
  S T U V W X
  Tab 7 8 9 / Y
  Bksp 4 5 6 * Z
  Lock=Caps 1 2 3 -=0x2d Enter
  Shift=ShiftL Space 0 .=Stop + Enter
'''.strip().splitlines()])
PHOTOS['hc120.png'] = keyset(_hc[0] + [(65, 45, 112, 92), (127, 45, 175, 92)],
                             _hc[1] + ['On/Off=Off', 'Backlight'], grow=True)

# ── Workabout, WorkaboutMX (photos) ─────────────────────────────────────
# Black letter keys, found as dark shapes; the light-grey keys above them
# are cut up by their legends, so those were measured as a grid. Codes
# from the Workabout branch of core/series3c.cpp setKeyboardKey. The
# contrast key has no position on the modelled matrix, and the original
# Workabout's green SCAN key is the barcode trigger, not a key — neither
# gets a zone.
WA_LETTERS = split('''
  A B C D E F
  G H I J K L
  M N O P Q R
  S T U V W X
  Y Z
  Space
''')

# The WorkaboutMX in workaboutMX.png. Its symbol keys are exactly the
# matrix's: *: -_ += on the right of the digits, .,  and /? beside the 0.
_wmx_pad = grid([(104, 137), (149, 182), (195, 228), (240, 273)],
                [(327, 355), (368, 396), (409, 438), (451, 479)],
                [split('7 8 9 *'), split('4 5 6 -=0x2d'), split('1 2 3 +'), split('0 . / -')])
PHOTOS['workaboutMX.png'] = keyset(
    [(66, 240, 114, 264), (126, 242, 160, 267), (173, 243, 206, 268), (219, 243, 253, 268),
     (291, 243, 312, 266),
     (80, 279, 113, 303), (127, 281, 159, 304), (173, 282, 205, 305), (219, 282, 252, 306),
     (265, 280, 298, 304)]
    + _wmx_pad[0] + [(242, 451, 279, 479)]
    + found('workaboutMX.png', (60, 480, 320, 715), hi=50, minarea=150, maxbox=30000, erode=2)
    + [(74, 644, 140, 669), (73, 681, 139, 707), (150, 683, 181, 707)],
    split('On/Esc=Esc Tab Up Menu Backlight Off Left Down Right Del=Bksp')
    + _wmx_pad[1] + ['Enter'] + WA_LETTERS + split('Shift=ShiftL Psion Ctrl'),
    grow=True)

# The Workabout photo is of a later keyboard than the matrix the emulator
# models (MAME's): its pad has a ± key, a */ key and a lone : key where the
# matrix has -_, /? and *: — so each sends the matrix key whose unshifted
# legend it carries at the bottom (−, / and, for :, the *: key).
_wa_pad = grid([(112, 153), (167, 208), (222, 263), (277, 318)],
               [(505, 541), (555, 591), (605, 641), (654, 691)],
               [split('±=0x2d - - /'), split('7 8 9 0'), split('4 5 6 :=*'), split('1 2 3 -')])
PHOTOS['workabout.png'] = keyset(
    [(63, 402, 124, 435), (137, 404, 179, 436), (193, 404, 235, 436), (249, 403, 291, 436),
     (331, 404, 354, 434),
     (82, 448, 123, 480), (137, 451, 179, 483), (193, 451, 235, 483), (249, 450, 291, 483),
     (306, 448, 347, 479)]
    + _wa_pad[0] + [(277, 654, 327, 692)]
    + found('workabout.png', (60, 690, 370, 975), hi=55, minarea=250, maxbox=30000, erode=2)
    + [(81, 885, 161, 917), (82, 930, 162, 963), (176, 931, 210, 963)],
    split('On/Esc=Esc Tab Up Menu Backlight Off Left Down Right Del=Bksp')
    + _wa_pad[1] + ['Enter'] + WA_LETTERS + split('Shift=ShiftL Psion Ctrl'),
    grow=True)

# ── Organiser II, Organiser I (photos) ──────────────────────────────────
# Six-by-six keypads. Each zone is the key's whole cell — the key and the
# legends printed round it — measured as a grid. Codes from
# core/organiser{1,2}.cpp setKeyboardKey.
_o2 = grid([(82, 150), (151, 220), (220, 289), (289, 358), (358, 427), (427, 497)],
           [(309, 378), (378, 447), (447, 517), (517, 586), (586, 655), (655, 725)],
           [line.split() for line in '''
  ON=Esc MODE=Menu Up Down Left Right
  A B C D E F
  G H I J K L
  M N O P Q R
  S T U V W X
  SHIFT=ShiftL DEL=Bksp Y Z SPACE=Space EXE=Enter
'''.strip().splitlines()])
PHOTOS['organiser2.png'] = keyset(*_o2)

_o1 = grid([(126, 186), (186, 247), (247, 308), (308, 368), (368, 429), (429, 489)],
           [(314, 375), (375, 434), (434, 491), (491, 551), (551, 612), (612, 670)],
           [line.split() for line in '''
  ON=Esc MODE=Menu SAVE=Up FIND=Down <<=Left >>=Right
  A B C D E F
  G H I J K L
  M N O P Q R
  S T U V W X
  SHIFT=ShiftL Y Z SPACE=Space DELETE=Bksp EXECUTE=Enter
'''.strip().splitlines()])
PHOTOS['organiser1.png'] = keyset(*_o1)

# ── MC400 (photo of its MC600 sibling), MC200 (artwork) ─────────────────
# A full laptop keyboard on the psionmc matrix (core/series3.cpp
# mc400KeyMatrix). The MC400 photo's keys touch, so they were measured off
# magnified crops, row by row; the MC200's light keys are found and its
# grey ones measured. Neither machine's matrix has the F-keys, the row of
# small keys above them or LCD / Record, so those get no zones. Of the
# MC200's eight small keys, only the three whose glyphs say what they are
# (Home, Page Up, Page Down) are mapped.
_mc_digits = [(d, 164 + 50.3 * i, 164 + 50.3 * i + 48) for i, d in enumerate('1234567890')]
_mc400 = rows(
    row(741, 788, [('Esc', 112, 162)] + _mc_digits
           + [('-=0x2d', 667, 716), ('=', 717, 767), ('Backspace=Bksp', 770, 870)]),
    row(790, 839, [('Tab', 104, 179), ('Q', 180, 231), ('W', 232, 281), ('E', 282, 332),
                      ('R', 334, 385), ('T', 386, 437), ('Y', 439, 489), ('U', 490, 540),
                      ('I', 542, 592), ('O', 594, 645), ('P', 646, 696), ('[', 698, 747),
                      (']', 749, 799), ('Enter', 800, 889)]),
    row(839, 889, [('Control=Ctrl', 94, 180), ('A', 182, 235), ('S', 236, 287), ('D', 289, 340),
                      ('F', 342, 395), ('G', 396, 447), ('H', 450, 500), ('J', 504, 555),
                      ('K', 556, 608), ('L', 610, 660), (';', 662, 712), ("'", 715, 765),
                      ('\\', 767, 821), ('Enter#2', 822, 889)]),
    row(889, 940, [('ShiftL', 90, 202), ('Z', 204, 256), ('X', 257, 310), ('C', 311, 364),
                      ('V', 366, 419), ('B', 420, 472), ('N', 474, 529), ('M', 532, 582),
                      (',', 585, 635), ('.', 637, 690), ('/', 692, 745), ('Up', 747, 797),
                      ('ShiftR', 800, 892)]),
    row(942, 1000, [('Psion', 112, 193), ('Caps', 194, 250), ('#', 251, 305), ('Space', 306, 700),
                       ('Left', 700, 754), ('Down', 755, 810), ('Right', 810, 865)]),
    row(664, 679, [('Task=Menu', 716, 746), ('Home', 752, 781), ('PgUp', 787, 817)]),
    row(691, 707, [('Del', 716, 746), ('End', 752, 781), ('PgDn', 787, 817)]),
)
PHOTOS['MC400.png'] = keyset(*_mc400, grow=True)

# The MC200 artwork prints ?/ on two keys of the bottom letter row; the
# second sits where the MC keyboard has its up arrow, and sends that.
_mc200 = found('MC200.png', (85, 765, 975, 965), lo=165, minarea=250, maxbox=30000, erode=2)
PHOTOS['MC200.png'] = keyset(
    _mc200 + [(843, 768, 947, 800), (82, 805, 167, 842),
              (870, 805, 957, 842), (884, 842, 957, 875), (76, 842, 176, 877),
              (67, 877, 197, 912), (877, 875, 975, 907), (100, 913, 195, 962),
              (202, 913, 255, 962), (257, 913, 320, 962), (750, 913, 807, 962),
              (815, 913, 872, 962), (877, 913, 935, 962),
              (855, 711, 887, 726), (895, 711, 930, 726), (900, 730, 931, 746)],
    split('''
      Esc 1 2 3 4 5 6 7 8 9 0 -=0x2d =
      Q W E R T Y U I O P [ ]
      A S D F G H J K L ; ' \\
      Z X C V B N M , . / ↑=Up
      Space
      Backspace=Bksp Tab Enter Enter#2 Caps ShiftL ShiftR Ctrl Psion Alt=AltR Left Down Right
      Home PgUp PgDn
    '''), grow=True)

# ── output ──────────────────────────────────────────────────────────────
HEADER = """\
// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// GENERATED by tools/photo-keys/build.py from public/device-skins/ — do not
// edit by hand; change the measurements there and re-run it.
//
// The keys printed on each device-skin photo, as click zones: for every
// key, its rectangle in the photo's own pixels and the EPOC scan code it
// sends. EmulatorView lays them over the photo in device mode.

export interface PhotoKey {
  /** Tooltip and accessible name. */
  label: string;
  /** EPOC scan code (EStdKey*, core/emubase.h). */
  code: number;
  /** [left, top, right, bottom] in the photo's pixels. */
  rect: readonly [number, number, number, number];
  /** Shift / Ctrl / Fn / Psion: a tap holds it for the next key. */
  modifier?: true;
}

export interface PhotoKeyLayout {
  /** Pixel size of the photo the rects were measured on. */
  width: number;
  height: number;
  keys: readonly PhotoKey[];
}

export const PHOTO_KEY_LAYOUTS: Readonly<Record<string, PhotoKeyLayout>> = {
"""

def main():
    from PIL import Image
    check = None
    if '--check' in sys.argv:
        check = sys.argv[sys.argv.index('--check') + 1]
        os.makedirs(check, exist_ok=True)
    out = [HEADER]
    for photo in sorted(PHOTOS, key=str.lower):
        keys = PHOTOS[photo]
        w, h = Image.open(os.path.join(SKINS, photo)).size
        if check:
            draw(os.path.join(SKINS, photo), keys, os.path.join(check, photo))
        out.append(f"  '{photo}': {{\n    width: {w}, height: {h},\n    keys: [\n")
        # Largest first. A zone drawn later sits on top, and the boxes that
        # overlap are an L-shaped Enter's (its box covers the corner of the
        # key beside its stem) or a key's skirt — in both, the smaller key
        # is the one actually under the pointer.
        area = lambda k: (k['rect'][2] - k['rect'][0]) * (k['rect'][3] - k['rect'][1])
        for k in sorted(keys, key=area, reverse=True):
            x0, y0, x1, y1 = (max(0, round(v)) for v in k['rect'])
            x1, y1 = min(w, x1), min(h, y1)
            mod = ', modifier: true' if k['modifier'] else ''
            out.append(f"      {{ label: {json.dumps(k['label'], ensure_ascii=False)}, "
                       f"code: {k['code']}, rect: [{x0}, {y0}, {x1}, {y1}]{mod} }},\n")
        out.append('    ],\n  },\n')
    out.append('};\n')
    with open(OUT, 'w') as f:
        f.write(''.join(out))
    print(f'wrote {OUT}: {len(PHOTOS)} photos, {sum(len(k) for k in PHOTOS.values())} keys')

if __name__ == '__main__':
    main()
