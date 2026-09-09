#!/usr/bin/env python3
"""Generate BoxDead scene tilesets in the LevelEdit++ ".mx" format.

For each scene we emit:
  assets/maps/<level>/<level>.mx        (JSON, editor's exact ExportMX schema)
  assets/maps/<level>/assets/<Tile>.bmp (32x32 tile images)

Each .mx lists tiles (Ground/Wall/Block) with a "filepath" (relative to the
.mx dir) and "locations" as [x, y, w, h] world-pixel placements on a 40x22
grid (1280x704). Walls form the border (solid); Blocks are scattered obstacles
(solid); Ground is the walkable floor.
"""
import json
import os
import struct

TILE = 32
# Larger than the viewport so the camera scrolls. 80x44 = 2560 x 1408
# (the 1280x720 view shows about half the map at once).
COLS, ROWS = 80, 44

LEVELS = [
    {"name": "Courtyard",   "ground": (40, 44, 54),   "wall": (74, 80, 96),   "block": (96, 106, 126)},
    {"name": "Asylum",      "ground": (58, 40, 40),   "wall": (94, 52, 52),   "block": (116, 64, 64)},
    {"name": "Sewers",      "ground": (36, 54, 48),   "wall": (54, 74, 64),   "block": (74, 94, 80)},
    {"name": "Graveyard",   "ground": (48, 46, 64),   "wall": (74, 72, 94),   "block": (96, 94, 116)},
    {"name": "Hells Gate",  "ground": (70, 30, 30),   "wall": (104, 42, 42),   "block": (134, 54, 54)},
]


def write_bmp(path, base, highlight):
    """Write a 32x32 24-bit BMP with a base fill + a lighter top edge."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    px = []
    for y in range(TILE):
        row = bytearray()
        for x in range(TILE):
            r, g, b = base
            # top-left highlight for a faux 3D bevel
            if y == 0 or x == 0:
                r, g, b = highlight
            elif y == TILE - 1 or x == TILE - 1:
                r = max(0, r - 24); g = max(0, g - 24); b = max(0, b - 24)
            # subtle checker for ground texture
            row += bytes((b, g, r))
        px.append(bytes(row))
    pixel_data = b"".join(reversed(px))  # BMP is bottom-up
    row_size = TILE * 3
    pixel_size = row_size * TILE
    file_header = struct.pack(b"<2sIHHI", b"BM", 14 + 40 + pixel_size, 0, 0, 14 + 40)
    info_header = struct.pack(
        b"<IIIHHIIIIII", 40, TILE, TILE, 1, 24, 0, pixel_size, 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(file_header)
        f.write(info_header)
        f.write(pixel_data)


def build_level(level):
    name = level["name"]
    out_dir = os.path.join("assets", "maps", name)
    asset_dir = os.path.join(out_dir, "assets")
    os.makedirs(asset_dir, exist_ok=True)

    write_bmp(os.path.join(asset_dir, "Ground.bmp"), level["ground"], tuple(min(255, c + 28) for c in level["ground"]))
    write_bmp(os.path.join(asset_dir, "Wall.bmp"), level["wall"], tuple(min(255, c + 30) for c in level["wall"]))
    write_bmp(os.path.join(asset_dir, "Block.bmp"), level["block"], tuple(min(255, c + 30) for c in level["block"]))

    ground_locs, wall_locs, block_locs = [], [], []

    # Scatter interior Block obstacles in a loose symmetric pattern that
    # scales with the grid and leaves the centre spawn area open.
    import math
    obstacles = set()
    cx, cy = COLS // 2, ROWS // 2
    # ring of blocks around the centre, plus corner clusters
    for r in range(4, ROWS - 4, 6):
        for c in range(4, COLS - 4, 6):
            if (c - cx) ** 2 + (r - cy) ** 2 < 36:  # keep centre clear
                continue
            obstacles.add((c, r))

    for row in range(ROWS):
        for col in range(COLS):
            x, y = col * TILE, row * TILE
            border = (col == 0 or col == COLS - 1 or row == 0 or row == ROWS - 1)
            if border:
                wall_locs.append([x, y, TILE, TILE])
            elif (col, row) in obstacles:
                block_locs.append([x, y, TILE, TILE])
            else:
                ground_locs.append([x, y, TILE, TILE])

    doc = {
        "name": name,
        "tiles": {
            "Ground": {"filepath": "assets/Ground.bmp", "locations": ground_locs},
            "Wall": {"filepath": "assets/Wall.bmp", "locations": wall_locs},
            "Block": {"filepath": "assets/Block.bmp", "locations": block_locs},
        },
    }
    with open(os.path.join(out_dir, name + ".mx"), "w") as f:
        json.dump(doc, f, indent=4)
    print(f"  {name}: {len(ground_locs)} ground + {len(wall_locs)} wall + {len(block_locs)} block tiles")


def main():
    root = os.path.dirname(os.path.abspath(__file__))
    # Repo root is the parent of the tools/ dir.
    os.chdir(os.path.dirname(root))
    print("Generating BoxDead scene tilesets (.mx)...")
    for level in LEVELS:
        build_level(level)
    print("Done.")


if __name__ == "__main__":
    main()
