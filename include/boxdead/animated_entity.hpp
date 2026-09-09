// BoxDead - AnimatedEntity: an Entity that plays frame-based animations.
//
// Player and Enemy derive from this so they can run named animations (walk,
// idle, ...) while lightweight entities (projectiles, items) stay simple.
// All animations must be added before the first play() call.
#pragma once

#include "boxdead/animation.hpp"
#include "boxdead/entity.hpp"

#include <string>

namespace bd {

class AnimatedEntity : public Entity {
public:
    using Entity::Entity;

    // Register a named animation sliced from a horizontal sprite sheet of
    // `frame_count` equal `frame_size`-pixel frames.
    void add_animation(const std::string& name, Texture* tex, int frame_count,
                      int frame_size, float frame_duration, bool loop);

    void play_animation(const std::string& name) { animator_.play(name); }
    void update_animator(float dt) { animator_.update(dt); }
    size_t animator_frame_index() const { return animator_.frame_index(); }
    bool animator_playing() const { return animator_.texture() != nullptr; }

    void render(SDL_Renderer* r, float cam_x, float cam_y) const override;

protected:
    Animator animator_;
};

}  // namespace bd
