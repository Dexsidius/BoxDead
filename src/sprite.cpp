// BoxDead - Sprite implementation
#include "boxdead/sprite.hpp"
#include "boxdead/weapon.hpp"

#include <algorithm>
#include <vector>

namespace bd {

void draw_sprite(SDL_Renderer* r, const Sprite& s, float cx, float cy, float w,
                 float h) {
    const SDL_FRect dst{cx - w * 0.5f, cy - h * 0.5f, w, h};
    if (s.texture && s.texture->get()) {
        SDL_FRect src = s.src;
        if (src.w == 0.0f || src.h == 0.0f) {
            src = SDL_FRect{0.0f, 0.0f, s.texture->w(), s.texture->h()};
        }
        // Tint the texture by the sprite color (white = no change).
        SDL_SetTextureColorMod(s.texture->get(), s.color.r, s.color.g,
                               s.color.b);
        SDL_RenderTexture(r, s.texture->get(), &src, &dst);
    } else {
        SDL_SetRenderDrawColor(r, s.color.r, s.color.g, s.color.b, s.color.a);
        SDL_RenderFillRect(r, &dst);
    }
}

namespace {
inline Uint32 pack_color(SDL_Color c) {
    return static_cast<Uint32>(c.r) | (static_cast<Uint32>(c.g) << 8) |
           (static_cast<Uint32>(c.b) << 16) |
           (static_cast<Uint32>(c.a) << 24);
}
}  // namespace

std::unique_ptr<Texture> make_solid_sprite_texture(SDL_Renderer* r,
                                                   SDL_Color base,
                                                   int size) {
    std::vector<Uint32> pixels(static_cast<size_t>(size) * size);
    const SDL_Color dark{static_cast<Uint8>(base.r * 0.55f),
                         static_cast<Uint8>(base.g * 0.55f),
                         static_cast<Uint8>(base.b * 0.55f), base.a};
    const int b = 4;  // border thickness
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool border = x < b || x >= size - b || y < b ||
                                y >= size - b;
            pixels[static_cast<size_t>(y) * size + x] =
                pack_color(border ? dark : base);
        }
    }
    return Texture::from_pixels(r, pixels.data(), size, size);
}

std::unique_ptr<Texture> make_cross_sprite_texture(SDL_Renderer* r,
                                                   SDL_Color base,
                                                   SDL_Color cross,
                                                   int size) {
    std::vector<Uint32> pixels(static_cast<size_t>(size) * size);
    const SDL_Color dark{static_cast<Uint8>(base.r * 0.55f),
                         static_cast<Uint8>(base.g * 0.55f),
                         static_cast<Uint8>(base.b * 0.55f), base.a};
    const int b = 3;                 // border thickness
    const int t = 3;                 // cross thickness
    const int c = size / 2;          // center
    const int lo = b + 1;            // inner padding
    const int hi = size - b - 1;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool border = x < b || x >= size - b || y < b ||
                                y >= size - b;
            const bool on_cross =
                ((y >= c - t / 2 && y < c + (t + 1) / 2) ||
                 (x >= c - t / 2 && x < c + (t + 1) / 2)) &&
                x >= lo && x < hi && y >= lo && y < hi;
            pixels[static_cast<size_t>(y) * size + x] =
                pack_color(border ? dark
                                  : (on_cross ? cross : base));
        }
    }
    return Texture::from_pixels(r, pixels.data(), size, size);
}

