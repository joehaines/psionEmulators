#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
"""Turn the photogrammetry scan of a real Psion Series 5mx into the 3D view's
model.

Input:  tools/psion-5-mx.zip (RealityCapture export: a 100k-vertex,
        200k-triangle OBJ of an OPEN machine plus a texture atlas).
Output: frontend/public/device-3d/5mx-scan.glb       (the parts, as named meshes)
        frontend/public/device-3d/5mx-scan-{8k,4k}.jpg (the atlas, cropped)
        frontend/src/lib/device3d/psion5mx-scan.ts   (measured constants)

What it does:
  1. Aligns the scan: y up (the base's underside, levelled by a plane fit),
     x across the width (the horizontal line in the screen's plane), z
     towards the typist; scales it so the lid is Psion's published 170 mm.
  2. Measures the screen: its plane, and the LCD and digitiser-glass edges
     (read off the flattened screen face; see LCD_* / PAD_* below).
  3. Splits the single fused shell into the three rigid parts of the real
     mechanism (US 5,742,475): the lid (screen leaf, with its side arms),
     the keyboard tray, and the base (with the battery barrel and the rear
     block the open lid leans on).
  4. Works out the shut pose: the keyboard tips flat and slides back until
     its front is flush with the base, and the lid lies face-down on the
     keys with its free edge at the front. Per the patent the lid's arms
     are pivoted to the base, so the lid swings about one fixed axis; that
     axis is the fixed point of the open → shut rigid motion.
  5. Writes a GLB (positions, normals, UVs; texture loaded separately so
     the page can pick 8k or 4k), compressed with gltfpack (meshopt).

Requires numpy and Pillow, and `npx gltfpack` (npm package gltfpack).
Run from the repo root:  python3 tools/scan3d/build_5mx_scan.py
"""

import io
import json
import os
import struct
import subprocess
import sys
import tempfile
import zipfile

import numpy as np
from PIL import Image, ImageFilter

import texclean

Image.MAX_IMAGE_PIXELS = None
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ZIP = os.path.join(ROOT, 'tools', 'psion-5-mx.zip')
OUT_PUBLIC = os.path.join(ROOT, 'frontend', 'public', 'device-3d')
OUT_TS = os.path.join(ROOT, 'frontend', 'src', 'lib', 'device3d', 'psion5mx-scan.ts')

WIDTH_MM = 170.0

# Measured on the flattened screen face (lid-plane coordinates: x across,
# w up the lid from the plane origin), mm. The LCD is the green glass; the
# digitiser pad adds the printed icon strips left of and below it.
LCD = dict(x0=-61.5, x1=73.6, w0=-25.3, w1=29.2)
PAD = dict(x0=-68.5, x1=75.0, w0=-35.0, w1=30.6)


# ── Keyboard (UK) ─────────────────────────────────────────────────────────
# Key positions measured off the scan's keyboard flattened into the deck
# plane (8 px/mm image: x = px / 8 − 88, d = py / 8 − 2, d running from the
# hinge towards the typist). The scanned machine is French, but the 5mx's
# keys are the same shape in every country; the UK legends and codes below
# are transcribed from photographs of a UK 5mx.
DECK_ORIGIN_ZY = (-1.0, 18.6)


def _px(p):
    return p / 8 - 88


def _pd(p):
    return p / 8 - 2


DIGIT_TOP = '!"£$%^&*()'
DIGIT_SUB = ['_', '#', '\\', '@', '<', '>', '[', ']', '{', '}']
LETTER_SUB = {'E': '€', 'Y': '×', 'U': '÷', 'I': '+', 'O': '−', 'P': '=', 'L': ';'}


