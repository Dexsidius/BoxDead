// BoxDead - Isometric character renderer implementation.
//
// Everything here is drawn from a handful of primitives:
//   fill_quad / outline_quad  - a flat shaded polygon and its edge lines
//   fill_ellipse              - shadows, barrel lids, explosion puffs
//   draw_box                  - a yawed, shaded, outlined 3D box (one limb)
//   fill_face_rect            - a decal (eye, tie, blood) painted onto one
//                               face of a box in that face's own UV space
// A character is a stack of boxes plus decals; a barrel is a cylinder faked
// from two ellipses and a body quad.
#include "boxdead/iso_sprite.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace bd {

namespace {
constexpr float kPi = 3.14159265358979323846f;

struct Pt3 {
    float x, y, z;
};

inline SDL_FPoint project(Pt3 p, float cx, float cy) {
    // Isometric projection: right (+x) goes down-right, forward (+y) goes
    // down-left, up (+z) goes up. Feet sit at z=0.
    return SDL_FPoint{cx + (p.x - p.y) * 0.5f,
                      cy + (p.x + p.y) * 0.25f - p.z};
}

inline Uint8 mul8(Uint8 a, Uint8 b) {
    return static_cast<Uint8>((static_cast<int>(a) * b + 127) / 255);
}

// Multiply a color by a tint (255,255,255 = unchanged). Alpha is preserved.
inline SDL_Color tinted(SDL_Color c, SDL_Color t) {
    return SDL_Color{mul8(c.r, t.r), mul8(c.g, t.g), mul8(c.b, t.b), c.a};
}

// Scale a color's brightness (f > 1 lightens, f < 1 darkens).
inline SDL_Color shade(SDL_Color c, float f) {
    const auto ch = [f](Uint8 v) {
        return static_cast<Uint8>(
            std::clamp(static_cast<float>(v) * f, 0.0f, 255.0f));
    };
    return SDL_Color{ch(c.r), ch(c.g), ch(c.b), c.a};
}

inline SDL_FColor to_fcolor(SDL_Color c) {
    return SDL_FColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
                      c.a / 255.0f};
}

inline bool visible_color(SDL_Color c) { return c.a > 0; }