std::unique_ptr<Texture> make_walk_sheet_texture(SDL_Renderer* r,
                                                 SDL_Color base,
                                                 int frame_count,
                                                 int frame_size,
                                                 bool moving) {
    const int sheet_w = frame_count * frame_size;
    const int sheet_h = frame_size;
    std::vector<Uint32> pixels(static_cast<size_t>(sheet_w) * sheet_h, 0);

    const SDL_Color dark{static_cast<Uint8>(base.r * 0.5f),
                         static_cast<Uint8>(base.g * 0.5f),
                         static_cast<Uint8>(base.b * 0.5f), 255};
    const SDL_Color leg{static_cast<Uint8>(base.r * 0.65f),
                        static_cast<Uint8>(base.g * 0.65f),
                        static_cast<Uint8>(base.b * 0.65f), 255};

    const int b = std::max(2, frame_size / 12);  // border thickness
    const int body_top = b;
    const int body_h = frame_size * 7 / 10;    // body occupies the top 70%
    const int leg_top = body_h;
    const int leg_region = frame_size - leg_top - b;
    const int leg_w = std::max(3, frame_size / 7);
    const int leg_x[2] = {frame_size * 3 / 10 - leg_w / 2,
                          frame_size * 7 / 10 - leg_w / 2};

    auto fill_rect = [&](int ox, int x0, int y0, int w0, int h0,
                         Uint32 col) {
        for (int yy = 0; yy < h0; ++yy) {
            for (int xx = 0; xx < w0; ++xx) {
                const int px = ox + x0 + xx;
                const int py = y0 + yy;
                if (px >= 0 && px < sheet_w && py >= 0 && py < sheet_h)
                    pixels[static_cast<size_t>(py) * sheet_w + px] = col;
            }
        }
    };

    for (int f = 0; f < frame_count; ++f) {
        const int ox = f * frame_size;  // frame origin x
        const int bob = moving ? 0 : ((f % 2 == 0) ? 0 : 1);

        // Body: dark border + base fill, centered, with a small bob.
        fill_rect(ox, b, body_top + bob, frame_size - 2 * b, body_h - 2 * b,
                  pack_color(dark));
        fill_rect(ox, b + 1, body_top + bob + 1, frame_size - 2 * b - 2,
                  body_h - 2 * b - 2, pack_color(base));

        // Two legs; heights alternate per frame when walking.
        for (int side = 0; side < 2; ++side) {
            int lh = leg_region;
            if (moving) {
                const bool forward = (f % 2 == 0) == (side == 0);
                lh = leg_region * (forward ? 1.0f : 0.55f);
            }
            fill_rect(ox, leg_x[side], leg_top + (leg_region - lh), leg_w, lh,
                      pack_color(leg));
        }
    }

    return Texture::from_pixels(r, pixels.data(), sheet_w, sheet_h);
}

namespace {
// Darken a color for borders/outlines.
inline SDL_Color shade(SDL_Color c, float f) {
    return SDL_Color{static_cast<Uint8>(c.r * f), static_cast<Uint8>(c.g * f),
                     static_cast<Uint8>(c.b * f), c.a};
}
inline Uint32 pack(SDL_Color c) { return pack_color(c); }
}  // namespace