def uk_keys():
    rows = []

    def key(kid, code, a, b, legend, modifier=False):
        return dict(id=kid, code=code, x0=_px(a), x1=_px(b), legend=legend, modifier=modifier)

    def letter(ch, a, b):
        L = {'main': ch}
        if ch in LETTER_SUB:
            L['sub'] = LETTER_SUB[ch]
        if ch == 'M':
            L['icon'] = 'cdown'
        return key(ch, ord(ch), a, b, L)

    row = [key('Esc', 4, 50, 160, {'main': 'Esc', 'sub': 'On', 'sub2': 'Off', 'word': True})]
    for i, dch in enumerate('1234567890'):
        a = 165 + i * 107.5
        row.append(key(dch, ord(dch), a, a + 105, {'main': dch, 'top': DIGIT_TOP[i], 'sub': DIGIT_SUB[i]}))
    row.append(key('Del', 1, 1245, 1350, {'main': 'Del', 'icon': 'del', 'word': True}))
    rows.append((75, 160, row))

    row = [letter(ch, 95 + i * 114, 205 + i * 114) for i, ch in enumerate('QWERTYUIOP')]
    row.append(key('Enter', 3, 1240, 1355, {'main': 'Enter', 'word': True}))
    rows.append((165, 255, row))

    row = [key('Tab', 2, 55, 125, {'main': 'Tab', 'sub': 'Caps', 'word': True})]
    row += [letter(ch, 130 + i * 113.5, 240 + i * 113.5) for i, ch in enumerate('ASDFGHJKL')]
    row.append(key('Quote', 126, 130 + 9 * 113.5, 240 + 9 * 113.5, {'main': "'", 'top': '~', 'sub': ':'}))
    row.append(key('Enter', 3, 1280, 1355, {'icon': 'enter'}))
    rows.append((275, 380, row))

    row = [key('ShiftL', 18, 75, 180, {'main': 'Shift', 'word': True}, True)]
    row += [letter(ch, 190 + i * 114, 295 + i * 114) for i, ch in enumerate('ZXCVBN')]
    row.append(letter('M', 880, 985))
    row.append(key('Stop', 122, 990, 1095, {'main': '.', 'top': '?', 'icon': 'cup'}))
    row.append(key('Up', 16, 1105, 1210, {'icon': 'up', 'sub': 'PgUp'}))
    row.append(key('ShiftR', 19, 1215, 1330, {'main': 'Shift', 'word': True}, True))
    rows.append((385, 490, row))

    row = [
        key('Ctrl', 22, 75, 180, {'main': 'Ctrl', 'word': True}, True),
        key('Fn', 24, 190, 295, {'main': 'Fn', 'icon': 'fn', 'word': True}, True),
        key('Menu', 148, 300, 405, {'main': 'Menu', 'sub': 'Dial', 'word': True}),
        key('Space', 5, 415, 865, {'sub': 'Backlight'}),
        key('Comma', 121, 880, 985, {'main': ',', 'top': '/', 'sub': 'Help'}),
        key('Left', 14, 990, 1095, {'icon': 'left', 'sub': 'Home'}),
        key('Down', 17, 1105, 1210, {'icon': 'down', 'sub': 'PgDn'}),
        key('Right', 15, 1215, 1330, {'icon': 'right', 'sub': 'End'}),
    ]
    rows.append((495, 610, row))

    keys = []
    for y0, y1, row in rows:
        for k in row:
            k['d0'], k['d1'] = _pd(y0), _pd(y1)
            keys.append(k)
    return keys


def log(*a):
    print(*a, file=sys.stderr)


# ── Load ───────────────────────────────────────────────────────────────────
def load():
    outer = zipfile.ZipFile(ZIP)
    inner = zipfile.ZipFile(io.BytesIO(outer.read('source/Psion.zip')))
    obj = inner.read('Psion_100k-ZR-Tex.obj').decode('ascii').splitlines()
    V, C, T, F, FT = [], [], [], [], []
    for line in obj:
        if line.startswith('v '):
            a = line.split()
            V.append([float(x) for x in a[1:4]])
            C.append([float(x) for x in a[4:7]])
        elif line.startswith('vt '):
            a = line.split()
            T.append([float(a[1]), float(a[2])])
        elif line.startswith('f '):
            a = line.split()[1:]
            F.append([int(p.split('/')[0]) - 1 for p in a])
            FT.append([int(p.split('/')[1]) - 1 for p in a])
    tex = outer.read('textures/Psion_100k-ZR-Tex_u1_v1-b.jpg')
    return (np.array(V), np.array(C), np.array(T), np.array(F), np.array(FT), tex)


def face_normals(V, F):
    P = V[F]
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    A = np.linalg.norm(n, axis=1) / 2
    return n / (2 * A[:, None] + 1e-12), A


def cluster(N, A, seed, deg):
    m = N @ seed > np.cos(np.radians(deg))
    v = (N[m] * A[m, None]).sum(0)
    return v / np.linalg.norm(v), m


# ── Align ──────────────────────────────────────────────────────────────────
def align(V, F):
    N, A = face_normals(V, F)
    # Biggest flat face: the base's underside (seed found by inspection,
    # in the scan's own frame).
    down, _ = cluster(N, A, np.array([-0.847, -0.36, -0.391]), 8)
    up = -down
    screen, _ = cluster(N, A, np.array([0.907, 0.413, -0.081]), 8)
    x = np.cross(screen, up)
    x /= np.linalg.norm(x)
    z = np.cross(x, up)
    R = np.stack([x, up, z])
    Q = (V - V.mean(0)) @ R.T
    # The keyboard (low, far from the barrel) is towards +z.
    low = Q[:, 1] < np.percentile(Q[:, 1], 30)
    if Q[low, 2].mean() < Q[:, 2].mean():
        Q[:, 0] *= -1
        Q[:, 2] *= -1
    Q *= WIDTH_MM / (Q[:, 0].max() - Q[:, 0].min())
    # Level: plane fit to the base's underside.
    N, A = face_normals(Q, F)
    c = Q[F].mean(1)
    m = (N[:, 1] < -0.95) & (c[:, 1] < c[:, 1].min() + 6) & (np.abs(c[:, 0]) < 70) \
        & (c[:, 2] > c[:, 2].min() + 25) & (c[:, 2] < c[:, 2].max() - 38)
    M = np.c_[c[m, 0], c[m, 2], np.ones(m.sum())]
    (a, b, _), *_ = np.linalg.lstsq(M, c[m, 1], rcond=None)
    up = np.array([-a, 1, -b])
    up /= np.linalg.norm(up)
    x = np.array([1.0, 0, 0]) - up * up[0]
    x /= np.linalg.norm(x)
    R = np.stack([x, up, np.cross(x, up)])
    Q = Q @ R.T
    Q[:, 0] -= (Q[:, 0].max() + Q[:, 0].min()) / 2
    Q[:, 1] -= Q[:, 1].min()
    log('levelled: slope', np.degrees(np.arctan(b)), 'extent', Q.min(0), Q.max(0))
    return Q


