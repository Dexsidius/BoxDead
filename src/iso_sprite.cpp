// BoxDead - Isometric character renderer implementation.
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

inline SDL_FColor to_fcolor(SDL_Color c) {
    return SDL_FColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
                      c.a / 255.0f};
}

void fill_quad(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c,
               SDL_FPoint d, SDL_Color col) {
    const SDL_FColor fc = to_fcolor(col);
    const SDL_Vertex v[4] = {{a, fc, {0, 0}},
                            {b, fc, {0, 0}},
                            {c, fc, {0, 0}},
                            {d, fc, {0, 0}}};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

void fill_ellipse(SDL_Renderer* r, float cx, float cy, float rx, float ry,
                  SDL_Color col) {
    constexpr int N = 18;
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

// A yawed box drawn with back-face culling + back-to-front ordering so it
// reads as a shaded 3D volume. The two visible side faces get `front` (mid)
// and `side` (dark) shades; the top face gets `top` (lightest).
struct Box {
    float cxw, cyw, czw;  // world position of the box base center
    float hw, hd, hh;     // half-extents (x, y, z)
};

void draw_box(SDL_Renderer* r, float cx, float cy, const Box& b, float yaw,
              SDL_Color top, SDL_Color front, SDL_Color side) {
    const float ca = std::cos(yaw);
    const float sa = std::sin(yaw);
    auto rot = [&](Pt3 p) {
        return Pt3{p.x * ca - p.y * sa + b.cxw,
                   p.x * sa + p.y * ca + b.cyw,
                   p.z + b.czw};
    };
    // 8 corners (base 0..3 CCW, top 4..7).
    const Pt3 local[8] = {
        {-b.hw, -b.hd, 0}, {b.hw, -b.hd, 0}, {b.hw, b.hd, 0}, {-b.hw, b.hd, 0},
        {-b.hw, -b.hd, b.hh}, {b.hw, -b.hd, b.hh}, {b.hw, b.hd, b.hh}, {-b.hw, b.hd, b.hh},
    };
    SDL_FPoint p[8];
    for (int i = 0; i < 8; ++i) p[i] = project(rot(local[i]), cx, cy);

    struct SideFace {
        int v[4];
        float nx, ny;
    };
    const SideFace faces[4] = {
        {{0, 1, 5, 4}, 0.0f, -1.0f},  // -y
        {{1, 2, 6, 5}, 1.0f, 0.0f},   // +x
        {{2, 3, 7, 6}, 0.0f, 1.0f},  // +y
        {{3, 0, 4, 7}, -1.0f, 0.0f}, // -x
    };

    struct Vis {
        SDL_FPoint v[4];
        float dot;   // alignment with the view direction (1,1,1)
        float depth;  // screen-y of face center; smaller = further back
    };
    std::vector<Vis> vis;
    for (const auto& f : faces) {
        const float nx = f.nx * ca - f.ny * sa;
        const float ny = f.nx * sa + f.ny * ca;
        const float dot = nx + ny;  // (1,1,1) . (nx,ny,0)
        if (dot > 0.01f) {
            Vis d;
            for (int i = 0; i < 4; ++i) d.v[i] = p[f.v[i]];
            d.dot = dot;
            d.depth = (p[f.v[0]].y + p[f.v[2]].y) * 0.5f;
            vis.push_back(d);
        }
    }
    // Back-to-front: furthest (smallest depth) first.
    std::sort(vis.begin(), vis.end(),
              [](const Vis& a, const Vis& c) { return a.depth < c.depth; });
    // Most-aligned face = front (mid shade); the other = side (dark).
    for (size_t i = 0; i < vis.size(); ++i) {
        const bool is_front = (i + 1 == vis.size());  // last drawn = most front
        const SDL_Color col = is_front ? front : side;
        fill_quad(r, vis[i].v[0], vis[i].v[1], vis[i].v[2], vis[i].v[3], col);
    }
    // Top face (roof) drawn last so it sits above the walls.
    fill_quad(r, p[4], p[5], p[6], p[7], top);
}
}  // namespace

void draw_iso_character(SDL_Renderer* r, float cx, float cy, float w, float h,
                        float facing_x, float facing_y, float walk_phase,
                        const IsoCharStyle& style, Texture* gun_tex,
                        float gun_angle_rad) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Body footprint + standing height.
    const float hw = w * 0.5f;
    const float hd = w * 0.4f;
    const float leg_h = h * 0.4f;        // leg height (feet -> hips)
    const float bh = h * 0.5f;           // body height (hips -> shoulders)
    const float head_h = h * 0.32f;      // head height
    const float head_hw = w * 0.26f;
    const float head_hd = w * 0.22f;
    const float leg_hw = w * 0.19f;
    const float leg_hd = w * 0.16f;

    // World yaw that points the body's front toward the screen facing dir.
    const float yaw =
        std::atan2(-facing_x + facing_y, facing_x + facing_y) + 0.02f;

    // Shadow on the ground (feet at z=0).
    fill_ellipse(r, cx, cy + hd * 0.25f, hw * 1.25f, hd * 0.7f,
                SDL_Color{0, 0, 0, 130});

    // Legs drawn FIRST so the body covers the hips; only the lower legs +
    // feet show below the torso. Each leg is a thin box planted on the ground
    // (full height, no shrink) whose foot strides forward/back along the
    // facing direction and lifts only while swinging forward through the air.
    // One leg swings while the other bears weight -> reads as a real walk
    // cycle instead of a synchronized hop.
    const float phase0 = std::sin(walk_phase);
    const float phase1 = std::sin(walk_phase + kPi);
    const float stride = hd * 1.5f;       // forward/back foot travel (pokes beyond body)
    const float lift_amt = leg_h * 0.4f;   // foot lift during the swing phase
    const float leg_off = hw * 0.55f;      // left/right of body center
    for (int s = 0; s < 2; ++s) {
        const float ph = (s == 0) ? phase0 : phase1;
        const float phc = (s == 0) ? walk_phase : walk_phase + kPi;
        // Foot travels forward/back with cos(); lifts only while swinging
        // forward (sin < 0), planted (lift 0) while bearing weight.
        const float fwd = stride * std::cos(phc);
        const float lift = std::max(0.0f, -ph) * lift_amt;
        Box leg{};
        leg.cxw = (s == 0 ? leg_off : -leg_off);
        leg.cyw = fwd;        // stride forward/back under the body
        leg.czw = lift;       // foot lifts off the ground during the swing
        leg.hw = leg_hw;
        leg.hd = leg_hd;
        leg.hh = leg_h;       // full height — the leg never shrinks/hops
        draw_box(r, cx, cy, leg, yaw, style.leg, style.leg, style.leg);
    }

    // Body box sits on top of the legs (base at the hip line).
    Box body{};
    body.cxw = 0.0f;
    body.cyw = 0.0f;
    body.czw = leg_h;
    body.hw = hw;
    body.hd = hd;
    body.hh = bh;
    draw_box(r, cx, cy, body, yaw, style.body_top, style.body_front,
             style.body_side);

    // Head box on top of the body.
    Box head{};
    head.cxw = 0.0f;
    head.cyw = -hd * 0.1f;
    head.czw = leg_h + bh;
    head.hw = head_hw;
    head.hd = head_hd;
    head.hh = head_h;
    draw_box(r, cx, cy, head, yaw, style.head_top, style.head_front,
             style.head_side);

    // Gun in the hand, at shoulder height, rotated to the exact aim angle.
    if (gun_tex && gun_tex->get()) {
        const SDL_FPoint hand3 =
            project(Pt3{hw * 0.6f, -hd * 0.2f, leg_h + bh * 0.9f}, cx, cy);
        const double angle_deg = gun_angle_rad * 180.0 / kPi;
        const float scale = 1.25f;
        const float gw = static_cast<float>(gun_tex->w()) * scale;
        const float gh = static_cast<float>(gun_tex->h()) * scale;
        const SDL_FPoint piv{gw * 0.15f, gh * 0.8f};
        const SDL_FRect dst{hand3.x - piv.x, hand3.y - piv.y, gw, gh};
        SDL_RenderTextureRotated(r, gun_tex->get(), nullptr, &dst, angle_deg,
                                 &piv, SDL_FLIP_NONE);
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

}  // namespace bd
