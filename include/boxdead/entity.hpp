// BoxDead - Entity base class + shared game context
#pragma once

#include "boxdead/math.hpp"
#include "boxdead/sprite.hpp"

#include <SDL3/SDL.h>

#include <vector>

namespace bd {

class Tilemap;  // forward declaration (defined in boxdead/tilemap.hpp)

// A dynamic blocker: something that stops movement but is not part of the
// static tileset (currently only barrels, which can be destroyed).
struct Obstacle {
    Vec2 pos;   // centre
    Vec2 size;  // full width/height
};

// Shared per-frame state passed to every entity's update().
struct GameContext {
    const bool* keys = nullptr;   // SDL keyboard state array
    float world_w = 0.0f;
    float world_h = 0.0f;
    Vec2 player_pos;              // updated before enemies tick
    const Tilemap* tilemap = nullptr;  // current scene tileset (for collision)
    const std::vector<Obstacle>* obstacles = nullptr;  // barrels, ...

    // True if a world point is inside a solid tile or a dynamic obstacle.
    // Movement code steps per-axis through this so entities slide along
    // walls and barrels instead of sticking to them.
    bool blocked(float px, float py) const;
};

// Base entity: position (centered), size, velocity, alive flag.
// Subclasses override update() and set a sprite_ for rendering.
class Entity {
public:
    Vec2 pos;
    Vec2 size;
    Vec2 vel;
    int health = 1;       // hits before death (player/enemies share this)
    bool alive = true;

    Entity(float x, float y, float w, float h) : pos{x, y}, size{w, h} {}
    virtual ~Entity() = default;

    virtual void update(float /*dt*/, const GameContext& /*ctx*/) {}
    // Draw the entity. `cam_x/cam_y` subtract the camera so a world larger
    // than the viewport scrolls: screen = world - camera.
    virtual void render(SDL_Renderer* r, float cam_x, float cam_y) const;

    // Apply damage; sets alive=false when health drops to zero.
    void damage(int amount) {
        health -= amount;
        if (health <= 0) alive = false;
    }

protected:
    Sprite sprite_;
};

// Axis-aligned overlap test for two centered entities.
bool entities_overlap(const Entity& a, const Entity& b);

}  // namespace bd
