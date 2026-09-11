#!/usr/bin/env python3
"""Turn the Blender bullet render into the game's sprite.

    blender --background --factory-startup --python tools/blender_bullet.py
    python tools/build_bullet.py

Takes tools/_bullet_render.png (transparent background, round pointing +X),
crops it to the art, scales it down to sprite size, and adds a dark rim.

The rim is the point. A brass round on the Asylum's pale cobble or the
Courtyard's flagstone is low-contrast whichever way you shade it, so the sprite
carries its own outline: a dark halo one pixel proud of the silhouette reads
against light *and* dark ground, which is the same trick the characters use.

Writes a 32-bit BMP - SDL_LoadBMP keeps the alpha channel, so no SDL_image
dependency is needed.
"""
import os

from PIL import Image

# On-screen size. Long enough to read as a round in flight without becoming a
# dash; the game rotates it to the direction of travel.
WIDTH = 22
RIM = (18, 14, 16, 255)   # near-black, slightly warm


def add_rim(sprite):
    """Dilate the silhouette by a pixel and paint that ring dark."""
    w, h = sprite.width, sprite.height
    out = Image.new('RGBA', (w + 2, h + 2), (0, 0, 0, 0))
    alpha = sprite.split()[-1]

    # Paint the halo first: the silhouette offset in all 8 directions.
    halo = Image.new('RGBA', out.size, (0, 0, 0, 0))
    solid = Image.new('RGBA', sprite.size, RIM)
    for dx in (0, 1, 2):
        for dy in (0, 1, 2):
            halo.paste(solid, (dx, dy), alpha)
    out.alpha_composite(halo)
    # Then the bullet itself, centred inside its halo.
    out.alpha_composite(sprite, (1, 1))
    return out


def main():
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo)
    src = os.path.join('tools', '_bullet_render.png')
    if not os.path.exists(src):
        raise SystemExit(f"{src} not found - run the Blender step first:\n"
                         f"  blender --background --factory-startup "
                         f"--python tools/blender_bullet.py")

    im = Image.open(src).convert('RGBA')
    box = im.getbbox()
    if box:
        im = im.crop(box)

    height = max(1, round(WIDTH * im.height / im.width))
    # LANCZOS keeps the shading readable at this size; the alpha edge is
    # thresholded afterwards so the rim has something crisp to hug.
    small = im.resize((WIDTH, height), Image.LANCZOS)
    r, g, b, a = small.split()
    a = a.point(lambda v: 0 if v < 90 else 255)
    small = Image.merge('RGBA', (r, g, b, a))

    sprite = add_rim(small)
    out = os.path.join('assets', 'sprites', 'bullet.bmp')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sprite.save(out, format='BMP')
    print(f"  {out}  {sprite.width}x{sprite.height} (32-bit, alpha kept)")

    # A magnified copy so the sprite can actually be looked at.
    sprite.resize((sprite.width * 12, sprite.height * 12),
                  Image.NEAREST).save(os.path.join('tools', '_bullet_x12.png'))
    print('  tools/_bullet_x12.png (preview)')


if __name__ == '__main__':
    main()
