// BoxDead - Enemy implementation
#include "boxdead/enemy.hpp"
#include "boxdead/iso_sprite.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace bd {

namespace {

// Per-kind stats, kept in one place so adding an enemy is a table entry plus a
// palette rather than a scatter of conditionals.
struct EnemyStats {
    int health;
    int points;     // score for killing one
    float speed;    // pixels per second
    float size;     // collision box (the figure is drawn larger, see render)
    float range;    // fireball range; 0 for melee-only kinds
    float interval; // seconds between volleys
    int count;      // fireballs per volley
    float spread;   // degrees the volley fans across
};

constexpr float kBaseSpeed = 120.0f;

EnemyStats stats_for(EnemyKind k) {
    switch (k) {
        // Point-blank spitter: same speed as a zombie, one extra hit to kill.
        case EnemyKind::Devil:
            return {2, 150, kBaseSpeed, 36.0f, 50.0f, 1.4f, 1, 0.0f};
        // Mini-boss: slightly quicker than the horde and a real ranged threat.
        case EnemyKind::YellowDemon:
            return {5, 250, kBaseSpeed * 1.2f, 42.0f, 260.0f, 1.5f, 1, 0.0f};
        // End-of-level boss: big, slow, heavy, fires a three-way spread.
        case EnemyKind::Boss:
            return {60, 1000, kBaseSpeed * 0.8f, 76.0f, 340.0f, 1.1f, 3, 26.0f};
        case EnemyKind::Zombie:
        default:
            return {1, 50, kBaseSpeed, 36.0f, 0.0f, 0.0f, 0, 0.0f};
    }
}

// Boxhead zombie: sickly green skin, black hair, bloodied white office shirt
// with a red tie, dark slacks.
IsoCharStyle zombie_style() {
    IsoCharStyle s;
    s.skin = SDL_Color{150, 186, 96, 255};
    s.hair = SDL_Color{34, 30, 30, 255};
    s.shirt = SDL_Color{226, 226, 220, 255};
    s.pants = SDL_Color{54, 54, 62, 255};
    s.shoe = SDL_Color{28, 26, 30, 255};
    s.tie = SDL_Color{176, 38, 38, 255};
    s.eye = SDL_Color{28, 40, 26, 255};
    s.blood = true;
    return s;
}

// Red devil: red hide, no hair, two black horns, dark red body.
IsoCharStyle devil_style() {
    IsoCharStyle s;
    s.skin = SDL_Color{206, 58, 48, 255};
    s.hair = SDL_Color{0, 0, 0, 0};  // bald, so the horns read clearly
    s.shirt = SDL_Color{138, 30, 28, 255};
    s.pants = SDL_Color{86, 20, 20, 255};
    s.shoe = SDL_Color{40, 14, 14, 255};
    s.horn = SDL_Color{32, 22, 24, 255};
    s.eye = SDL_Color{252, 214, 96, 255};  // glowing eyes
    return s;
}

// Yellow demon: brass-gold hide with dark horns and burning red eyes, so it
// stands out against both the green horde and the red devils.
IsoCharStyle yellow_demon_style() {
    IsoCharStyle s;
    s.skin = SDL_Color{240, 200, 56, 255};
    s.hair = SDL_Color{0, 0, 0, 0};
    s.shirt = SDL_Color{198, 152, 26, 255};
    s.pants = SDL_Color{140, 104, 16, 255};
    s.shoe = SDL_Color{72, 52, 10, 255};
    s.horn = SDL_Color{58, 42, 16, 255};
    s.eye = SDL_Color{226, 46, 30, 255};
    return s;
}

// Boss: a hulking near-black demon with bone horns and burning amber eyes.
IsoCharStyle boss_style() {
    IsoCharStyle s;
    s.skin = SDL_Color{132, 46, 52, 255};
    s.hair = SDL_Color{0, 0, 0, 0};
    s.shirt = SDL_Color{74, 24, 32, 255};
    s.pants = SDL_Color{50, 18, 24, 255};
    s.shoe = SDL_Color{28, 12, 16, 255};
    s.horn = SDL_Color{218, 200, 168, 255};  // bone
    s.eye = SDL_Color{255, 176, 40, 255};
    return s;
}

IsoCharStyle style_for(EnemyKind k) {
    switch (k) {
        case EnemyKind::Devil: return devil_style();
        case EnemyKind::YellowDemon: return yellow_demon_style();
        case EnemyKind::Boss: return boss_style();
        case EnemyKind::Zombie:
        default: return zombie_style();
    }
}

}  // namespace

Enemy::Enemy(EnemyKind kind, float x, float y)
    : AnimatedEntity(x, y, stats_for(kind).size, stats_for(kind).size),
      kind_(kind),
      speed_(stats_for(kind).speed),
      max_health_(stats_for(kind).health) {
    health = max_health_;
    sprite_.color = SDL_Color{255, 255, 255, 255};  // no tint
}

int Enemy::points() const { return stats_for(kind_).points; }
float Enemy::fire_range() const { return stats_for(kind_).range; }
float Enemy::fire_interval() const { return stats_for(kind_).interval; }
int Enemy::fire_count() const { return stats_for(kind_).count; }
float Enemy::fire_spread_deg() const { return stats_for(kind_).spread; }

void Enemy::update(float dt, const GameContext& ctx) {
    if (stun_ > 0.0f) {
        // Frozen: no movement, no walk cycle, and the Game skips it when
        // handing out ranged attacks. Tinted pale blue so the player can see
        // which enemies the concussion caught.
        stun_ = std::max(0.0f, stun_ - dt);
        sprite_.color = SDL_Color{150, 190, 255, 255};
        update_animator(dt);
        return;
    }
    sprite_.color = SDL_Color{255, 255, 255, 255};
    if (lure_ > 0.0f) lure_ = std::max(0.0f, lure_ - dt);

    // Lured enemies walk to the lure point instead of at the player; once the
    // lure expires (or they arrive) they go back to hunting.
    const Vec2 target = (lure_ > 0.0f) ? lure_pos_ : ctx.player_pos;
    Vec2 d{target.x - pos.x, target.y - pos.y};
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > 1.0f) {
        d.x /= len;
        d.y /= len;
        const float sx = d.x * speed_ * dt;
        const float sy = d.y * speed_ * dt;
        // Slide along walls and barrels: block each axis that enters one.
        if (!ctx.blocked(pos.x + sx, pos.y)) pos.x += sx;
        if (!ctx.blocked(pos.x, pos.y + sy)) pos.y += sy;
        facing_ = d;  // face the player
    }

    // Enemies are always chasing, so the walk cycle runs continuously. Big
    // bosses take longer strides, so their cycle runs slower.
    walk_phase_ += dt * (is_boss() ? 5.0f : 8.0f);
    play_animation("walk");
    update_animator(dt);
}

void Enemy::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    IsoCharStyle style = style_for(kind_);
    style.tint = sprite_.color;  // flashes when the Game tints a hit enemy
    // Same as the player: the figure is drawn bigger than its hitbox so the
    // characters read at Boxhead scale without making the horde unfair.
    constexpr float kDrawScale = 1.3f;
    draw_iso_character(r, pos.x - cam_x, pos.y + size.y * 0.25f - cam_y,
                       size.x * kDrawScale, size.y * kDrawScale,
                       facing_.x, facing_.y, walk_phase_, style, nullptr, 0.0f);
}

}  // namespace bd