# ── Measure the screen plane ───────────────────────────────────────────────
def screen_frame(V, F, C):
    N, A = face_normals(V, F)
    ns, m = cluster(N, A, np.array([0, np.cos(np.radians(30)), np.sin(np.radians(30))]), 10)
    c = V[F].mean(1)
    col = C[F].mean(1)
    g = m & (col[:, 1] > col[:, 2] + 0.06) & (col[:, 1] > col[:, 0] - 0.02) & (c[:, 1] > 20)
    o = (c[g] * A[g, None]).sum(0) / A[g].sum()
    w = np.cross(ns, [1, 0, 0])
    w /= np.linalg.norm(w)
    if w[1] < 0:
        w = -w
    log('screen tilt', np.degrees(np.arccos(ns[1])), 'origin', o)
    return o, ns, w


# ── Segment ────────────────────────────────────────────────────────────────
def segment(V, F, T, FT, o, ns, w, hinge):
    c = V[F].mean(1)
    u = T[FT].mean(1)[:, 0]
    s = (c - o) @ ns          # height above the screen face
    a = (c - o) @ w           # distance up the lid
    hz, hy = hinge
    # Geometry first: the lid slab above its knuckle, and anything higher
    # than the base ever reaches (the rear block tops out ~26.5 mm).
    lid = ((s > -9.8) & (s < 3) & (a > -34)) | (c[:, 1] > 27.5)
    tray = ~lid & ((c[:, 2] > 44.3) | ((c[:, 2] > hz - 3) & (c[:, 1] > 8.6)))
    # Round the hinge the two faces meet; there the texture says which is
    # which — RealityCapture unwrapped the screen face (u < 0.1) and the
    # keyboard (0.1 ≤ u < 0.2) as separate islands.
    zone = (c[:, 2] > hz - 8) & (c[:, 2] < hz + 12) & (c[:, 1] > 12) & (c[:, 1] < 27.5)
    lid = np.where(zone & (u < 0.1), True, np.where(zone & (u >= 0.1) & (u < 0.2), False, lid))
    tray = np.where(zone & (u < 0.1), False, np.where(zone & (u >= 0.1) & (u < 0.2), True, tray))
    base = ~lid & ~tray
    log('triangles lid/tray/base', lid.sum(), tray.sum(), base.sum())
    return {'lid': lid, 'tray': tray, 'base': base}


def trim_lid_rim(V, F, parts, o, ns, w):
    """Split off the rim where the scan of the lid's back stops.

    Open, the hinge half of the lid's back faces the rear block and was
    never scanned; the scan bridges from the lid's back onto the block
    there (and down its sides to the base), closing the wedge under the
    screen. Shut, that half is the top of the machine and is drawn by the
    cap and the photo decal instead, so anything of the scan that stands
    proud of their plane there goes into its own part, `rim`: the page
    shows it as scanned while the lid is open and folds it flat into the
    lid's back as the lid closes. Returns the plane offset (along the
    screen normal; negative is outward) the cap and decal use.
    """
    N, _ = face_normals(V, F)
    c = V[F].mean(1)
    s = (c - o) @ ns
    a = (c - o) @ w
    back = parts['lid'] & (N @ ns < -0.8) & (a > 8) & (np.abs(c[:, 0]) < 60)
    s_out = float(np.percentile(s[back], 3))
    rim = parts['lid'] & (a < 8) & (s < s_out + 0.6) & (s < -3)
    parts['lid'] = parts['lid'] & ~rim
    parts['rim'] = rim
    log('lid outer plane', s_out, 'rim triangles', rim.sum())
    return s_out


# ── Shut pose ──────────────────────────────────────────────────────────────
def rot_x(t):
    c, s = np.cos(t), np.sin(t)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


