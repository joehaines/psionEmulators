# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
"""Repaint the scan's texture: clean case, UK screen face, UK keys.

A photogrammetry texture is a photograph of the object: the light and
shadow of the room it was scanned in are baked into it, along with
reflections, blotches where photos disagreed, and — on this machine — a
French keyboard and French screen silkscreen. Under the 3D view's own lights
that baked shading is wrong, and once the lid swings shut it is upside down.

So the texture is rebuilt by what each texel IS on the machine (texmap.py
says where every texel sits):
  - case surfaces: one clean albedo per material (grey paint, black
    plastic), keeping only the scan's small-scale detail (ports, grilles,
    seams, printing) as a ratio against a smoothed copy of itself, so broad
    baked light and shadow drop out;
  - the screen face: the UK device photo (public/device-skins/5mx.png),
    mapped through the measured LCD and digitiser-glass edges, so the
    silkscreen is the English one and lines up with the touch mapping;
  - key tops: clean keycap black with UK legends drawn crisply, from the
    UK 5mx keyboard (transcribed from photographs of one).
"""

import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage, sparse

import texmap

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, 'fonts', 'Jost-Medium.woff')
FONT_FALLBACK = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'

# Albedos (sRGB). The paint is the 5mx's warm metallic grey; the black is
# the keyboard tray / battery barrel plastic.
PAINT = np.array([146, 142, 134], np.float32)
BLACK = np.array([36, 36, 39], np.float32)
KEYCAP = np.array([40, 40, 43], np.float32)
LEGEND = np.array([226, 226, 220], np.float32)


# ── Smoothing on the mesh ─────────────────────────────────────────────────
def adjacency(F, n):
    i = np.concatenate([F[:, 0], F[:, 1], F[:, 2], F[:, 1], F[:, 2], F[:, 0]])
    j = np.concatenate([F[:, 1], F[:, 2], F[:, 0], F[:, 0], F[:, 1], F[:, 2]])
    A = sparse.coo_matrix((np.ones(len(i), np.float32), (i, j)), shape=(n, n)).tocsr()
    A.data[:] = 1
    A = A + sparse.identity(n, np.float32, format='csr')
    d = np.asarray(A.sum(1)).ravel()
    return sparse.diags(1 / d) @ A


def smooth(A, x, iterations):
    for _ in range(iterations):
        x = A @ x
    return x


def masked_blur(img, mask, sigma):
    m = mask.astype(np.float32)
    num = ndimage.gaussian_filter(img * (m[..., None] if img.ndim == 3 else m), sigma=(sigma, sigma, 0) if img.ndim == 3 else sigma)
    den = ndimage.gaussian_filter(m, sigma)
    den = den[..., None] if img.ndim == 3 else den
    return num / np.maximum(den, 1e-4)


