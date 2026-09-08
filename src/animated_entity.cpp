// BoxDead - AnimatedEntity implementation
#include "boxdead/animated_entity.hpp"

#include "boxdead/sprite.hpp"

namespace bd {

void AnimatedEntity::add_animation(const std::string& name, Texture* tex,
                                   int frame_count, int frame_size,
                                   float frame_duration, bool loop) {
    Animation a = make_animation(tex, frame_count, frame_size);
    a.frame_duration = frame_duration;
    a.loop = loop;
    animator_.add(name, a);
}

void AnimatedEntity::render(SDL_Renderer* r) const {
    if (animator_playing() && animator_.frame()) {
        // Compose a sprite from the current animation frame, keeping the
        // entity's color as the tint (preserves the player's i-frame flash).
        Sprite s;
        s.texture = animator_.texture();
        s.src = *animator_.frame();
        s.color = sprite_.color;
        draw_sprite(r, s, pos.x, pos.y, size.x, size.y);
    } else {
        draw_sprite(r, sprite_, pos.x, pos.y, size.x, size.y);
    }
}

}  // namespace bd