std::unique_ptr<Texture> make_creature_sheet_texture(SDL_Renderer* r,
                                                   const CreaturePalette& pal,
                                                   int frame_count,
                                                   int frame_size,
                                                   bool moving) {
    const int sheet_w = frame_count * frame_size;
    const int sheet_h = frame_size;
    std::vector<Uint32> pixels(static_cast<size_t>(sheet_w) * sheet_h, 0);

    const int b = 1;                              // outline thickness
    // Horns sit above the head, so leave room at the top when present.
    const int head_top = (pal.horn.a != 0) ? 8 : 2;
    const int head_bottom = frame_size * 2 / 5;   // head occupies top ~40%
    const int torso_bottom = frame_size * 18 / 25;
    const int leg_top = torso_bottom;
    const int leg_region = frame_size - leg_top - b;
    const int body_x = frame_size * 3 / 20;       // left margin
    const int body_w = frame_size * 7 / 10;       // body width
    const int center = frame_size / 2;

    auto fill_rect = [&](int ox, int x0, int y0, int w0, int h0, Uint32 col) {
        for (int yy = 0; yy < h0; ++yy)
            for (int xx = 0; xx < w0; ++xx) {
                const int px = ox + x0 + xx;
                const int py = y0 + yy;
                if (px >= 0 && px < sheet_w && py >= 0 && py < sheet_h)
                    pixels[static_cast<size_t>(py) * sheet_w + px] = col;
            }
    };

    for (int f = 0; f < frame_count; ++f) {
        const int ox = f * frame_size;
        const int bob = moving ? 0 : ((f % 2 == 0) ? 0 : 1);

        // --- Head: outline + hair band + face ---
        const SDL_Color face_dark = shade(pal.skin, 0.55f);
        fill_rect(ox, body_x, head_top + bob, body_w, head_bottom - head_top,
                  pack(face_dark));
        if (pal.hair.a != 0) {
            const int hair_h = (head_bottom - head_top) / 3 + 1;
            fill_rect(ox, body_x + b, head_top + bob + b, body_w - 2 * b, hair_h,
                      pack(pal.hair));
            fill_rect(ox, body_x + b, head_top + bob + b + hair_h,
                      body_w - 2 * b, head_bottom - head_top - b - hair_h,
                      pack(pal.skin));
        } else {
            fill_rect(ox, body_x + b, head_top + bob + b, body_w - 2 * b,
                      head_bottom - head_top - 2 * b, pack(pal.skin));
        }

        // --- Horns (stepped triangles at the head top) ---
        if (pal.horn.a != 0) {
            const int horn_h = 6;
            const int horn_bases[2] = {body_x + body_w / 4,
                                       body_x + 3 * body_w / 4};
            for (int side = 0; side < 2; ++side) {
                for (int row = 0; row < horn_h; ++row) {
                    const int w = row + 1;
                    const int yx = head_top + bob - horn_h + row;
                    fill_rect(ox, horn_bases[side] - w / 2, yx, w, 1,
                              pack(pal.horn));
                }
            }
        }

        // --- Torso: outline + shirt + tie + blood ---
        const SDL_Color torso_dark = shade(pal.torso, 0.5f);
        fill_rect(ox, body_x, torso_bottom > head_top ? head_bottom : head_top,
                  body_w, torso_bottom - head_bottom, pack(torso_dark));
        fill_rect(ox, body_x + b, head_bottom + b, body_w - 2 * b,
                  torso_bottom - head_bottom - 2 * b, pack(pal.torso));
        if (pal.tie.a != 0) {
            fill_rect(ox, center - 1, head_bottom + b, 3,
                      torso_bottom - head_bottom - 2 * b, pack(pal.tie));
        }
        if (pal.blood) {
            // Deterministic blood splatter on the torso.
            const int sx[4] = {body_x + 2, body_x + body_w - 5,
                               body_x + 3, body_x + body_w - 4};
            const int sy[4] = {head_bottom + 1, head_bottom + 2,
                               head_bottom + 4, head_bottom + 5};
            for (int i = 0; i < 4; ++i)
                fill_rect(ox, sx[i], sy[i], 2, 2, pack(pal.tie));
        }

        // --- Legs (anchored at the hip; 4-phase walk so each frame differs) ---
        const int leg_w = std::max(3, body_w / 5);
        const int leg_x[2] = {body_x + body_w / 4 - leg_w / 2,
                              body_x + 3 * body_w / 4 - leg_w / 2};
        static constexpr float kPhase[4] = {1.0f, 0.7f, 0.4f, 0.85f};
        for (int side = 0; side < 2; ++side) {
            int lh = leg_region;
            if (moving) {
                const float factor = kPhase[(f + (side == 0 ? 0 : 2)) % 4];
                lh = static_cast<int>(leg_region * factor);
            }
            fill_rect(ox, leg_x[side], leg_top, leg_w, lh, pack(pal.pants));
        }
    }

    return Texture::from_pixels(r, pixels.data(), sheet_w, sheet_h);
}

// --- Procedural gun sprites (side view, muzzle pointing +x / right) ---------
// Drawn into an RGBA pixel buffer. The grip sits at the lower-left so the
// rotation pivot (the hand) is stable; the muzzle is at the right edge.
namespace {
struct GunCanvas {
    int w, h;
    std::vector<Uint32> px;
    explicit GunCanvas(int ww, int hh) : w(ww), h(hh), px(size_t(ww) * hh, 0) {}
    void fill(int x0, int y0, int x1, int y1, Uint32 c) {
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
        if (x1 > w) x1 = w;
        if (y1 > h) y1 = h;
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
                px[size_t(y) * w + x] = c;
    }
    void dot(int x, int y, Uint32 c) {
        if (x >= 0 && x < w && y >= 0 && y < h) px[size_t(y) * w + x] = c;
    }
};
inline Uint32 gun_pack(Uint8 r, Uint8 g, Uint8 b) {
    return Uint32(r) | (Uint32(g) << 8) | (Uint32(b) << 16) | (Uint32(255) << 24);
}
}  // namespace