def smoothstep(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


# ── Key legends ───────────────────────────────────────────────────────────
def font(size_px, fallback=False):
    path = FONT_FALLBACK if fallback or not os.path.exists(FONT) else FONT
    return ImageFont.truetype(path, max(6, int(round(size_px))))


def glyph_font(ch, size_px):
    """Jost for text; DejaVu for the few symbols Jost lacks."""
    f = font(size_px)
    try:
        if f.getmask(ch).getbbox() is None and ch.strip():
            return font(size_px, True)
    except Exception:
        return font(size_px, True)
    return f


def draw_icon(d, icon, cx, cy, s, w):
    col = (255, 255, 255, 255)
    def tri(pts):
        d.line([(cx + x * s, cy + y * s) for x, y in pts + pts[:1]], fill=col, width=w, joint='curve')
    if icon == 'up':
        tri([(0, -0.42), (0.3, 0.3), (-0.3, 0.3)])
    elif icon == 'down':
        tri([(0, 0.42), (0.3, -0.3), (-0.3, -0.3)])
    elif icon == 'left':
        tri([(-0.48, 0), (0.42, -0.22), (0.42, 0.22)])
    elif icon == 'right':
        tri([(0.48, 0), (-0.42, -0.22), (-0.42, 0.22)])
    elif icon == 'enter':
        d.line([(cx + 0.3 * s, cy - 0.7 * s), (cx + 0.3 * s, cy + 0.3 * s), (cx - 0.45 * s, cy + 0.3 * s)], fill=col, width=w)
        tri([(-0.6, 0.3), (-0.35, 0.16), (-0.35, 0.44)])
    elif icon == 'del':
        d.line([(cx + 0.55 * s, cy), (cx - 0.45 * s, cy)], fill=col, width=w)
        tri([(-0.6, 0), (-0.32, -0.12), (-0.32, 0.12)])
    elif icon == 'fn':
        d.line([(cx - 0.75 * s, cy + 0.18 * s), (cx + 0.75 * s, cy + 0.18 * s), (cx + 0.52 * s, cy - 0.18 * s),
                (cx - 0.52 * s, cy - 0.18 * s), (cx - 0.75 * s, cy + 0.18 * s)], fill=col, width=w)
        tri([(0, 0.05), (0.13, -0.12), (-0.13, -0.12)])
    elif icon in ('cdown', 'cup'):
        r, ox = 0.2 * s, cx - 0.22 * s
        d.ellipse([ox - r, cy - r, ox + r, cy + r], outline=col, width=w)
        d.pieslice([ox - r, cy - r, ox + r, cy + r], 90, 270, fill=col)
        d.line([(cx + 0.08 * s, cy), (cx + 0.42 * s, cy)], fill=col, width=w)
        if icon == 'cup':
            d.line([(cx + 0.25 * s, cy - 0.17 * s), (cx + 0.25 * s, cy + 0.17 * s)], fill=col, width=w)


def legend_raster(keys, k, x0, x1, d0, d1):
    """White-on-transparent legends in the deck frame: column = (x − x0)·k,
    row = (d − d0)·k (d towards the typist, so text reads from the front)."""
    img = Image.new('RGBA', (int((x1 - x0) * k), int((d1 - d0) * k)), (0, 0, 0, 0))
    dr = ImageDraw.Draw(img)

    def text(s, size_mm, cx, cy, anchor='mm'):
        f = glyph_font(s, size_mm * k)
        dr.text((cx, cy), s, font=f, fill=(255, 255, 255, 255), anchor=anchor)

    for key in keys:
        L = key.get('legend') or {}
        if not L:
            continue
        kx0, kx1 = (key['x0'] - x0) * k, (key['x1'] - x0) * k
        ky0, ky1 = (key['d0'] - d0) * k, (key['d1'] - d0) * k
        # The legend sits on the cap's top, inset from the zone.
        ix, iy = 1.6 * k, 1.5 * k
        cx0, cx1, cy0, cy1 = kx0 + ix, kx1 - ix, ky0 + iy, ky1 - iy
        cw, ch = cx1 - cx0, cy1 - cy0
        mx = (cx0 + cx1) / 2
        lw = max(1, int(round(0.2 * k)))
        icon = L.get('icon')
        if L.get('word'):
            has_sub = 'sub' in L or icon == 'fn'
            text(L.get('main', ''), 2.2, mx, cy0 + ch * (0.32 if has_sub else 0.45))
            if icon == 'del':
                draw_icon(dr, 'del', mx, cy0 + ch * 0.74, 3.0 * k, lw)
            if icon == 'fn':
                draw_icon(dr, 'fn', mx, cy0 + ch * 0.78, 3.0 * k, lw)
            if 'sub' in L:
                if 'sub2' in L:
                    text(L['sub'], 1.35, mx, cy0 + ch * 0.62)
                    text(L['sub2'], 1.35, mx, cy0 + ch * 0.88)
                else:
                    text(L['sub'], 1.55, mx, cy0 + ch * 0.82)
        else:
            if 'top' in L:
                text(L['top'], 2.0, cx0 + cw * 0.36, cy0 + ch * 0.2)
            if 'main' in L:
                if 'top' in L:
                    text(L['main'], 2.6, cx0 + cw * 0.36, cy0 + ch * 0.55)
                else:
                    text(L['main'], 3.4, mx, cy0 + ch * 0.42)
            if icon in ('up', 'down', 'left', 'right'):
                draw_icon(dr, icon, mx, cy0 + ch * 0.36, 3.0 * k, lw)
            if icon == 'enter':
                draw_icon(dr, 'enter', mx, cy0 + ch * 0.45, 3.2 * k, lw)
            if icon in ('cdown', 'cup'):
                draw_icon(dr, icon, cx0 + cw * 0.72, cy0 + ch * 0.82, 2.6 * k, lw)
            if 'sub' in L:
                s = L['sub']
                if len(s) > 1:
                    text(s, 1.45, mx, cy0 + ch * 0.84)
                else:
                    text(s, 1.9, cx0 + cw * 0.76, cy0 + ch * 0.8)
    return np.array(img)


# ── The repaint ───────────────────────────────────────────────────────────
def repaint(V, F, T, FT, band, vmax, parts, frame, keys, deck, photo):
    """band: the cropped atlas (h, w, 3) uint8. Returns (rgb, orm) arrays."""
    H, W = band.shape[:2]
    tri, bary = texmap.rasterise(T, FT, W, H, vmax)
    covered = tri >= 0
    tri, bary = texmap.dilate(tri, bary, 6)
    valid = tri >= 0
    t = np.maximum(tri, 0)

    src = band.astype(np.float32)
    lum = src @ np.array([0.299, 0.587, 0.114], np.float32)

    # Per-vertex luminance and warmth from a lightly blurred texture at each
    # corner's UV.
    soft = masked_blur(src, covered, 2.0)
    uv = T[FT]
    cr = np.clip(((vmax - uv[..., 1]) / vmax * H).astype(int), 0, H - 1)
    cc = np.clip((uv[..., 0] * W).astype(int), 0, W - 1)
    corner = soft[cr, cc]                                   # (faces, 3, rgb)
    n = len(V)
    acc = np.zeros((n, 3), np.float32)
    cnt = np.zeros(n, np.float32)
    np.add.at(acc, F.ravel(), corner.reshape(-1, 3))
    np.add.at(cnt, F.ravel(), 1)
    vcol = acc / np.maximum(cnt, 1)[:, None]
    vlum = vcol @ np.array([0.299, 0.587, 0.114], np.float32)
    # The paint is warm, the black plastic neutral, whatever light fell on
    # them: in the scan the paint's R − B is ~0.24 of its luminance, the
    # black's 0.07–0.15. (Luminance alone fails — the room light made lit
    # black brighter than shaded paint.)
    vwarm = (vcol[:, 0] - vcol[:, 2]) / np.maximum(vlum, 20)
    A = adjacency(F, n)
    v_large = smooth(A, vlum, 70)       # ~6 mm: the baked light
    # Smoothed over ~4 mm: a highlight on paint is desaturated (less warm)
    # but small next to the panel it sits on.
    v_warm = smooth(A, vwarm, 30)
    L_large = texmap.per_texel(v_large[:, None], F, tri, bary)[..., 0]
    warm = texmap.per_texel(v_warm[:, None], F, tri, bary)[..., 0]
    L_small = masked_blur(lum, covered, 1.2)

    part = np.full(len(F), 0, np.uint8)
    part[parts['lid']] = 1
    part[parts['tray']] = 2
    tpart = part[np.maximum(tri, 0)]
    g = smoothstep(warm, 0.165, 0.205)  # 0 black … 1 paint
    g[tpart == 2] = 0                   # the keyboard tray is all black plastic
    # The lid is all paint outside, bar the black hinge bar along its foot
    # (and the screen face, repainted below).
    pos = texmap.per_texel(V.astype(np.float32), F, tri, bary)
    a_lid = (pos - frame['o']) @ frame['w']
    g[(tpart == 1) & (a_lid > -33.0)] = 1
    # On the base, the battery barrel across the back is black and the
    # strip above it (behind the shut lid) is paint, whatever the room's
    # light made of them; both measured on the scan.
    # The lower shell, under the black band, is paint all round (the
    # photographs of the shut machine, from the front and both sides).
    base = tpart == 0
    y, z = pos[..., 1], pos[..., 2]
    barrel = base & (z < -40.0) & (y > 3) & (y < 20)
    g[base & (y < 8.6)] = 1
    g[base & (z < -30.0) & (y >= 20)] = 1
    g[barrel] = 0
    # Outside, the lid's foot is paint too; only its inner face carries the
    # black hinge bar.
    s_lid = (pos - frame['o']) @ frame['ns']
    g[(tpart == 1) & (a_lid <= -33.0) & (s_lid < -3)] = 1
    # Detail is the scan's luminance against its own ~1.5 mm blur: seams,
    # grilles, ports and printing are finer than that; the room's light and
    # its highlights are broader, so they drop out.
    L_ref = masked_blur(lum, covered, 14)
    ratio = L_small / np.maximum(L_ref, 4)
    # Paint keeps fine detail but no sparkle; black keeps light print (the
    # moulded label, port markings) with some headroom.
    hi = 1.12 * g + 2.2 * (1 - g)
    lo = 0.4 * g + 0.4 * (1 - g)
    # Surfaces the open-pose scan barely saw — the base's front under the
    # overhanging keyboard, the rear strip and the lid's foot by the hinge —
    # are reconstructed noise, not detail: keep only a trace of it there.
    a_l = (pos - frame['o']) @ frame['w']
    unseen = ((tpart == 0) & (pos[..., 2] > 30) & (pos[..., 1] > 1.5)) \
        | ((tpart == 0) & (pos[..., 2] < -30) & (pos[..., 1] >= 20)) \
        | ((tpart == 1) & (a_l < -28))
    lo = np.where(unseen, 0.85, lo)
    hi = np.where(unseen, 1.06, hi)
    detail = np.clip(ratio, lo, hi)
    albedo = (PAINT * g[..., None] + BLACK * (1 - g[..., None])) * detail[..., None]
    rough = 0.52 * g + 0.62 * (1 - g)
    metal = 0.35 * g

    # Normals per texel.
    P = V[F]
    fn = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    fn /= np.linalg.norm(fn, axis=1, keepdims=True) + 1e-12
    nrm = fn[t].astype(np.float32)

    # ── Screen face: the UK photo ──
    o, ns, w = frame['o'], frame['ns'], frame['w']
    rel = pos - o
    s_n = rel @ ns
    a_w = rel @ w
    face = valid & (tpart == 1) & (nrm @ ns > 0.55) & (np.abs(s_n) < 2.2)
    xs = pos[..., 0]
    img = photo['img']
    px = np.interp(xs, photo['x_mm'], photo['x_px'])
    py = np.interp(-a_w, photo['w_mm_neg'], photo['y_px'])
    inside = face & (px >= 0) & (py >= 0) & (px < img.shape[1] - 1) & (py < img.shape[0] - 1)
    pr = ndimage.map_coordinates
    for ch in range(3):
        samp = pr(img[..., ch].astype(np.float32), [py[inside], px[inside]], order=1)
        # The photo carries its own studio light; take some of it back out
        # so the scene's lights don't brighten it a second time.
        albedo[..., ch][inside] = samp * 0.84
    alpha = pr(img[..., 3].astype(np.float32), [py[inside], px[inside]], order=1) / 255
    # Where the photo is transparent (its rounded corners), keep the paint.
    keep = albedo[inside]
    paint_here = (PAINT * 0.92)[None, :]
    albedo[inside] = keep * alpha[:, None] + paint_here * (1 - alpha[:, None])
    rough[inside] = 0.5
    metal[inside] = 0.15

    # ── Key tops: clean caps with UK legends ──
    d_o, along, up = deck['o'], deck['along'], deck['up']
    drel = pos - d_o
    dd = drel @ along
    dh = drel @ up
    on_tray = valid & (tpart == 2) & (nrm @ up > 0.8)
    leg = legend_raster(keys, deck['k'], deck['x0'], deck['x1'], deck['d0'], deck['d1'])
    # A key's top: within its zone and within a millimetre of the zone's
    # highest points. The zones overlap the Enter key's L, so a texel goes
    # to the first key that claims it.
    cap_mask = np.zeros_like(valid)
    for key in keys:
        z = on_tray & ~cap_mask & (xs > key['x0'] + 0.5) & (xs < key['x1'] - 0.5) \
            & (dd > key['d0'] + 0.4) & (dd < key['d1'] - 0.4)
        if z.sum() < 50:
            continue
        top = np.percentile(dh[z], 90)
        cap_mask |= z & (dh > top - 1.1)
    shade = np.clip(L_large[cap_mask] / np.median(L_large[cap_mask]), 0.9, 1.08)
    albedo[cap_mask] = KEYCAP[None, :] * shade[:, None]
    lc = np.clip(((xs[cap_mask] - deck['x0']) * deck['k']).astype(int), 0, leg.shape[1] - 1)
    lr = np.clip(((dd[cap_mask] - deck['d0']) * deck['k']).astype(int), 0, leg.shape[0] - 1)
    la = leg[lr, lc, 3:4].astype(np.float32) / 255
    albedo[cap_mask] = albedo[cap_mask] * (1 - la) + LEGEND[None, :] * la
    rough[cap_mask] = 0.6
    metal[cap_mask] = 0

    rgb = np.clip(albedo, 0, 255)
    rgb[~valid] = PAINT * 0.6
    orm = np.stack([np.full_like(rough, 255), rough * 255, metal * 255], -1)
    return rgb.astype(np.uint8), orm.astype(np.uint8)