def kinematics(V, F, parts, o, ns, w):
    c = V[F].mean(1)
    tray_c = c[parts['tray']]
    # Keyboard tilt: slope of the key tops (highest tray points in bins).
    zs = np.arange(10, 70, 4.0)
    tops = [tray_c[(tray_c[:, 2] > z) & (tray_c[:, 2] < z + 4) & (np.abs(tray_c[:, 0]) < 60), 1].max() for z in zs]
    slope = np.polyfit(zs, tops, 1)[0]
    tilt = -np.arctan(slope)
    front = V[F[parts['tray']]].reshape(-1, 3)[:, 2].max()
    base_front = V[F[parts['base']]].reshape(-1, 3)
    base_front = base_front[np.abs(base_front[:, 0]) < 60][:, 2].max()
    contact = np.array([0, 8.6, 44.5])
    # Shut tray: rotate by -tilt about the contact edge, slide back so the
    # front is flush with the base.
    # The back must go DOWN as the tray un-tips; pick the sign that does.
    shut_angle = -tilt
    probe = (np.array([0, 15.0, 10.0]) - contact) @ rot_x(shut_angle).T + contact
    if probe[1] > 15.0:
        shut_angle = tilt
    Rt = rot_x(shut_angle)
    tv = V[F[parts['tray']]].reshape(-1, 3)
    tv2 = (tv - contact) @ Rt.T + contact
    slide = tv2[:, 2].max() - base_front
    key_top = tv2[(np.abs(tv2[:, 0]) < 60), 1].max()
    log('tray tilt deg', np.degrees(tilt), 'front', front, 'base front', base_front,
        'slide', slide, 'shut key tops', key_top)
    # Shut lid: screen face down, a hair above the keys; free edge flush
    # with the base front.
    lid_v = V[F[parts['lid']]].reshape(-1, 3)
    a_max = ((lid_v - o) @ w).max()
    face_y = key_top + 0.6
    # Rotation taking ns to -y, about x.
    # R_x(t) takes (y, z) = (sin a, cos a) to (sin(a - t), cos(a - t)), so
    # t = a + 90° sends the screen normal to straight down.
    ang = np.arctan2(ns[1], ns[2])
    theta = ang + np.pi / 2
    Rl = rot_x(theta)
    assert np.allclose(Rl @ ns, [0, -1, 0], atol=0.01)
    # Translation: o goes to (0, face_y, base_front - a_max).
    target = np.array([0, face_y, base_front - a_max])
    t = target - Rl @ o
    # Fixed point in (y, z): (I - R) p = t.
    M = (np.eye(3) - Rl)[1:, 1:]
    pivot_yz = np.linalg.solve(M, t[1:])
    log('lid swing deg', np.degrees(theta), 'pivot (y, z)', pivot_yz, 'shut free edge z', base_front)
    return dict(
        trayTilt=float(tilt),
        trayShutAngle=float(shut_angle),
        trayContact=[float(contact[1]), float(contact[2])],
        traySlide=float(slide),
        lidSwing=float(theta),
        lidPivot=[float(pivot_yz[0]), float(pivot_yz[1])],
        shutKeyTop=float(key_top),
    )


# ── Caps ───────────────────────────────────────────────────────────────────
# The scan is one closed shell of the OPEN machine, so splitting it leaves
# faces that were never scanned because they were hidden: the lower half of
# the lid's back (it lies against the rear block when open) and the front of
# the rear block. Shut, those are on show. Each cap is a flat quad textured
# with a single texel picked from the scanned surface next to it, so it
# carries that surface's colour; while the lid is open every cap sits
# inside the shell, out of sight.
def quad(p0, p1, p2, p3, normal, uv):
    pos = np.array([p0, p1, p2, p3], np.float32)
    nor = np.tile(np.asarray(normal, np.float32), (4, 1))
    uvs = np.tile(np.asarray(uv, np.float32), (4, 1))
    # Wind so the face points along `normal`.
    if np.dot(np.cross(pos[1] - pos[0], pos[2] - pos[0]), normal) < 0:
        idx = np.array([0, 2, 1, 0, 3, 2], np.uint32)
    else:
        idx = np.array([0, 1, 2, 0, 2, 3], np.uint32)
    return pos, nor, uvs, idx


def median_uv(T, FT, mask):
    return np.median(T[FT[mask]].reshape(-1, 2), axis=0)


def caps(V, F, T, FT, parts, o, ns, w, vmax, kin, s_out):
    pivot3 = np.array([0, *kin['lidPivot']])
    swing = kin['lidSwing']
    c = V[F].mean(1)
    N, _ = face_normals(V, F)
    s = (c - o) @ ns
    a = (c - o) @ w
    X = 84.0
    to_gltf = lambda uv: np.array([uv[0], 1 - uv[1] / vmax], np.float32)
    out = {'lid': [], 'base': []}
    # Lid back: where the scanned back face starts (its lowest stretch).
    back = parts['lid'] & (N @ ns < -0.8) & (a < 20)
    a_start = np.percentile(a[back], 2)
    s_back = s_out + 0.15
    # Colour: the lid's back where it is well lit and plain (between the
    # badge and the free edge), not the shadowed strip by the rear block.
    paint = parts['lid'] & (N @ ns < -0.8) & (a > a_start + 15) & (a < 30) \
        & (np.abs(c[:, 0]) > 25) & (np.abs(c[:, 0]) < 60)
    uv = to_gltf(median_uv(T, FT, paint))
    a_foot = -34.0
    P = lambda x, aa, ss: o + (x - o[0]) * np.array([1.0, 0, 0]) + aa * w + ss * ns
    out['lid'].append(quad(P(-X, a_foot, s_back), P(X, a_foot, s_back),
                           P(X, 9.0, s_back), P(-X, 9.0, s_back), -ns, uv))
    out['lid'].append(quad(P(-X, a_foot, -0.5), P(X, a_foot, -0.5),
                           P(X, a_foot, s_back), P(-X, a_foot, s_back), -w, uv))
    # Base: shut, the lid stops short of the rear block, over a stretch of
    # base the scan never saw (the open lid stands in it). A grey filler at
    # the shut lid's height closes it, stepping up to the rear block. Open,
    # it is inside the shell, under the screen.
    shut = (V[F[parts['lid']]].reshape(-1, 3) - pivot3) @ rot_x(swing).T + pivot3
    lid_top = np.percentile(shut[:, 1], 99.5)
    lid_back = np.percentile(shut[:, 2], 0.5)
    rb = parts['base'] & (N[:, 1] > 0.8) & (c[:, 1] > 23) & (np.abs(c[:, 0]) > 20) & (np.abs(c[:, 0]) < 60)
    rb_top = np.median(c[rb, 1])
    rb_front = np.percentile(c[rb, 2], 98)
    out['base'].append(quad([-X, lid_top - 0.2, rb_front - 0.5], [X, lid_top - 0.2, rb_front - 0.5],
                            [X, lid_top - 0.2, lid_back + 1.0], [-X, lid_top - 0.2, lid_back + 1.0], [0, 1, 0], uv))
    out['base'].append(quad([-X, lid_top - 0.2, rb_front], [X, lid_top - 0.2, rb_front],
                            [X, rb_top - 0.3, rb_front], [-X, rb_top - 0.3, rb_front], [0, 0, 1], uv))
    log('caps: lid back from a', a_start, 's', s_back, '; rear block top', rb_top, 'front z', rb_front,
        '; shut lid top', lid_top, 'back z', lid_back)
    return out


