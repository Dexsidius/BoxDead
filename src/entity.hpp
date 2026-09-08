// BoxDead - Entity system
// Entity: base type with position/size/velocity and update/render hooks.
// Player: controllable entity driven by keyboard input.
// Enemy: chases the player.
#pragma once

#include "sprite.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Shared per-frame state passed to every entity's update().
struct GameContext {
    const bool* keys = nullptr;   // SDL keyboard state array
    float world_w = 0.0f;
    float world_h = 0.0f;
    Vec2 player_pos;              // updated before enemies tick
};

class Entity {
public:
    Vec2 pos;    // center of the entity, in world pixels
    Vec2 size;   // width / height in world pixels
    Vec2 vel;
    bool alive = true;

    Entity(float x, float y, float w, float h) : pos{x, y}, size{w, h} {}
    virtual ~Entity() = default;

    virtual void update(float /*dt*/, const GameContext& /*ctx*/) {}
    virtual void render(SDL_Renderer* r) const {
        draw_sprite(r, sprite_, pos.x, pos.y, size.x, size.y);
    }

protected:
    Sprite sprite_;
};

class Player : public Entity {
public:
    explicit Player(float x, float y)
        : Entity(x, y, 32.0f, 32.0f), speed_(320.0f) {
        sprite_.color = SDL_Color{60, 160, 255, 255};
    }

    void set_texture(Texture* tex) { sprite_.texture = tex; }

    void update(float dt, const GameContext& ctx) override {
        float dx = 0.0f;
        float dy = 0.0f;
        if (ctx.keys) {
            if (ctx.keys[SDL_SCANCODE_A] || ctx.keys[SDL_SCANCODE_LEFT])
                dx -= 1.0f;
            if (ctx.keys[SDL_SCANCODE_D] || ctx.keys[SDL_SCANCODE_RIGHT])
                dx += 1.0f;
            if (ctx.keys[SDL_SCANCODE_W] || ctx.keys[SDL_SCANCODE_UP])
                dy -= 1.0f;
            if (ctx.keys[SDL_SCANCODE_S] || ctx.keys[SDL_SCANCODE_DOWN])
                dy += 1.0f;
        }
        // Normalize diagonals so diagonal speed matches cardinal speed.
        if (dx != 0.0f && dy != 0.0f) {
            const float inv = 1.0f / std::sqrt(2.0f);
            dx *= inv;
            dy *= inv;
        }
        pos.x += dx * speed_ * dt;
        pos.y += dy * speed_ * dt;

        // Clamp to the world bounds.
        const float hw = size.x * 0.5f;
        const float hh = size.y * 0.5f;
        pos.x = std::clamp(pos.x, hw, ctx.world_w - hw);
        pos.y = std::clamp(pos.y, hh, ctx.world_h - hh);
    }

private:
    float speed_;
};

class Enemy : public Entity {
public:
    explicit Enemy(float x, float y)
        : Entity(x, y, 28.0f, 28.0f), speed_(120.0f) {
        sprite_.color = SDL_Color{220, 45, 45, 255};
    }

    void set_texture(Texture* tex) { sprite_.texture = tex; }

    void update(float dt, const GameContext& ctx) override {
        Vec2 d{ctx.player_pos.x - pos.x, ctx.player_pos.y - pos.y};
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len > 1.0f) {
            d.x /= len;
            d.y /= len;
            pos.x += d.x * speed_ * dt;
            pos.y += d.y * speed_ * dt;
        }
    }

private:
    float speed_;
};