void fill_quad(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c,
               SDL_FPoint d, SDL_Color col) {
    if (!visible_color(col)) return;
    const SDL_FColor fc = to_fcolor(col);
    const SDL_Vertex v[4] = {{a, fc, {0, 0}},
                            {b, fc, {0, 0}},
                            {c, fc, {0, 0}},
                            {d, fc, {0, 0}}};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

// Trace the four edges of a quad. This is what gives the figures the crisp
// dark Boxhead silhouette instead of reading as untextured gradient blobs.
void outline_quad(SDL_Renderer* r, const SDL_FPoint q[4], SDL_Color col) {
    if (!visible_color(col)) return;
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    const SDL_FPoint loop[5] = {q[0], q[1], q[2], q[3], q[0]};
    SDL_RenderLines(r, loop, 5);
}

void fill_ellipse(SDL_Renderer* r, float cx, float cy, float rx, float ry,
                  SDL_Color col) {
    if (!visible_color(col)) return;
    constexpr int N = 22;
    const SDL_FColor fc = to_fcolor(col);
    SDL_Vertex v[N + 1];
    int idx[3 * N];
    v[0] = SDL_Vertex{{cx, cy}, fc, {0, 0}};
    for (int i = 0; i < N; ++i) {
        const float a = static_cast<float>(i) / N * 2.0f * kPi;
        v[i + 1] = SDL_Vertex{{cx + std::cos(a) * rx, cy + std::sin(a) * ry},
                              fc, {0, 0}};
        idx[i * 3 + 0] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = (i + 1 < N ? i + 2 : 1);
    }
    SDL_RenderGeometry(r, nullptr, v, N + 1, idx, 3 * N);
}

void outline_ellipse(SDL_Renderer* r, float cx, float cy, float rx, float ry,
                     SDL_Color col) {
    if (!visible_color(col)) return;
    constexpr int N = 22;
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    SDL_FPoint loop[N + 1];
    for (int i = 0; i <= N; ++i) {
        const float a = static_cast<float>(i % N) / N * 2.0f * kPi;
        loop[i] = SDL_FPoint{cx + std::cos(a) * rx, cy + std::sin(a) * ry};
    }
    SDL_RenderLines(r, loop, N + 1);
}

// One body part: an axis-aligned box in the character's local space, yawed
// with the body. `base` is the centre of its bottom face, in local space —
// x/y are rotated by the body yaw along with the box itself, so a limb placed
// out to the side or forward orbits with the facing instead of staying pinned
// to a world direction.
struct Box {
    Pt3 base;
    float hw, hd, hh;  // half-extents (x, y) and full height (z)
};

// Local side faces, in order: 0 = front (-y, the character's face), 1 = right
// (+x), 2 = back (+y), 3 = left (-x). Each is [base_a, base_b, top_b, top_a],
// which is also the UV frame used by fill_face_rect (u across, v upward).
struct SideFace {
    int v[4];
    float nx, ny;
};
constexpr SideFace kFaces[4] = {
    {{1, 0, 4, 5}, 0.0f, -1.0f},   // front
    {{2, 1, 5, 6}, 1.0f, 0.0f},    // right
    {{3, 2, 6, 7}, 0.0f, 1.0f},    // back
    {{0, 3, 7, 4}, -1.0f, 0.0f},   // left
};

// Draw a yawed box with back-face culling and back-to-front face ordering so
// it reads as a shaded 3D volume. Faces are shaded by how squarely they face
// the (1,1) view direction. If `want_face` is 0..3 and that face is visible,
// its projected quad is written to `face_out` (for decals) and true returned.
bool draw_box(SDL_Renderer* r, float cx, float cy, const Box& b, float yaw,
              SDL_Color color, SDL_Color outline, int want_face = -1,
              SDL_FPoint face_out[4] = nullptr) {
    const float ca = std::cos(yaw);
    const float sa = std::sin(yaw);
    auto rot = [&](Pt3 p) {
        // Offset in local space first, then yaw the whole part into world
        // space; z is already vertical and needs no rotation.
        const float lx = p.x + b.base.x;
        const float ly = p.y + b.base.y;
        return Pt3{lx * ca - ly * sa, lx * sa + ly * ca, p.z + b.base.z};
    };
    // 8 corners (base 0..3 CCW, top 4..7).
    const Pt3 local[8] = {
        {-b.hw, -b.hd, 0}, {b.hw, -b.hd, 0}, {b.hw, b.hd, 0}, {-b.hw, b.hd, 0},
        {-b.hw, -b.hd, b.hh}, {b.hw, -b.hd, b.hh}, {b.hw, b.hd, b.hh},
        {-b.hw, b.hd, b.hh},
    };
    SDL_FPoint p[8];
    for (int i = 0; i < 8; ++i) p[i] = project(rot(local[i]), cx, cy);

    struct Vis {
        int face;
        SDL_FPoint v[4];
        float dot;    // alignment with the view direction
        float depth;  // screen-y of the face centre; smaller = further back
    };
    std::vector<Vis> vis;
    vis.reserve(2);
    bool want_visible = false;
    for (int f = 0; f < 4; ++f) {
        const float nx = kFaces[f].nx * ca - kFaces[f].ny * sa;
        const float ny = kFaces[f].nx * sa + kFaces[f].ny * ca;
        const float dot = nx + ny;  // view direction is (1,1)
        if (dot <= 0.01f) continue;
        Vis d;
        d.face = f;
        for (int i = 0; i < 4; ++i) d.v[i] = p[kFaces[f].v[i]];
        d.dot = dot;
        d.depth = (d.v[0].y + d.v[2].y) * 0.5f;
        vis.push_back(d);
        if (f == want_face) {
            want_visible = true;
            if (face_out) {
                for (int i = 0; i < 4; ++i) face_out[i] = d.v[i];
            }
        }
    }
    // Back-to-front: furthest (smallest depth) first.
    std::sort(vis.begin(), vis.end(),
              [](const Vis& a, const Vis& c) { return a.depth < c.depth; });
    for (const Vis& f : vis) {
        // 0 (edge-on) -> darkest, 1.414 (square to the view) -> lightest.
        const float lit = 0.62f + 0.30f * (f.dot / 1.4142f);
        fill_quad(r, f.v[0], f.v[1], f.v[2], f.v[3], shade(color, lit));
        outline_quad(r, f.v, outline);
    }
    // Top face (roof) last so it sits above the walls.
    const SDL_FPoint top[4] = {p[4], p[5], p[6], p[7]};
    fill_quad(r, top[0], top[1], top[2], top[3], shade(color, 1.12f));
    outline_quad(r, top, outline);
    return want_visible;
}

// Point inside a face quad in its own UV space (u across, v bottom-to-top).
inline SDL_FPoint face_point(const SDL_FPoint q[4], float u, float v) {
    const float bx = q[0].x + (q[1].x - q[0].x) * u;
    const float by = q[0].y + (q[1].y - q[0].y) * u;
    const float tx = q[3].x + (q[2].x - q[3].x) * u;
    const float ty = q[3].y + (q[2].y - q[3].y) * u;
    return SDL_FPoint{bx + (tx - bx) * v, by + (ty - by) * v};
}

// Paint a rectangular decal (eye, mouth, tie, blood) onto one box face.
void fill_face_rect(SDL_Renderer* r, const SDL_FPoint q[4], float u0, float v0,
                    float u1, float v1, SDL_Color col) {
    fill_quad(r, face_point(q, u0, v0), face_point(q, u1, v0),
              face_point(q, u1, v1), face_point(q, u0, v1), col);
}
}  // namespace

