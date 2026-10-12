# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
"""Where every texel of the scan's texture sits on the model.

Rasterises the mesh into its own UV space: for each texel of the (cropped)
atlas, which triangle covers it and its barycentric coordinates there. From
that, any per-vertex quantity — position, normal, a smoothed colour — can be
read per texel, which is what lets build_5mx_scan.py repaint the texture by
where things are on the machine (the screen bezel, a key top) rather than by
where they happen to land in the atlas.
"""

import numpy as np


def rasterise(T, FT, width, height, vmax):
    """Return (tri, bary): tri[h, w] = triangle index or -1; bary[h, w] =
    the first two barycentric weights (the third is 1 − both).

    Texel (r, c) of the cropped atlas samples UV (u, v) with
    u = (c + 0.5) / width and v = vmax · (1 − (r + 0.5) / height).
    """
    tri = np.full((height, width), -1, np.int32)
    bary = np.zeros((height, width, 2), np.float32)
    # Triangle corners in texel coordinates.
    X = T[FT][:, :, 0] * width - 0.5
    Y = (vmax - T[FT][:, :, 1]) / vmax * height - 0.5
    x0 = np.clip(np.floor(X.min(1)).astype(int), 0, width - 1)
    x1 = np.clip(np.ceil(X.max(1)).astype(int), 0, width - 1)
    y0 = np.clip(np.floor(Y.min(1)).astype(int), 0, height - 1)
    y1 = np.clip(np.ceil(Y.max(1)).astype(int), 0, height - 1)
    det = (Y[:, 1] - Y[:, 2]) * (X[:, 0] - X[:, 2]) + (X[:, 2] - X[:, 1]) * (Y[:, 0] - Y[:, 2])
    for i in range(len(FT)):
        d = det[i]
        if abs(d) < 1e-12:
            continue
        gy, gx = np.mgrid[y0[i]:y1[i] + 1, x0[i]:x1[i] + 1]
        xs, ys = X[i], Y[i]
        l0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
        l1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
        inside = (l0 >= -1e-4) & (l1 >= -1e-4) & (l0 + l1 <= 1 + 1e-4)
        if not inside.any():
            continue
        r, c = gy[inside], gx[inside]
        tri[r, c] = i
        bary[r, c, 0] = l0[inside]
        bary[r, c, 1] = l1[inside]
    return tri, bary


def dilate(tri, bary, steps):
    """Grow coverage outwards by `steps` texels, copying the neighbour's
    triangle and barycentrics, so texture filtering at island edges samples
    the island's own colour rather than the gutter."""
    for _ in range(steps):
        empty = tri < 0
        if not empty.any():
            break
        for dr, dc in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            src_t = np.roll(tri, (dr, dc), axis=(0, 1))
            src_b = np.roll(bary, (dr, dc), axis=(0, 1))
            take = empty & (src_t >= 0)
            tri[take] = src_t[take]
            bary[take] = src_b[take]
            empty &= ~take
    return tri, bary


def per_texel(values, F, tri, bary):
    """Interpolate a per-vertex array (n, k) to texels (h, w, k)."""
    t = np.maximum(tri, 0)
    f = F[t]
    b0, b1 = bary[..., 0:1], bary[..., 1:2]
    out = values[f[..., 0]] * b0 + values[f[..., 1]] * b1 + values[f[..., 2]] * (1 - b0 - b1)
    out[tri < 0] = 0
    return out
