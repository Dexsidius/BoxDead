#!/usr/bin/env python3
"""Build BoxDead's scene tilesets and maps.

Every pixel here is generated from code -- there is no source artwork, no
tracing and no recolouring of third-party assets, so the output carries no
licence beyond this repository's own and can be redistributed freely.

For each scene this emits:

    assets/maps/<Level>/<Level>.mx          (LevelEdit++ ".mx" JSON)
    assets/maps/<Level>/assets/<Tile>.bmp   (32x32, or 64x64 for big props)

Art techniques
--------------
Floors have to tile seamlessly, so every generator works on a torus: noise is
sampled with wrapped coordinates, and cobbles are a Voronoi diagram using
toroidal distance, so a stone crossing the right edge reappears on the left.
Props do not tile -- they are drawn once and composited over the scene's own
floor tile, then saved opaque, because SDL_LoadBMP has no reliable alpha path
and baking guarantees a prop sits on ground matching its scene.

Tile names drive behaviour in-game (see src/tilemap.cpp): a name containing
wall/block/rock/stone/crate/pillar/obstacle/... is solid, one containing
barrel/drum/explosive/tnt becomes a destructible Barrel entity, anything else
is walkable decoration.

Paths inside the .mx use LevelEdit++'s own convention
("exports/<Level>/assets/<Tile>.bmp") so a scene folder can be dropped into the
editor's `exports/` directory and opened as-is; BoxDead falls back to resolving
the basename next to the .mx, so one file works in both.

Usage:
    python tools/build_levels.py
"""
import json
import math
import os
import random

from PIL import Image, ImageDraw

TILE = 32


# --------------------------------------------------------------------------
# colour helpers
# --------------------------------------------------------------------------
def clamp8(v):
    return max(0, min(255, int(v)))


def shade(rgb, f):
    return tuple(clamp8(c * f) for c in rgb)


def mix(a, b, t):
    return tuple(clamp8(a[i] + (b[i] - a[i]) * t) for i in range(3))


# --------------------------------------------------------------------------
# wrapped value noise -- seamless because the lattice wraps modulo `period`
# --------------------------------------------------------------------------
def value_noise(size, period, rng):
    """size x size float grid in [0,1], tiling seamlessly at `size`."""
    lat = [[rng.random() for _ in range(period)] for _ in range(period)]
    step = size / period

    def smooth(t):
        return t * t * (3 - 2 * t)

    out = [[0.0] * size for _ in range(size)]
    for y in range(size):
        gy = y / step
        y0 = int(gy) % period
        y1 = (y0 + 1) % period
        fy = smooth(gy - int(gy))
        for x in range(size):
            gx = x / step
            x0 = int(gx) % period
            x1 = (x0 + 1) % period
            fx = smooth(gx - int(gx))
            top = lat[y0][x0] + (lat[y0][x1] - lat[y0][x0]) * fx
            bot = lat[y1][x0] + (lat[y1][x1] - lat[y1][x0]) * fx
            out[y][x] = top + (bot - top) * fy
    return out


def fbm(size, rng, octaves=(2, 4, 8), weights=(0.55, 0.3, 0.15)):
    layers = [value_noise(size, p, rng) for p in octaves]
    return [[sum(w * l[y][x] for w, l in zip(weights, layers))
             for x in range(size)] for y in range(size)]


