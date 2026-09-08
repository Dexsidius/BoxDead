// BoxDead - Isometric character renderer (procedural, per-frame).
// Draws a shaded 3D box character (shadow, legs, body, head) and an optional
// gun in the hand. The body yaws to face the aim/movement direction; the gun
// rotates continuously to the exact aim angle. World gameplay stays top-down;
// only the character rendering is isometric.
#pragma once

#include <SDL3/SDL.h>

#include "boxdead/texture.hpp"

namespace bd {

// Palette for an isometric character: each visible face gets a shade so the box
// reads as 3D (top lightest, front mid, side dark).
struct IsoCharStyle {
    SDL_Color body_top{255, 255, 255, 255};
    SDL_Color body_front{200, 200, 200, 255};
    SDL_Color body_side{150, 150, 150, 255};
    SDL_Color head_top{255, 255, 255, 255};
    SDL_Color head_front{200, 200, 200, 255};
    SDL_Color head_side{150, 150, 150, 255};
    SDL_Color leg{80, 80, 90, 255};
    SDL_Color shadow{0, 0, 0, 90};
};

// Draw an isometric character with its feet centered at screen point (cx, cy).
// w,h set the body footprint width and standing height in pixels. facing_x/y
// is the normalized aim/movement direction (body yaws toward it). walk_phase
// drives leg alternation (radians). When gun_tex is non-null the current gun
// is drawn in the hand, rotated to gun_angle_rad (screen-space radians).
void draw_iso_character(SDL_Renderer* r, float cx, float cy, float w, float h,
                        float facing_x, float facing_y, float walk_phase,
                        const IsoCharStyle& style, Texture* gun_tex,
                        float gun_angle_rad);

}  // namespace bd