def append(part, extra):
    pos, nor, uv, idx = part
    for p2, n2, u2, i2 in extra:
        idx = np.concatenate([idx, i2 + len(pos)])
        pos = np.concatenate([pos, p2])
        nor = np.concatenate([nor, n2])
        uv = np.concatenate([uv, u2])
    return pos, nor, uv, idx


# ── Lid top from a photograph ──────────────────────────────────────────────
# Open, the half of the lid's outside nearest the hinge faces the rear block,
# so the scan never saw it — including the top of the badge. Shut, it is
# the top of the machine. tools/scan3d/5mx-shut-top-photo.jpg is a real
# 5mx photographed shut from above; its lid is rectified into the lid's own
# (x, a) frame, colour-matched to the scan where both see the same paint,
# and laid over the lid as a decal that fades into the scan past the badge.
PHOTO = os.path.join(ROOT, 'tools', 'scan3d', '5mx-shut-top-photo.jpg')
# The photo's lid top face: hinge-left, hinge-right, front-right,
# front-left corners, in photo pixels.
PHOTO_CORNERS = [(112, 47), (737, 72), (741, 300), (18, 233)]
DK = 8                              # decal px per mm
DX0, DX1 = -85.0, 85.0
DA0, DA1 = -35.5, 18.0              # hinge end → where the fade ends
FADE0 = 12.0                        # fade starts (past the badge)