# --------------------------------------------------------------------------
# floor / wall generators (seamless)
# --------------------------------------------------------------------------
def cobble_tile(rng, base, mortar, cells=5, size=TILE, jitter=0.55):
    """Irregular cobbles: a toroidal Voronoi diagram, mortar along the seams."""
    step = size / cells
    seeds = []
    for cy in range(cells):
        for cx in range(cells):
            sx = (cx + 0.5 + rng.uniform(-jitter, jitter)) * step
            sy = (cy + 0.5 + rng.uniform(-jitter, jitter)) * step
            seeds.append((sx % size, sy % size, rng.uniform(0.82, 1.18)))

    grain = fbm(size, rng)
    img = Image.new('RGBA', (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            best = best2 = 1e9
            tone = 1.0
            for sx, sy, t in seeds:
                dx = abs(x - sx)
                dy = abs(y - sy)
                dx = min(dx, size - dx)   # wrap: shortest way round the torus
                dy = min(dy, size - dy)
                d = dx * dx + dy * dy
                if d < best:
                    best2 = best
                    best = d
                    tone = t
                elif d < best2:
                    best2 = d
            edge = math.sqrt(best2) - math.sqrt(best)  # distance to the seam
            col = shade(base, tone * (0.9 + 0.2 * grain[y][x]))
            if edge < 1.6:
                col = mix(mortar, col, max(0.0, edge / 1.6) * 0.7)
            else:
                # soft highlight just inside each stone
                col = mix(col, shade(col, 1.18), max(0.0, 1.0 - edge / 6.0) * 0.35)
            px[x, y] = col + (255,)
    return img


def flagstone_tile(rng, base, grout, slabs=2, size=TILE):
    """Big rectangular slabs with grout lines and per-slab tone."""
    grain = fbm(size, rng)
    img = Image.new('RGBA', (size, size))
    px = img.load()
    step = size // slabs
    tones = [[rng.uniform(0.88, 1.12) for _ in range(slabs)] for _ in range(slabs)]
    for y in range(size):
        for x in range(size):
            cx, cy = x // step, y // step
            ix, iy = x % step, y % step
            col = shade(base, tones[cy][cx] * (0.92 + 0.16 * grain[y][x]))
            if ix < 2 or iy < 2:
                col = mix(grout, col, 0.25)
            elif ix < 4 or iy < 4:
                col = shade(col, 1.1)      # lit inner bevel
            elif ix >= step - 3 or iy >= step - 3:
                col = shade(col, 0.88)     # shadowed far edge
            px[x, y] = col + (255,)
    return img


def brick_tile(rng, base, mortar, rows=4, size=TILE):
    """Offset brick courses -- the wall workhorse."""
    grain = fbm(size, rng)
    img = Image.new('RGBA', (size, size))
    px = img.load()
    rh = size // rows
    bw = size // 2
    tones = {}
    for y in range(size):
        row = y // rh
        offset = (row % 2) * (bw // 2)
        for x in range(size):
            bx = (x + offset) % size
            key = (row, bx // bw)
            if key not in tones:
                tones[key] = rng.uniform(0.85, 1.15)
            col = shade(base, tones[key] * (0.9 + 0.2 * grain[y][x]))
            iy = y % rh
            ix = bx % bw
            if iy < 2 or ix < 2:
                col = mix(mortar, col, 0.2)
            elif iy < 3:
                col = shade(col, 1.16)
            elif iy >= rh - 2:
                col = shade(col, 0.84)
            px[x, y] = col + (255,)
    return img


def rough_tile(rng, base, size=TILE):
    """Plain stone with a bit of grain."""
    grain = fbm(size, rng, octaves=(4, 8, 16))
    img = Image.new('RGBA', (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            px[x, y] = shade(base, 0.86 + 0.28 * grain[y][x]) + (255,)
    return img


# --------------------------------------------------------------------------
# prop generators (drawn once, baked over the floor)
# --------------------------------------------------------------------------
def _blob(draw, cx, cy, rx, ry, col, rng, lumps=7):
    """An irregular rounded mass, built from overlapping ellipses."""
    draw.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=col)
    for _ in range(lumps):
        a = rng.uniform(0, math.pi * 2)
        d = rng.uniform(0.25, 0.7)
        lx = cx + math.cos(a) * rx * d
        ly = cy + math.sin(a) * ry * d
        lr = rng.uniform(0.3, 0.55) * min(rx, ry)
        draw.ellipse([lx - lr, ly - lr, lx + lr, ly + lr], fill=col)


def rock_prop(rng, base, size=TILE):
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = size / 2, size * 0.58
    rx, ry = size * 0.36, size * 0.30
    d.ellipse([cx - rx * 1.1, size * 0.80, cx + rx * 1.1, size * 0.96],
              fill=(0, 0, 0, 90))                       # ground shadow
    _blob(d, cx, cy, rx, ry, shade(base, 0.72) + (255,), rng)
    _blob(d, cx, cy - ry * 0.18, rx * 0.82, ry * 0.72, base + (255,), rng)
    _blob(d, cx - rx * 0.2, cy - ry * 0.42, rx * 0.45, ry * 0.34,
          shade(base, 1.28) + (255,), rng, lumps=4)     # lit crown
    for _ in range(3):                                   # cracks
        x0 = rng.uniform(cx - rx * 0.5, cx + rx * 0.5)
        y0 = rng.uniform(cy - ry * 0.3, cy + ry * 0.4)
        d.line([x0, y0, x0 + rng.uniform(-4, 4), y0 + rng.uniform(2, 6)],
               fill=shade(base, 0.5) + (255,), width=1)
    return im


def headstone_prop(rng, base, size=TILE):
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w, h = size * 0.46, size * 0.62
    x0, y0 = (size - w) / 2, size * 0.28
    lean = rng.uniform(-1.5, 1.5)
    d.ellipse([x0 - 4, size * 0.84, x0 + w + 4, size * 0.97], fill=(0, 0, 0, 95))
    d.rounded_rectangle([x0 + lean, y0, x0 + w + lean, y0 + h],
                        radius=int(w * 0.45), fill=shade(base, 0.82) + (255,))
    d.rounded_rectangle([x0 + 2 + lean, y0 + 2, x0 + w - 3 + lean, y0 + h],
                        radius=int(w * 0.4), fill=base + (255,))
    d.line([x0 + 5 + lean, y0 + h * 0.45, x0 + w - 6 + lean, y0 + h * 0.45],
           fill=shade(base, 0.6) + (255,), width=1)
    d.line([x0 + 6 + lean, y0 + h * 0.62, x0 + w - 8 + lean, y0 + h * 0.62],
           fill=shade(base, 0.6) + (255,), width=1)
    return im


def bush_prop(rng, base, size=TILE, berries=None):
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = size / 2, size * 0.60
    d.ellipse([cx - size * 0.34, size * 0.82, cx + size * 0.34, size * 0.96],
              fill=(0, 0, 0, 80))
    _blob(d, cx, cy + 2, size * 0.36, size * 0.28, shade(base, 0.62) + (255,), rng, 8)
    _blob(d, cx, cy, size * 0.33, size * 0.26, base + (255,), rng, 8)
    _blob(d, cx - 2, cy - 3, size * 0.22, size * 0.16,
          shade(base, 1.25) + (255,), rng, 5)
    if berries:
        for _ in range(rng.randint(3, 6)):
            bx = cx + rng.uniform(-size * 0.26, size * 0.26)
            by = cy + rng.uniform(-size * 0.16, size * 0.16)
            d.ellipse([bx - 1.4, by - 1.4, bx + 1.4, by + 1.4], fill=berries + (255,))
    return im


def tree_prop(rng, trunk, leaf, size=TILE * 2, dead=False):
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx = size / 2
    d.ellipse([cx - size * 0.30, size * 0.80, cx + size * 0.30, size * 0.96],
              fill=(0, 0, 0, 95))
    if not dead:
        tw = size * 0.10
        d.rectangle([cx - tw, size * 0.44, cx + tw, size * 0.90],
                    fill=shade(trunk, 0.8) + (255,))
        d.rectangle([cx - tw, size * 0.44, cx + tw * 0.2, size * 0.90],
                    fill=trunk + (255,))
    if dead:
        # Recursive bare branching: each limb forks into two thinner ones, so
        # the silhouette reads as a dead tree rather than a stick.
        def limb(x, y, angle, length, width, depth):
            ex = x + math.cos(angle) * length
            ey = y - math.sin(angle) * length
            d.line([x, y, ex, ey], fill=shade(trunk, 0.85 + 0.05 * depth) + (255,),
                   width=max(1, int(width)))
            if depth <= 0:
                return
            for turn in (rng.uniform(0.4, 0.75), -rng.uniform(0.4, 0.75)):
                limb(ex, ey, angle + turn, length * rng.uniform(0.58, 0.72),
                     width * 0.62, depth - 1)

        crown = size * 0.44
        d.line([cx, size * 0.90, cx, crown], fill=trunk + (255,),
               width=max(3, int(size * 0.10)))
        for a in (math.pi * 0.62, math.pi * 0.38, math.pi * 0.5):
            limb(cx, crown, a, size * rng.uniform(0.16, 0.22),
                 size * 0.055, 2)
    else:
        _blob(d, cx, size * 0.36, size * 0.36, size * 0.30,
              shade(leaf, 0.68) + (255,), rng, 9)
        _blob(d, cx, size * 0.33, size * 0.32, size * 0.26, leaf + (255,), rng, 9)
        _blob(d, cx - size * 0.06, size * 0.27, size * 0.20, size * 0.15,
              shade(leaf, 1.22) + (255,), rng, 6)
    return im


def mushroom_prop(rng, cap, stem, size=TILE):
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i, (ox, sc) in enumerate(((0, 1.0), (-7, 0.62), (6, 0.7))):
        cx = size / 2 + ox
        base_y = size * (0.86 - 0.04 * i)
        cw = size * 0.24 * sc
        d.rectangle([cx - cw * 0.30, base_y - size * 0.22 * sc, cx + cw * 0.30, base_y],
                    fill=stem + (255,))
        d.ellipse([cx - cw, base_y - size * 0.34 * sc, cx + cw,
                   base_y - size * 0.14 * sc], fill=cap + (255,))
        d.ellipse([cx - cw * 0.5, base_y - size * 0.32 * sc, cx - cw * 0.1,
                   base_y - size * 0.24 * sc], fill=shade(cap, 1.35) + (255,))
    return im


def barrel_prop(rng, body, band, size=TILE):
    im = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w, h = size * 0.52, size * 0.66
    x0, y0 = (size - w) / 2, size * 0.26
    d.ellipse([x0 - 3, size * 0.86, x0 + w + 3, size * 0.98], fill=(0, 0, 0, 95))
    d.rectangle([x0, y0, x0 + w, y0 + h], fill=body + (255,))
    d.rectangle([x0, y0, x0 + w * 0.34, y0 + h], fill=shade(body, 1.22) + (255,))
    d.rectangle([x0 + w * 0.82, y0, x0 + w, y0 + h], fill=shade(body, 0.72) + (255,))
    for v in (0.26, 0.66):
        d.rectangle([x0, y0 + h * v, x0 + w, y0 + h * v + 3], fill=band + (255,))
    d.ellipse([x0, y0 - 4, x0 + w, y0 + 5], fill=shade(body, 1.3) + (255,))
    return im


# --------------------------------------------------------------------------
# scenes
# --------------------------------------------------------------------------
def S(**kw):
    return kw


SCENES = [
    S(name='Courtyard', cols=100, rows=56,
      floor=('flagstone', (150, 152, 132), (92, 96, 82)),
      wall=('brick', (96, 104, 84), (48, 54, 42)),
      props=[('Mossy Rock', 'rock', (118, 132, 104)),
             ('Hedge Block', 'bush', (86, 130, 70)),
             ('Courtyard Tree Obstacle', 'tree', ((104, 74, 48), (92, 140, 74)))],
      decor=[('Grass Tuft', 'bush_small', (104, 148, 84))],
      barrel=((176, 62, 46), (86, 74, 62))),
    S(name='Asylum', cols=100, rows=56,
      floor=('cobble', (168, 168, 154), (104, 104, 96)),
      wall=('brick', (112, 110, 100), (56, 56, 50)),
      props=[('Rubble Rock', 'rock', (156, 154, 146)),
             ('Ward Pillar', 'headstone', (172, 172, 168))],
      decor=[('Fungal Bloom', 'mushroom', ((186, 176, 128), (150, 152, 128)))],
      barrel=((176, 62, 46), (86, 74, 62))),
    S(name='Sewers', cols=100, rows=56,
      floor=('cobble', (96, 124, 110), (44, 62, 56)),
      wall=('brick', (66, 92, 80), (30, 46, 40)),
      props=[('Slime Rock', 'rock', (92, 128, 100)),
             ('Sludge Crate', 'rock', (74, 106, 88))],
      decor=[('Algae', 'bush_small', (96, 150, 96))],
      barrel=((168, 74, 44), (74, 78, 66))),
    S(name='Graveyard', cols=100, rows=56,
      floor=('cobble', (118, 112, 136), (58, 54, 70)),
      wall=('brick', (80, 76, 98), (38, 36, 48)),
      props=[('Headstone', 'headstone', (168, 164, 180)),
             ('Dead Tree Obstacle', 'tree_dead', ((84, 74, 82), (84, 74, 82)))],
      decor=[('Withered Bush', 'bush_small', (108, 100, 88))],
      barrel=((166, 66, 52), (78, 70, 78))),
    S(name='Hells Gate', cols=100, rows=56,
      floor=('cobble', (150, 74, 58), (72, 32, 26)),
      wall=('brick', (104, 44, 36), (48, 20, 16)),
      props=[('Obsidian Rock', 'rock', (72, 52, 54)),
             ('Cinder Block', 'rock', (128, 62, 46))],
      decor=[('Ash Scrub', 'bush_small', (118, 74, 52))],
      barrel=((196, 92, 44), (92, 60, 44))),
    # Stress test: 3280 is not a multiple of the editor's 32px grid (102.5
    # tiles), so this rounds up to 103x64 = 3296x2048 and ~7000 placements.
    S(name='The Sprawl', cols=103, rows=64,
      floor=('cobble', (136, 136, 144), (66, 66, 74)),
      wall=('brick', (92, 94, 104), (44, 46, 52)),
      props=[('Sprawl Rock', 'rock', (150, 150, 156)),
             ('Ruin Pillar', 'headstone', (160, 160, 168)),
             ('Sprawl Tree Obstacle', 'tree_dead', ((96, 88, 80), (96, 88, 80)))],
      decor=[('Weeds', 'bush_small', (110, 128, 88))],
      barrel=((176, 62, 46), (86, 74, 62))),
]


def make_floor(kind, rng, base, accent):
    if kind == 'cobble':
        return cobble_tile(rng, base, accent)
    if kind == 'flagstone':
        return flagstone_tile(rng, base, accent)
    if kind == 'brick':
        return brick_tile(rng, base, accent)
    return rough_tile(rng, base)


def bake(art, floor, size):
    canvas = Image.new('RGBA', (size, size))
    for y in range(0, size, TILE):
        for x in range(0, size, TILE):
            canvas.paste(floor, (x, y))
    canvas.alpha_composite(art)
    return canvas


def make_prop(kind, rng, colour, floor, size=TILE):
    if kind == 'rock':
        art = rock_prop(rng, colour)
    elif kind == 'headstone':
        art = headstone_prop(rng, colour)
    elif kind == 'bush':
        art = bush_prop(rng, colour)
    elif kind == 'bush_small':
        small = bush_prop(rng, colour)
        small = small.resize((int(TILE * 0.72), int(TILE * 0.72)), Image.NEAREST)
        art = Image.new('RGBA', (TILE, TILE), (0, 0, 0, 0))
        art.alpha_composite(small, ((TILE - small.width) // 2,
                                    TILE - small.height - 2))
    elif kind == 'mushroom':
        art = mushroom_prop(rng, colour[0], colour[1])
    elif kind == 'tree':
        art = tree_prop(rng, colour[0], colour[1], size=size)
    elif kind == 'tree_dead':
        art = tree_prop(rng, colour[0], colour[1], size=size, dead=True)
    else:
        raise SystemExit('unknown prop kind ' + kind)
    return bake(art, floor, size)


def save_bmp(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.convert('RGB').save(path, format='BMP')


# --------------------------------------------------------------------------
# layout
# --------------------------------------------------------------------------
def build_layout(scene, rng):
    """{tile name: [[x, y, w, h], ...]} -- border, ruins, props, barrels."""
    cols, rows = scene['cols'], scene['rows']
    cx, cy = cols // 2, rows // 2
    occupied = set()
    walls, barrels = set(), set()
    props = {p[0]: [] for p in scene['props']}
    decor = {d[0]: [] for d in scene['decor']}

    def free(c, r, span=1):
        if not (2 <= c and c + span - 1 < cols - 2 and 2 <= r and r + span - 1 < rows - 2):
            return False
        if (c - cx) ** 2 + (r - cy) ** 2 < 36:  # keep the spawn area open
            return False
        return all((c + dc, r + dr) not in occupied
                   for dc in range(span) for dr in range(span))

    def take(c, r, span=1):
        for dc in range(span):
            for dr in range(span):
                occupied.add((c + dc, r + dr))

    # Ruin clusters: hollow rectangles read as broken rooms rather than blobs.
    for _ in range(int(cols * rows / 130)):
        c, r = rng.randrange(3, cols - 6), rng.randrange(3, rows - 6)
        w, h = rng.randint(2, 6), rng.randint(2, 5)
        if rng.random() < 0.5:
            w, h = h, w
        for dr in range(h):
            for dc in range(w):
                if (dc in (0, w - 1) or dr in (0, h - 1)) and free(c + dc, r + dr):
                    walls.add((c + dc, r + dr))
                    take(c + dc, r + dr)

    # Two-tile props first: they need the room.
    for name, kind, _col in [p for p in scene['props'] if p[1].startswith('tree')]:
        for _ in range(int(cols * rows / 220)):
            c, r = rng.randrange(3, cols - 4), rng.randrange(3, rows - 4)
            if free(c, r, 2):
                props[name].append([c * TILE, r * TILE, TILE * 2, TILE * 2])
                take(c, r, 2)

    # Single-tile solid props, in knots of 1-4.
    for name, kind, _col in [p for p in scene['props'] if not p[1].startswith('tree')]:
        for _ in range(int(cols * rows / 90)):
            c, r = rng.randrange(3, cols - 3), rng.randrange(3, rows - 3)
            for _ in range(rng.randint(1, 4)):
                nc, nr = c + rng.randint(-1, 1), r + rng.randint(-1, 1)
                if free(nc, nr):
                    props[name].append([nc * TILE, nr * TILE, TILE, TILE])
                    take(nc, nr)

    # Walkable decoration: not marked occupied, so the player walks over it.
    for name, _kind, _col in scene['decor']:
        for _ in range(int(cols * rows / 60)):
            c, r = rng.randrange(2, cols - 2), rng.randrange(2, rows - 2)
            if (c, r) not in occupied and (c - cx) ** 2 + (r - cy) ** 2 >= 25:
                decor[name].append([c * TILE, r * TILE, TILE, TILE])

    # Explosive barrels in clusters of 1-3 so one shot chains.
    for _ in range(int(cols * rows / 150)):
        c, r = rng.randrange(3, cols - 3), rng.randrange(3, rows - 3)
        if not free(c, r):
            continue
        barrels.add((c, r))
        take(c, r)
        for _ in range(rng.randint(0, 2)):
            nc, nr = c + rng.randint(-1, 1), r + rng.randint(-1, 1)
            if free(nc, nr):
                barrels.add((nc, nr))
                take(nc, nr)

    for c in range(cols):
        walls.add((c, 0))
        walls.add((c, rows - 1))
    for r in range(rows):
        walls.add((0, r))
        walls.add((cols - 1, r))

    out = {
        'Ground': [[c * TILE, r * TILE, TILE, TILE]
                   for r in range(rows) for c in range(cols) if (c, r) not in walls],
        'Wall': sorted([c * TILE, r * TILE, TILE, TILE] for c, r in walls),
        'Barrel': sorted([c * TILE, r * TILE, TILE, TILE] for c, r in barrels),
    }
    out.update({k: v for k, v in props.items() if v})
    out.update({k: v for k, v in decor.items() if v})
    return out


def main():
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo)
    print('Generating BoxDead scene art and maps (no external assets)...')

    for scene in SCENES:
        name = scene['name']
        out_dir = os.path.join('assets', 'maps', name)
        asset_dir = os.path.join(out_dir, 'assets')
        os.makedirs(asset_dir, exist_ok=True)

        # One seed per scene keeps regeneration byte-for-byte reproducible.
        seed = sum(ord(ch) * (i + 1) for i, ch in enumerate(name))

        fk, fbase, faccent = scene['floor']
        wk, wbase, waccent = scene['wall']
        floor = make_floor(fk, random.Random(seed + 1), fbase, faccent)
        wall = make_floor(wk, random.Random(seed + 2), wbase, waccent)
        save_bmp(floor, os.path.join(asset_dir, 'Ground.bmp'))
        save_bmp(wall, os.path.join(asset_dir, 'Wall.bmp'))

        for i, (tname, kind, colour) in enumerate(scene['props'] + scene['decor']):
            size = TILE * 2 if kind.startswith('tree') else TILE
            img = make_prop(kind, random.Random(seed + 10 + i), colour, floor, size)
            save_bmp(img, os.path.join(asset_dir, tname + '.bmp'))

        # Barrels are drawn as 3D props in-game; this image is only so the
        # Barrel layer shows something recognisable in the editor.
        bbody, bband = scene['barrel']
        save_bmp(bake(barrel_prop(random.Random(seed + 99), bbody, bband), floor, TILE),
                 os.path.join(asset_dir, 'Barrel.bmp'))

        layers = build_layout(scene, random.Random(seed))
        doc = {
            'name': name,
            'tiles': {
                tname: {'filepath': f'exports/{name}/assets/{tname}.bmp',
                        'locations': locs}
                for tname, locs in layers.items()
            },
        }
        with open(os.path.join(out_dir, name + '.mx'), 'w') as f:
            json.dump(doc, f, indent=4)

        total = sum(len(v) for v in layers.values())
        print(f"  {name:12s} {scene['cols']}x{scene['rows']} tiles "
              f"({scene['cols'] * TILE}x{scene['rows'] * TILE} px)  "
              f"{total} placements across {len(layers)} layers")
    print('Done.')


if __name__ == '__main__':
    main()
