// Unit test for the Animator: a named animation advances frames over time and
// loops. No SDL runtime needed (Animation holds a Texture* we leave null).
#include "boxdead/animation.hpp"

#include <cassert>
#include <cstdio>

int main() {
    bd::Animator a;

    // Build a 4-frame animation at 0.1s/frame, looping.
    bd::Animation anim;
    anim.texture = nullptr;
    for (int i = 0; i < 4; ++i)
        anim.frames.push_back(SDL_FRect{static_cast<float>(i), 0.0f, 1.0f, 1.0f});
    anim.frame_duration = 0.1f;
    anim.loop = true;
    a.add("walk", anim);

    // Not playing yet -> no frame.
    assert(a.frame() == nullptr);
    assert(a.frame_index() == 0);

    a.play("walk");
    assert(a.frame() != nullptr);
    assert(a.frame_index() == 0);

    // Under one frame's duration -> still frame 0.
    a.update(0.05f);
    assert(a.frame_index() == 0);

    // Cross into frame 1.
    a.update(0.06f);
    assert(a.frame_index() == 1);

    // Advance to the last frame and loop back to 0.
    a.update(0.1f);  // -> 2
    a.update(0.1f);  // -> 3
    assert(a.frame_index() == 3);
    a.update(0.1f);  // loops -> 0
    assert(a.frame_index() == 0);

    // Non-looping animation clamps on the last frame and reports finished.
    bd::Animation once = anim;
    once.loop = false;
    a.add("once", once);
    a.play("once");
    for (int i = 0; i < 10; ++i) a.update(0.1f);
    assert(a.finished());
    assert(a.frame_index() == 3);

    std::printf("animator logic OK\n");
    return 0;
}