def lid_frame_raster(V, F, T, FT, mask, o, w, atlas, vmax, k, x0, x1, a0, a1):
    """Rasterise triangles (x, a) → texture colour, rows from a0 (top) to a1.
    `atlas` is the texture as shipped: the band of the scan's atlas the mesh
    uses, v from 0 (bottom row) to vmax (top row)."""
    H, W, _ = atlas.shape
    Wi, Hi = int((x1 - x0) * k), int((a1 - a0) * k)
    img = np.zeros((Hi, Wi, 3), np.uint8)
    have = np.zeros((Hi, Wi), bool)
    for i in np.where(mask)[0]:
        P = V[F[i]]
        uv = T[FT[i]]
        X = (P[:, 0] - x0) * k
        A = (((P - o) @ w) - a0) * k
        xa, xb = max(int(np.floor(X.min())), 0), min(int(np.ceil(X.max())), Wi - 1)
        ya, yb = max(int(np.floor(A.min())), 0), min(int(np.ceil(A.max())), Hi - 1)
        if xb < xa or yb < ya:
            continue
        gx, gy = np.meshgrid(np.arange(xa, xb + 1) + 0.5, np.arange(ya, yb + 1) + 0.5)
        d = (A[1] - A[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (A[0] - A[2])
        if abs(d) < 1e-9:
            continue
        l0 = ((A[1] - A[2]) * (gx - X[2]) + (X[2] - X[1]) * (gy - A[2])) / d
        l1 = ((A[2] - A[0]) * (gx - X[2]) + (X[0] - X[2]) * (gy - A[2])) / d
        l2 = 1 - l0 - l1
        inside = (l0 >= -0.01) & (l1 >= -0.01) & (l2 >= -0.01)
        if not inside.any():
            continue
        u = l0 * uv[0, 0] + l1 * uv[1, 0] + l2 * uv[2, 0]
        v = l0 * uv[0, 1] + l1 * uv[1, 1] + l2 * uv[2, 1]
        px = np.clip((u * W).astype(int), 0, W - 1)
        py = np.clip(((vmax - v) / vmax * H).astype(int), 0, H - 1)
        yy, xx = gy[inside].astype(int), gx[inside].astype(int)
        img[yy, xx] = atlas[py[inside], px[inside]]
        have[yy, xx] = True
    return img, have


def rectify_photo(k, x0, x1, a0, a1):
    photo = Image.open(PHOTO).convert('RGB')
    Wi, Hi = int((x1 - x0) * k), int((a1 - a0) * k)
    dst = [(-85, -35.5), (85, -35.5), (85, 35.5), (-85, 35.5)]
    rows, rhs = [], []
    for (X, A), (px, py) in zip(dst, PHOTO_CORNERS):
        u, v = (X - x0) * k, (A - a0) * k
        rows.append([u, v, 1, 0, 0, 0, -u * px, -v * px]); rhs.append(px)
        rows.append([0, 0, 0, u, v, 1, -u * py, -v * py]); rhs.append(py)
    coef = np.linalg.solve(np.array(rows, float), np.array(rhs, float))
    return np.array(photo.transform((Wi, Hi), Image.PERSPECTIVE, tuple(coef), Image.BICUBIC))


def lid_decal(V, F, T, FT, parts, o, ns, w, atlas, vmax):
    N, _ = face_normals(V, F)
    c = V[F].mean(1)
    back = parts['lid'] & (N @ ns < -0.3) & (((c - o) @ ns) < -3)
    scan, have = lid_frame_raster(V, F, T, FT, back, o, w, atlas, vmax, DK, DX0, DX1, DA0, DA1)
    photo = rectify_photo(DK, DX0, DX1, DA0, DA1)
    # The photo's JPEG speckle, enlarged by the rectification, reads as
    # grit at this scale; a light blur leaves the paint's own texture.
    photo = np.array(Image.fromarray(photo).filter(ImageFilter.GaussianBlur(0.8))).astype(np.float32)
    # Colour match on paint both sources see, away from the badge.
    aa = DA0 + (np.arange(scan.shape[0]) + 0.5) / DK
    xx = DX0 + (np.arange(scan.shape[1]) + 0.5) / DK
    band = (aa[:, None] > FADE0) & (np.abs(xx)[None, :] > 25) & (np.abs(xx)[None, :] < 70) & have
    # Mean per channel, and one overall contrast factor (kept near 1: the
    # scan's spread includes its lighting, not just the paint).
    lum_p = photo[band].mean(1)
    lum_s = scan[band].astype(np.float32).mean(1)
    gain = float(np.clip(lum_s.std() / (lum_p.std() + 1e-6), 0.8, 1.15))
    for ch in range(3):
        ps, ss = photo[..., ch][band], scan[..., ch][band].astype(np.float32)
        photo[..., ch] = (photo[..., ch] - ps.mean()) * gain + ss.mean()
    rgb = np.clip(photo, 0, 255).astype(np.uint8)
    # Alpha: inside the lid's rounded outline, fading out past the badge.
    r = 6.0
    ex = np.clip(np.abs(xx)[None, :] - (85 - r), 0, None)
    ea = np.clip(-(aa[:, None]) - (35.5 - r), 0, None)
    corner = np.sqrt(ex ** 2 + ea ** 2) <= r
    # Past the badge, fade into the scan; at the hinge end and the sides,
    # stop short of the photo's dark seam and silhouette (the flat cap
    # under the decal shows there).
    fade = np.clip((DA1 - aa) / (DA1 - FADE0), 0, 1)[:, None] \
        * np.clip((aa + 33.8) / 1.5, 0, 1)[:, None] \
        * np.clip((83.5 - np.abs(xx)) / 1.5, 0, 1)[None, :]
    alpha = (corner * fade * 255).astype(np.uint8)
    log('lid decal', rgb.shape, 'colour-matched on', band.sum(), 'px')
    return np.dstack([rgb, alpha])


# ── The lid's hinge on the keyboard ───────────────────────────────────────
def solve_hinge(kin):
    """The point where the lid is hinged to the keyboard tray.

    The patent hinges the screen leaf to the keyboard leaf. The open pose
    (the scan) and the shut pose (lid face-down on the keys, both flush at
    the front) fix the lid's and the tray's motion; the hinge is the one
    point that both carry to the same place. It comes out at the screen's
    bottom front edge, just above the back of the keyboard.
    """
    piv = np.array([0, *kin['lidPivot']])
    con = np.array([0, *kin['trayContact']])
    Rl = rot_x(kin['lidSwing'])
    tl = piv - Rl @ piv
    Rt = rot_x(kin['trayShutAngle'])
    tt = con - Rt @ con - np.array([0, 0, kin['traySlide']])
    yz = np.linalg.solve((Rl - Rt)[1:, 1:], (tt - tl)[1:])
    log('lid hinge on the keyboard (y, z)', yz)
    # Relative to the tray, the lid turns by its own swing less the tray's.
    return [float(yz[0]), float(yz[1])], float(kin['lidSwing'] - kin['trayShutAngle'])


# ── Screen face ↔ UK device photo ─────────────────────────────────────────
PHOTO_UK = os.path.join(ROOT, 'frontend', 'public', 'device-skins', '5mx.png')
# public/device-skins/5mx.png: the lid's outline and, from the 2D view's
# DEVICE_SKIN_LAYOUTS, its LCD and digitiser-pad rectangles (pixels).
UK_LID = dict(x0=44, x1=1119, y0=4, y1=437)
UK_LCD = dict(x0=0.174 * 1179, x1=(0.174 + 0.702) * 1179, y0=0.064 * 912, y1=(0.064 + 0.340) * 912)
UK_PAD = dict(x0=0.132 * 1179, x1=(0.132 + 0.752) * 1179, y0=0.054 * 912, y1=(0.054 + 0.396) * 912)


def photo_mapping(V, F, parts, o, ns, w):
    N, _ = face_normals(V, F)
    c = V[F].mean(1)
    face = parts['lid'] & (N @ ns > 0.55) & (np.abs((c - o) @ ns) < 2.2)
    a = (c[face] - o) @ w
    a_lo, a_hi = np.percentile(a, 0.3), np.percentile(a, 99.7)
    x_lo, x_hi = np.percentile(c[face, 0], 0.3), np.percentile(c[face, 0], 99.7)
    log('screen face extents x', x_lo, x_hi, 'a', a_lo, a_hi)
    img = np.array(Image.open(PHOTO_UK).convert('RGBA'))
    return dict(
        img=img,
        x_mm=[x_lo, PAD['x0'], LCD['x0'], LCD['x1'], PAD['x1'], x_hi],
        x_px=[UK_LID['x0'], UK_PAD['x0'], UK_LCD['x0'], UK_LCD['x1'], UK_PAD['x1'], UK_LID['x1']],
        # Photo y runs down the screen, i.e. along −a.
        w_mm_neg=[-a_hi, -PAD['w1'], -LCD['w1'], -LCD['w0'], -PAD['w0'], -a_lo],
        y_px=[UK_LID['y0'], UK_PAD['y0'], UK_LCD['y0'], UK_LCD['y1'], UK_PAD['y1'], UK_LID['y1']],
    )


# ── Export ─────────────────────────────────────────────────────────────────
def vertex_normals(V, F):
    n = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
    N = np.zeros_like(V)
    for k in range(3):
        np.add.at(N, F[:, k], n)
    return N / (np.linalg.norm(N, axis=1, keepdims=True) + 1e-12)


def build_part(V, N, T, F, FT, mask, vmax):
    f, ft = F[mask], FT[mask]
    pairs = np.stack([f.reshape(-1), ft.reshape(-1)], 1)
    uniq, inv = np.unique(pairs, axis=0, return_inverse=True)
    pos = V[uniq[:, 0]].astype(np.float32)
    nor = N[uniq[:, 0]].astype(np.float32)
    uv = T[uniq[:, 1]].astype(np.float32).copy()
    uv[:, 1] = 1 - uv[:, 1] / vmax       # glTF v runs down from the top
    idx = inv.reshape(-1).astype(np.uint32)
    return pos, nor, uv, idx


def write_glb(path, parts):
    bin_chunks, views, accessors, meshes, nodes = [], [], [], [], []
    offset = 0

    def add(arr, target, comp, typ, minmax=False):
        nonlocal offset
        data = arr.tobytes()
        pad = (-len(data)) % 4
        bin_chunks.append(data + b'\0' * pad)
        views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(data), 'target': target})
        acc = {'bufferView': len(views) - 1, 'componentType': comp, 'count': len(arr), 'type': typ}
        if minmax:
            acc['min'] = arr.min(0).tolist()
            acc['max'] = arr.max(0).tolist()
        accessors.append(acc)
        offset += len(data) + pad
        return len(accessors) - 1

    for name, (pos, nor, uv, idx) in parts.items():
        p = add(pos, 34962, 5126, 'VEC3', True)
        n = add(nor, 34962, 5126, 'VEC3')
        t = add(uv, 34962, 5126, 'VEC2')
        i = add(idx, 34963, 5125, 'SCALAR')
        meshes.append({'name': name, 'primitives': [{
            'attributes': {'POSITION': p, 'NORMAL': n, 'TEXCOORD_0': t}, 'indices': i, 'material': 0}]})
        nodes.append({'name': name, 'mesh': len(meshes) - 1})
    gltf = {
        'asset': {'version': '2.0', 'generator': 'tools/scan3d/build_5mx_scan.py'},
        'scene': 0, 'scenes': [{'nodes': list(range(len(nodes)))}],
        'nodes': nodes, 'meshes': meshes,
        'materials': [{'name': 'scan', 'pbrMetallicRoughness': {'metallicFactor': 0, 'roughnessFactor': 0.7}}],
        'accessors': accessors, 'bufferViews': views, 'buffers': [{'byteLength': offset}],
    }
    js = json.dumps(gltf, separators=(',', ':')).encode()
    js += b' ' * ((-len(js)) % 4)
    binary = b''.join(bin_chunks)
    with open(path, 'wb') as fh:
        fh.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(binary)))
        fh.write(struct.pack('<II', len(js), 0x4E4F534A) + js)
        fh.write(struct.pack('<II', len(binary), 0x004E4942) + binary)


