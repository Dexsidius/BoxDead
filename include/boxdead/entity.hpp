// BoxDead - Entity base class + shared game context
#pragma once

#include "boxdead/math.hpp"
#include "boxdead/sprite.hpp"

#include <SDL3/SDL.h>

namespace bd {

// Shared per-frame state passed to every entity's update().
struct GameContext {
    const bool* keys = nullptr;   // SDL keyboard state array
    float world_w = 0.0f;
    float world_h = 0.0f;
    Vec2 player_pos;              // updated before enemies tick
};

// Base entity: position (centered), size, velocity, alive flag.
// Subclasses override update() and set a sprite_ for rendering.
class Entity {
public:
    Vec2 pos;
    Vec2 size;
    Vec2 vel;
    bool alive = true;

    Entity(float x, float y, float w, float h) : pos{x, y}, size{w, h} {}
    virtual ~Entity() = default;

    virtual void update(float /*dt*/, const GameContext& /*ctx*/) {}
    virtual void render(SDL_Renderer* r) const;

protected:
    Sprite sprite_;
};

// Axis-aligned overlap test for two centered entities.
bool entities_overlap(const Entity& a, const Entity& b);

}  // namespace bd