void draw_iso_character(SDL_Renderer* r, float cx, float cy, float w, float h,
                        float facing_x, float facing_y, float walk_phase,
                        const IsoCharStyle& style, Texture* gun_tex,
                        float gun_angle_rad) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    const SDL_Color tint = style.tint;
    const SDL_Color c_skin = tinted(style.skin, tint);
    const SDL_Color c_hair = tinted(style.hair, tint);
    const SDL_Color c_shirt = tinted(style.shirt, tint);
    const SDL_Color c_pants = tinted(style.pants, tint);
    const SDL_Color c_shoe = tinted(style.shoe, tint);
    const SDL_Color c_horn = tinted(style.horn, tint);
    const SDL_Color line = style.outline;

    // Boxhead proportions: stocky torso, short legs, oversized square head.
    const float hw = w * 0.36f;          // torso half-width
    const float hd = w * 0.28f;          // torso half-depth
    const float shoe_h = h * 0.10f;
    const float leg_h = h * 0.36f;       // ground -> hips (shoes included)
    const float torso_h = h * 0.36f;     // hips -> shoulders
    const float head_h = h * 0.32f;
    const float head_hw = w * 0.33f;
    const float head_hd = w * 0.28f;
    const float leg_hw = w * 0.15f;
    const float leg_hd = w * 0.14f;
    const float shoulder_z = leg_h + torso_h;

    // World yaw that turns the body's front (local -y) toward the screen-space
    // aim direction. Derived from the projection: a local -y unit vector lands
    // on screen at (0.5(sin+cos), 0.25(sin-cos)), so sin ~ fx + 2fy and
    // cos ~ fx - 2fy.
    float fx = facing_x;
    float fy = facing_y;
    if (std::abs(fx) + std::abs(fy) < 1e-4f) {
        fx = 0.0f;
        fy = 1.0f;  // default: facing the camera
    }
    const float yaw = std::atan2(fx + 2.0f * fy, fx - 2.0f * fy);
    const float sa = std::sin(yaw);
    const float caw = std::cos(yaw);
    // Is the character's front (local -y) turned toward the viewer? Drives
    // whether the outstretched arms are drawn over or behind the torso.
    const bool front_visible = (sa - caw) > 0.0f;

    // Ground shadow (feet at z=0).
    fill_ellipse(r, cx, cy + hd * 0.25f, hw * 1.30f, hd * 0.72f,
                 SDL_Color{0, 0, 0, 110});

    // --- Legs + shoes ------------------------------------------------------
    // Drawn first so the torso covers the hips; only the lower legs and feet
    // show below it. Each foot strides forward/back along the facing direction
    // and lifts only while swinging forward, so one leg bears weight while the
    // other swings — a walk cycle, not a hop.
    const float phase0 = std::sin(walk_phase);
    const float phase1 = std::sin(walk_phase + kPi);
    const float stride = hd * 1.40f;
    const float lift_amt = leg_h * 0.35f;
    const float leg_off = hw * 0.52f;
    for (int s = 0; s < 2; ++s) {
        const float ph = (s == 0) ? phase0 : phase1;
        const float phc = (s == 0) ? walk_phase : walk_phase + kPi;
        const float fwd = -stride * std::cos(phc);  // -y is forward
        const float lift = std::max(0.0f, -ph) * lift_amt;
        Box leg{};
        leg.base = Pt3{(s == 0 ? leg_off : -leg_off), fwd, lift + shoe_h};
        leg.hw = leg_hw;
        leg.hd = leg_hd;
        leg.hh = leg_h - shoe_h;
        draw_box(r, cx, cy, leg, yaw, c_pants, line);

        Box foot{};
        foot.base = Pt3{leg.base.x, fwd - leg_hd * 0.25f, lift};
        foot.hw = leg_hw * 1.05f;
        foot.hd = leg_hd * 1.25f;
        foot.hh = shoe_h;
        draw_box(r, cx, cy, foot, yaw, c_shoe, line);
    }

    // --- Arms --------------------------------------------------------------
    // Both arms reach straight forward (Boxhead characters always hold the
    // weapon out in front). When the character has its back to the camera the
    // arms are behind the torso, so they are drawn first in that case.
    const float arm_len = w * 0.55f;
    const float arm_hw = w * 0.125f;
    const float arm_h = h * 0.13f;
    const float arm_z = shoulder_z - arm_h * 1.9f;
    const float arm_y = -hd * 0.20f - arm_len * 0.5f;
    auto draw_arms = [&]() {
        for (int s = 0; s < 2; ++s) {
            Box arm{};
            arm.base = Pt3{(s == 0 ? 1.0f : -1.0f) * hw, arm_y, arm_z};
            arm.hw = arm_hw;
            arm.hd = arm_len * 0.5f;
            arm.hh = arm_h;
            draw_box(r, cx, cy, arm, yaw, c_skin, line);
        }
    };
    if (!front_visible) draw_arms();

    // --- Torso -------------------------------------------------------------
    Box torso{};
    torso.base = Pt3{0.0f, 0.0f, leg_h};
    torso.hw = hw;
    torso.hd = hd;
    torso.hh = torso_h;
    SDL_FPoint torso_face[4]{};
    const bool torso_front =
        draw_box(r, cx, cy, torso, yaw, c_shirt, line, 0, torso_face);
    if (torso_front) {
        // A tie down the middle (the zombie's office-worker look).
        if (visible_color(style.tie)) {
            const SDL_Color t = tinted(style.tie, tint);
            fill_face_rect(r, torso_face, 0.44f, 0.10f, 0.56f, 0.92f, t);
        }
        if (style.blood) {
            const SDL_Color gore = tinted(SDL_Color{128, 24, 24, 255}, tint);
            fill_face_rect(r, torso_face, 0.16f, 0.46f, 0.34f, 0.74f, gore);
            fill_face_rect(r, torso_face, 0.62f, 0.24f, 0.74f, 0.44f, gore);
            fill_face_rect(r, torso_face, 0.30f, 0.16f, 0.40f, 0.28f, gore);
        }
    }

    if (front_visible) draw_arms();

    // --- Head + hair + horns ----------------------------------------------
    Box head{};
    head.base = Pt3{0.0f, -hd * 0.06f, shoulder_z};
    head.hw = head_hw;
    head.hd = head_hd;
    head.hh = head_h;
    SDL_FPoint head_face[4]{};
    const bool head_front =
        draw_box(r, cx, cy, head, yaw, c_skin, line, 0, head_face);
    if (head_front) {
        const SDL_Color eye = tinted(style.eye, tint);
        fill_face_rect(r, head_face, 0.20f, 0.46f, 0.38f, 0.64f, eye);
        fill_face_rect(r, head_face, 0.62f, 0.46f, 0.80f, 0.64f, eye);
        if (style.blood) {  // zombies get a gaping mouth
            fill_face_rect(r, head_face, 0.34f, 0.16f, 0.66f, 0.30f,
                           tinted(SDL_Color{78, 26, 30, 255}, tint));
        }
    }

    // Hair sits as its own slab on the skull so it keeps a separate outline.
    if (visible_color(style.hair)) {
        Box hair{};
        hair.base = Pt3{head.base.x, head.base.y, shoulder_z + head_h};
        hair.hw = head_hw * 1.04f;
        hair.hd = head_hd * 1.04f;
        hair.hh = h * 0.08f;
        draw_box(r, cx, cy, hair, yaw, c_hair, line);
    }
    if (visible_color(style.horn)) {
        const float horn_z =
            shoulder_z + head_h + (visible_color(style.hair) ? h * 0.08f : 0.0f);
        for (int s = 0; s < 2; ++s) {
            Box horn{};
            horn.base = Pt3{(s == 0 ? 1.0f : -1.0f) * head_hw * 0.66f,
                            head.base.y - head_hd * 0.30f, horn_z};
            horn.hw = w * 0.07f;
            horn.hd = w * 0.07f;
            horn.hh = h * 0.20f;
            draw_box(r, cx, cy, horn, yaw, c_horn, line);
        }
    }

    // --- Gun ---------------------------------------------------------------
    // Held at the tip of the arms, rotated to the exact aim angle so the
    // muzzle lines up with where the bullets actually go.
    if (gun_tex && gun_tex->get()) {
        const float hand_x = hw * 0.20f * caw - (arm_y - arm_len * 0.42f) * sa;
        const float hand_y = hw * 0.20f * sa + (arm_y - arm_len * 0.42f) * caw;
        const SDL_FPoint hand =
            project(Pt3{hand_x, hand_y, arm_z + arm_h * 0.45f}, cx, cy);
        const double angle_deg = gun_angle_rad * 180.0 / kPi;
        // Sized against the body, not the texture: a fixed multiplier made
        // the shotgun longer than the character is tall.
        const float scale = w / 46.0f;
        const float gw = static_cast<float>(gun_tex->w()) * scale;
        const float gh = static_cast<float>(gun_tex->h()) * scale;
        const SDL_FPoint piv{gw * 0.15f, gh * 0.5f};
        const SDL_FRect dst{hand.x - piv.x, hand.y - piv.y, gw, gh};
        SDL_SetTextureColorMod(gun_tex->get(), tint.r, tint.g, tint.b);
        SDL_RenderTextureRotated(r, gun_tex->get(), nullptr, &dst, angle_deg,
                                 &piv, SDL_FLIP_NONE);
        SDL_SetTextureColorMod(gun_tex->get(), 255, 255, 255);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void draw_iso_barrel(SDL_Renderer* r, float cx, float cy, float w, float h,
                     const IsoBarrelStyle& style) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    const SDL_Color body = tinted(style.body, style.tint);
    const SDL_Color band = tinted(style.band, style.tint);
    const SDL_Color lid = tinted(style.lid, style.tint);
    const SDL_Color line = style.outline;

    // A cylinder faked the same way the projection treats the ground plane:
    // a circle of radius rx flattens to ry = rx * 0.5.
    const float rx = w * 0.5f;
    const float ry = rx * 0.5f;
    const float top_y = cy - h;

    // Ground shadow.
    fill_ellipse(r, cx, cy + ry * 0.15f, rx * 1.12f, ry * 0.95f,
                 SDL_Color{0, 0, 0, 110});

    // Drum shell: the bottom cap, then the barrel wall as a lit-to-shadowed
    // vertical band (light on the upper-left, where the iso "sun" sits).
    fill_ellipse(r, cx, cy, rx, ry, shade(body, 0.70f));
    const SDL_FPoint wall[4] = {{cx - rx, top_y}, {cx + rx, top_y},
                                {cx + rx, cy}, {cx - rx, cy}};
    fill_quad(r, wall[0], wall[1], wall[2], wall[3], body);
    // Vertical shading strips across the wall so it reads as round.
    constexpr int kStrips = 7;
    for (int i = 0; i < kStrips; ++i) {
        const float u0 = static_cast<float>(i) / kStrips;
        const float u1 = static_cast<float>(i + 1) / kStrips;
        // Brightest a third of the way in from the left, falling off to the
        // right edge.
        const float mid = (u0 + u1) * 0.5f;
        const float lit = 1.10f - 0.55f * std::abs(mid - 0.32f) / 0.68f;
        const SDL_Color col = shade(body, lit);
        fill_quad(r, SDL_FPoint{cx - rx + 2.0f * rx * u0, top_y},
                  SDL_FPoint{cx - rx + 2.0f * rx * u1, top_y},
                  SDL_FPoint{cx - rx + 2.0f * rx * u1, cy},
                  SDL_FPoint{cx - rx + 2.0f * rx * u0, cy}, col);
    }
    // Two hoops around the drum.
    for (int i = 0; i < 2; ++i) {
        const float v = (i == 0) ? 0.28f : 0.68f;
        const float by = top_y + h * v;
        const float bh = std::max(2.0f, h * 0.09f);
        fill_quad(r, SDL_FPoint{cx - rx, by}, SDL_FPoint{cx + rx, by},
                  SDL_FPoint{cx + rx, by + bh}, SDL_FPoint{cx - rx, by + bh},
                  band);
    }
    // Silhouette: sides + the rounded bottom.
    SDL_SetRenderDrawColor(r, line.r, line.g, line.b, line.a);
    SDL_RenderLine(r, cx - rx, top_y, cx - rx, cy);
    SDL_RenderLine(r, cx + rx, top_y, cx + rx, cy);
    outline_ellipse(r, cx, cy, rx, ry, line);

    // Lid on top, with a rim and a bung cap so it reads as a fuel drum.
    fill_ellipse(r, cx, top_y, rx, ry, lid);
    outline_ellipse(r, cx, top_y, rx, ry, line);
    fill_ellipse(r, cx, top_y, rx * 0.62f, ry * 0.62f, shade(lid, 0.86f));
    fill_ellipse(r, cx + rx * 0.30f, top_y - ry * 0.12f, rx * 0.16f,
                 ry * 0.16f, band);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void draw_explosion(SDL_Renderer* r, float cx, float cy, float radius,
                    float t) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const float p = std::clamp(t, 0.0f, 1.0f);
    // Fast expansion that eases out, then a fade over the whole life.
    const float grow = 1.0f - (1.0f - p) * (1.0f - p);
    const float fade = 1.0f - p;
    const float rr = radius * (0.35f + 0.65f * grow);
    // The blast hugs the ground, so squash it the same way the projection
    // squashes the ground plane.
    const float sq = 0.55f;

    // Flame layers from the outside in, then a white-hot core.
    // Each layer fades on its own curve: the outer flame dies quickest, the
    // hot centre lingers, so the blast reads as a fireball collapsing rather
    // than a flat disc dissolving.
    const auto puff = [&](float scale, SDL_Color col, float alpha, float k) {
        SDL_Color c = col;
        c.a = static_cast<Uint8>(
            std::clamp(alpha * std::pow(fade, k) * 255.0f, 0.0f, 255.0f));
        fill_ellipse(r, cx, cy, rr * scale, rr * scale * sq, c);
    };
    puff(1.00f, SDL_Color{238, 96, 28, 255}, 0.90f, 1.8f);
    puff(0.72f, SDL_Color{250, 152, 44, 255}, 0.95f, 1.3f);
    puff(0.46f, SDL_Color{252, 206, 96, 255}, 1.00f, 0.9f);
    if (p < 0.5f) {
        SDL_Color core{255, 248, 220, 255};
        core.a = static_cast<Uint8>((1.0f - p / 0.5f) * 255.0f);
        fill_ellipse(r, cx, cy, rr * 0.30f, rr * 0.30f * sq, core);
    }
    // Shockwave ring racing ahead of the fireball.
    SDL_Color ring{255, 214, 130, 0};
    ring.a = static_cast<Uint8>(
        std::clamp(std::pow(fade, 1.5f) * 170.0f, 0.0f, 255.0f));
    outline_ellipse(r, cx, cy, radius * grow, radius * grow * sq, ring);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

}  // namespace bd
