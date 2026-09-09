// BoxDead - Isometric character renderer (procedural, per-frame).
// Draws a Boxhead-style box figure: a shaded, black-outlined stack of boxes
// (shoes, legs, torso, arms, head, hair/horns) with a face on the head's front
// side and a gun held in the outstretched arms. The body yaws to face the
// aim/movement direction; the gun rotates continuously to the exact aim angle.
// World gameplay stays top-down; only the character rendering is isometric.
//
// The style is authored as flat base colors (skin, hair, shirt, ...) — the
// renderer derives the per-face top/front/side shading itself, so a character
// palette is a handful of colors instead of a shade table.
#pragma once

#include <SDL3/SDL.h>

#include "boxdead/texture.hpp"

namespace bd {

// Palette for a Boxhead-style character. Colors with alpha 0 are treated as
// "absent" (no hair slab, no tie, no horns) so one struct covers survivors,
// zombies, and devils.
struct IsoCharStyle {
    SDL_Color skin{236, 200, 158, 255};    // head + arms
    SDL_Color hair{26, 22, 26, 255};       // slab on top of the head
    SDL_Color shirt{60, 120, 200, 255};    // torso
    SDL_Color pants{48, 52, 72, 255};      // legs
    SDL_Color shoe{26, 24, 30, 255};       // feet
    SDL_Color tie{0, 0, 0, 0};             // stripe down the torso front
    SDL_Color horn{0, 0, 0, 0};            // two spikes on top of the head
    SDL_Color eye{24, 20, 24, 255};        // face detail on the head front
    SDL_Color outline{16, 14, 18, 235};    // edge lines (the Boxhead look)
    bool blood = false;                     // gore on the torso + a mouth
    // Multiplied into every color as it is drawn. Used for the player's
    // i-frame flash (red) and the barrel's pre-detonation flash (white).
    SDL_Color tint{255, 255, 255, 255};
};

// Draw an isometric character with its feet centered at screen point (cx, cy).
// w,h set the body footprint width and standing height in pixels. facing_x/y
// is the normalized aim/movement direction (body yaws toward it). walk_phase
// drives leg alternation (radians). When gun_tex is non-null the current gun
// is drawn in the hands, rotated to gun_angle_rad (screen-space radians).
void draw_iso_character(SDL_Renderer* r, float cx, float cy, float w, float h,
                        float facing_x, float facing_y, float walk_phase,
                        const IsoCharStyle& style, Texture* gun_tex,
                        float gun_angle_rad);

// Palette for an explosive barrel prop.
struct IsoBarrelStyle {
    SDL_Color body{188, 52, 40, 255};      // drum shell
    SDL_Color band{86, 24, 20, 255};       // two hoops around the drum
    SDL_Color lid{214, 88, 66, 255};       // top face
    SDL_Color outline{16, 14, 18, 235};
    SDL_Color tint{255, 255, 255, 255};    // white-flash while the fuse burns
};

// Draw an upright barrel with its base centered at screen point (cx, cy).
// `w` is the drum diameter in pixels, `h` its standing height.
void draw_iso_barrel(SDL_Renderer* r, float cx, float cy, float w, float h,
                     const IsoBarrelStyle& style);

// Draw an explosion fireball centered at (cx, cy). `radius` is the blast
// radius in world pixels; `t` is the animation progress in [0, 1] (0 = the
// instant of detonation, 1 = fully dissipated).
void draw_explosion(SDL_Renderer* r, float cx, float cy, float radius,
                    float t);

}  // namespace bd