def main():
    V, C, T, F, FT, tex = load()
    log('loaded', len(V), 'vertices', len(F), 'triangles')
    V = align(V, F)
    o, ns, w = screen_frame(V, F, C)
    hinge = (-1.0, 18.6)   # knuckle axis (z, y), read off a mid-width section
    parts = segment(V, F, T, FT, o, ns, w, hinge)
    s_out = trim_lid_rim(V, F, parts, o, ns, w)
    kin = kinematics(V, F, parts, o, ns, w)

    # Texture: the mesh only uses the bottom band of the atlas.
    vmax = float(np.ceil(T[np.unique(FT)][:, 1].max() * 1000) / 1000)
    im = Image.open(io.BytesIO(tex))
    W, H = im.size
    band = im.crop((0, int(H * (1 - vmax)), W, H)).convert('RGB')
    os.makedirs(OUT_PUBLIC, exist_ok=True)
    hinge_yz, hinge_swing = solve_hinge(kin)
    keys = uk_keys()
    tilt = kin['trayTilt']
    deck = dict(
        o=np.array([0, DECK_ORIGIN_ZY[1], DECK_ORIGIN_ZY[0]]),
        along=np.array([0, -np.sin(tilt), np.cos(tilt)]),
        up=np.array([0, np.cos(tilt), np.sin(tilt)]),
        k=24, x0=-88, x1=88, d0=-2, d1=80,
    )
    rgb, orm = texclean.repaint(
        V, F, T, FT, np.array(band), vmax, parts, dict(o=o, ns=ns, w=w), keys, deck,
        photo_mapping(V, F, parts, o, ns, w))
    clean = Image.fromarray(rgb)
    clean.save(os.path.join(OUT_PUBLIC, '5mx-scan-8k.jpg'), quality=88, optimize=True, progressive=True)
    clean.resize((W // 2, clean.size[1] // 2), Image.LANCZOS).save(
        os.path.join(OUT_PUBLIC, '5mx-scan-4k.jpg'), quality=88, optimize=True, progressive=True)
    Image.fromarray(orm).resize((2048, max(1, orm.shape[0] * 2048 // orm.shape[1])), Image.BILINEAR).save(
        os.path.join(OUT_PUBLIC, '5mx-scan-orm.png'), optimize=True)

    # Colour-matched to the repainted texture, which is what it sits on.
    decal = lid_decal(V, F, T, FT, parts, o, ns, w, rgb, vmax)
    Image.fromarray(decal, 'RGBA').save(os.path.join(OUT_PUBLIC, '5mx-scan-lidtop.png'), optimize=True)

    N = vertex_normals(V, F)
    glb_parts = {k: build_part(V, N, T, F, FT, m, vmax) for k, m in parts.items()}
    for k, extra in caps(V, F, T, FT, parts, o, ns, w, vmax, kin, s_out).items():
        glb_parts[k] = append(glb_parts[k], extra)
    # The decal's quad, just outside the lid's back; UVs span its image.
    s_decal = s_out - 0.05
    P = lambda x, a: o + (x - o[0]) * np.array([1.0, 0, 0]) + a * w + s_decal * ns
    pos = np.array([P(DX0, DA0), P(DX1, DA0), P(DX1, DA1), P(DX0, DA1)], np.float32)
    nor = np.tile((-ns).astype(np.float32), (4, 1))
    uvs = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], np.float32)
    idx = np.array([0, 1, 2, 0, 2, 3], np.uint32)
    if np.dot(np.cross(pos[1] - pos[0], pos[2] - pos[0]), -ns) < 0:
        idx = np.array([0, 2, 1, 0, 3, 2], np.uint32)
    glb_parts['lidtop'] = (pos, nor, uvs, idx)
    with tempfile.TemporaryDirectory() as tmp:
        raw = os.path.join(tmp, 'raw.glb')
        write_glb(raw, glb_parts)
        subprocess.run(['npx', '--yes', 'gltfpack@0.22.0', '-i', raw,
                        '-o', os.path.join(OUT_PUBLIC, '5mx-scan.glb'),
                        '-cc', '-kn', '-km', '-kv', '-noq'], check=True)

    meta = dict(
        screenOrigin=o.tolist(), screenNormal=ns.tolist(), screenUp=w.tolist(),
        lcd=LCD, pad=PAD, hinge=list(hinge), lidBack=float(s_out), **kin,
        lidHinge=hinge_yz, lidHingeSwing=hinge_swing,
        deck=dict(origin=deck['o'].tolist(), along=deck['along'].tolist(), up=deck['up'].tolist()),
        keys=[dict(id=k['id'], code=k['code'], x0=round(k['x0'], 2), x1=round(k['x1'], 2),
                   d0=round(k['d0'], 2), d1=round(k['d1'], 2), **({'modifier': True} if k['modifier'] else {}))
              for k in keys],
    )
    with open(OUT_TS, 'w') as fh:
        fh.write('// SPDX-License-Identifier: LicenseRef-PsionWebEmulator\n')
        fh.write('// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.\n\n')
        fh.write('// GENERATED by tools/scan3d/build_5mx_scan.py from tools/psion-5-mx.zip —\n')
        fh.write('// do not edit by hand. Millimetres; y up, z towards the typist.\n\n')
        fh.write('export const SCAN_5MX = ' + json.dumps(meta, indent=2) + ' as const;\n')
    log('wrote', OUT_PUBLIC, OUT_TS)


if __name__ == '__main__':
    main()
