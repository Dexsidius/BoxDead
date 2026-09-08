// BoxDead - Animation system
// Animation: a horizontal strip of frames in one texture, with timing.
// Animator: plays named animations by frame over time.
#pragma once

#include "boxdead/texture.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace bd {

// A frame-based animation. Frames are source rects into a single texture
// (a horizontal sprite sheet), played left-to-right at `frame_duration`.
struct Animation {
    Texture* texture = nullptr;
    std::vector<SDL_FRect> frames;
    float frame_duration = 0.1f;  // seconds per frame
    bool loop = true;
};

// Builds an Animation from a horizontal sprite sheet of `frame_count`
// equal-sized frames.
inline Animation make_animation(Texture* tex, int frame_count,
                                int frame_size) {
    Animation a;
    a.texture = tex;
    a.frames.reserve(static_cast<size_t>(frame_count));
    for (int i = 0; i < frame_count; ++i) {
        a.frames.push_back(SDL_FRect{static_cast<float>(i * frame_size), 0.0f,
                                     static_cast<float>(frame_size),
                                     static_cast<float>(frame_size)});
    }
    return a;
}

// Plays animations by name. Tracks the current frame over time and exposes
// it for rendering. Owns a small set of named Animation copies.
class Animator {
public:
    void add(const std::string& name, const Animation& anim);
    void play(const std::string& name);  // switch (no-op if already playing)
    void update(float dt);

    Texture* texture() const { return current_ ? current_->texture : nullptr; }
    const SDL_FRect* frame() const;
    bool finished() const { return finished_; }

private:
    std::unordered_map<std::string, Animation> animations_;
    Animation* current_ = nullptr;
    float timer_ = 0.0f;
    size_t index_ = 0;
    bool finished_ = false;
};

}  // namespace bd