std::unique_ptr<Texture> make_gun_texture(SDL_Renderer* r, WeaponKind k) {
    // Metal shades + a wooden grip.
    const Uint32 dark = gun_pack(60, 60, 70);
    const Uint32 mid = gun_pack(110, 110, 122);
    const Uint32 light = gun_pack(165, 165, 178);
    const Uint32 wood = gun_pack(96, 64, 40);
    const Uint32 hl = gun_pack(205, 205, 215);

    switch (k) {
        case WeaponKind::Shotgun: {
            GunCanvas c(36, 14);
            // double barrel (stacked) running to the muzzle
            c.fill(8, 4, 34, 7, light);   // top barrel
            c.fill(8, 7, 34, 10, mid);    // bottom barrel
            c.fill(32, 4, 34, 10, dark);  // muzzle cap
            // pump / foregrip under the barrel
            c.fill(12, 10, 18, 14, wood);
            // receiver + stock toward the grip
            c.fill(2, 5, 10, 9, mid);
            c.fill(0, 5, 4, 9, dark);    // stock back
            // grip
            c.fill(3, 9, 7, 14, wood);
            c.dot(6, 5, hl);
            return Texture::from_pixels(r, c.px.data(), c.w, c.h);
        }
        case WeaponKind::MachineGun: {
            GunCanvas c(40, 16);
            // long thin barrel to the muzzle
            c.fill(12, 6, 38, 9, light);
            c.fill(35, 5, 38, 10, dark);  // muzzle
            // receiver body
            c.fill(2, 5, 16, 11, mid);
            c.fill(2, 5, 4, 11, dark);     // stock back
            // curved magazine hanging down
            c.fill(8, 11, 12, 16, dark);
            c.fill(7, 14, 13, 16, dark);
            // grip
            c.fill(2, 11, 6, 16, wood);
            // sight on top
            c.dot(10, 4, dark);
            c.dot(30, 4, dark);
            return Texture::from_pixels(r, c.px.data(), c.w, c.h);
        }
        case WeaponKind::RocketLauncher: {
            GunCanvas c(42, 16);
            // fat launch tube with a flared muzzle and a rear venturi
            c.fill(6, 4, 36, 11, mid);
            c.fill(34, 2, 40, 13, dark);   // muzzle flare
            c.fill(0, 5, 6, 10, dark);     // back blast cone
            c.fill(10, 1, 22, 4, dark);    // optic block
            c.fill(12, 2, 20, 3, hl);
            c.fill(14, 11, 20, 16, wood);  // grip
            c.fill(24, 11, 30, 14, mid);   // forward handle
            return Texture::from_pixels(r, c.px.data(), c.w, c.h);
        }
        // The thrown weapons are held, not fired: draw the fist-sized object
        // itself so the in-hand sprite reads as a grenade rather than a gun.
        case WeaponKind::Grenade:
        case WeaponKind::Concussion:
        case WeaponKind::Lure: {
            const Uint32 shell =
                (k == WeaponKind::Grenade)    ? gun_pack(74, 92, 58)
                : (k == WeaponKind::Concussion) ? gun_pack(70, 108, 150)
                                                : gun_pack(150, 120, 48);
            const Uint32 shell_hi =
                (k == WeaponKind::Grenade)    ? gun_pack(104, 128, 82)
                : (k == WeaponKind::Concussion) ? gun_pack(104, 150, 198)
                                                : gun_pack(198, 168, 78);
            GunCanvas c(16, 16);
            c.fill(3, 5, 13, 15, shell);       // body
            c.fill(4, 6, 8, 10, shell_hi);     // lit shoulder
            c.fill(5, 2, 11, 5, dark);         // fuse cap
            c.fill(6, 0, 10, 2, mid);          // spoon
            c.fill(3, 8, 13, 9, dark);         // banding
            c.fill(3, 11, 13, 12, dark);
            return Texture::from_pixels(r, c.px.data(), c.w, c.h);
        }
        case WeaponKind::Pistol:
        default: {
            GunCanvas c(26, 14);
            // slide / body
            c.fill(2, 5, 20, 9, mid);
            c.fill(2, 5, 4, 9, dark);     // back of slide
            // short barrel front
            c.fill(18, 6, 24, 9, light);
            c.fill(23, 6, 25, 9, dark);  // muzzle
            // grip angled down-left
            c.fill(3, 9, 8, 14, wood);
            // trigger guard hint
            c.fill(8, 10, 12, 12, dark);
            // sights
            c.dot(6, 4, dark);
            c.dot(16, 4, dark);
            return Texture::from_pixels(r, c.px.data(), c.w, c.h);
        }
    }
}

}  // namespace bd
