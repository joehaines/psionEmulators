"""Helpers for measuring the clickable keys on the device-skin photos.

detect()  finds key faces by brightness + connected components.
tiles()   grows each face to fill half the gap to its neighbours, so a
          click anywhere on the keycap (skirt included) lands on the key.
warp()    carries a layout measured on one photo onto another photo of
          the same keyboard, through the homography fixed by four corners.
"""
from collections import deque
import numpy as np
from PIL import Image, ImageDraw

def load(path):
    im = Image.open(path).convert('RGBA')
    return im, np.asarray(im).astype(np.float32)

def detect(path, roi, lo=None, hi=None, minarea=200, maxarea=10**9, erode=0,
           rowtol=0.5, maxbox=None):
    im, arr = load(path)
    lum = 0.299 * arr[..., 0] + 0.587 * arr[..., 1] + 0.114 * arr[..., 2]
    H, W = lum.shape
    x0, y0, x1, y1 = roi
    m = np.zeros((H, W), bool)
    mm = arr[y0:y1, x0:x1, 3] > 128
    sub = lum[y0:y1, x0:x1]
    if lo is not None: mm &= sub >= lo
    if hi is not None: mm &= sub <= hi
    m[y0:y1, x0:x1] = mm
    for _ in range(erode):
        e = m.copy()
        e[1:, :] &= m[:-1, :]; e[:-1, :] &= m[1:, :]
        e[:, 1:] &= m[:, :-1]; e[:, :-1] &= m[:, 1:]
        m = e
    lab = np.zeros((H, W), np.int32)
    boxes = []
    n = 0
    ys, xs = np.nonzero(m)
    for sy, sx in zip(ys, xs):
        if lab[sy, sx]: continue
        n += 1
        q = deque([(sy, sx)]); lab[sy, sx] = n
        bx0 = bx1 = sx; by0 = by1 = sy; area = 0
        while q:
            y, x = q.popleft(); area += 1
            bx0 = min(bx0, x); bx1 = max(bx1, x); by0 = min(by0, y); by1 = max(by1, y)
            for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= ny < H and 0 <= nx < W and m[ny, nx] and not lab[ny, nx]:
                    lab[ny, nx] = n; q.append((ny, nx))
        if minarea <= area <= maxarea and (maxbox is None or (bx1 - bx0) * (by1 - by0) <= maxbox):
            boxes.append((int(bx0 - erode), int(by0 - erode), int(bx1 + 1 + erode), int(by1 + 1 + erode)))
    return rows_of(boxes, rowtol)

def rows_of(boxes, rowtol=0.5):
    boxes = sorted(boxes, key=lambda b: (b[1] + b[3]) / 2)
    rows = []
    for b in boxes:
        cy = (b[1] + b[3]) / 2; h = b[3] - b[1]
        for r in rows:
            rc = np.mean([(c[1] + c[3]) / 2 for c in r]); rh = np.mean([c[3] - c[1] for c in r])
            if abs(cy - rc) < rowtol * min(h, rh):
                r.append(b); break
        else:
            rows.append([b])
    for r in rows: r.sort(key=lambda b: b[0])
    return rows

def flat(rows):
    return [b for r in rows for b in r]

def tiles(boxes, pad=None):
    """Grow each box by half the gap to its nearest neighbour on each side
    (capped at `pad` px, default the median gap), so neighbouring keys meet
    and the keycap's skirt is part of the key."""
    boxes = [list(map(float, b)) for b in boxes]
    gaps = []
    for i, a in enumerate(boxes):
        right = [b[0] - a[2] for j, b in enumerate(boxes)
                 if j != i and b[0] >= a[2]
                 and min(a[3], b[3]) - max(a[1], b[1]) > 0.3 * (a[3] - a[1])]
        if right: gaps.append(min(right))
    med = float(np.median(gaps)) if gaps else 8.0
    cap = pad if pad is not None else med / 2
    out = []
    for i, a in enumerate(boxes):
        l = r = t = btm = cap
        for j, b in enumerate(boxes):
            if i == j: continue
            yo = min(a[3], b[3]) - max(a[1], b[1])
            xo = min(a[2], b[2]) - max(a[0], b[0])
            if yo > 0:
                if b[0] >= a[2]: r = min(r, (b[0] - a[2]) / 2)
                if b[2] <= a[0]: l = min(l, (a[0] - b[2]) / 2)
            if xo > 0:
                if b[1] >= a[3]: btm = min(btm, (b[1] - a[3]) / 2)
                if b[3] <= a[1]: t = min(t, (a[1] - b[3]) / 2)
        out.append([a[0] - l, a[1] - t, a[2] + r, a[3] + btm])
    return out

def homography(src, dst):
    A = []
    for (x, y), (u, v) in zip(src, dst):
        A.append([x, y, 1, 0, 0, 0, -u * x, -u * y, -u])
        A.append([0, 0, 0, x, y, 1, -v * x, -v * y, -v])
    _, _, vt = np.linalg.svd(np.array(A))
    return vt[-1].reshape(3, 3) / vt[-1][-1]

def warp(rects, src_quad, dst_quad):
    """Map rects through the homography taking src_quad onto dst_quad
    (each four corners: TL, TR, BR, BL). Returns axis-aligned boxes
    inscribed between the mapped corners' extents."""
    Hm = homography(src_quad, dst_quad)
    out = []
    for x0, y0, x1, y1 in rects:
        pts = np.array([[x0, y0, 1], [x1, y0, 1], [x1, y1, 1], [x0, y1, 1]]).T
        p = Hm @ pts
        p = p[:2] / p[2]
        xs = sorted(p[0]); ys = sorted(p[1])
        # Average the two inner/outer extents: the mapped quad is slightly
        # skewed, and the mean edge is what the keycap's edge really is.
        out.append([(xs[0] + xs[1]) / 2, (ys[0] + ys[1]) / 2, (xs[2] + xs[3]) / 2, (ys[2] + ys[3]) / 2])
    return out

def draw(path, keys, out):
    im = Image.open(path).convert('RGB')
    d = ImageDraw.Draw(im)
    for k in keys:
        x0, y0, x1, y1 = k['rect']
        d.rectangle([x0, y0, x1, y1], outline=(255, 0, 0))
        d.text((x0 + 2, y0 + 1), k['label'][:6], fill=(255, 0, 255))
    im.save(out)
