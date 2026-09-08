// BoxDead - Animator implementation
#include "boxdead/animation.hpp"

namespace bd {

void Animator::add(const std::string& name, const Animation& anim) {
    animations_[name] = anim;
}

void Animator::play(const std::string& name) {
    auto it = animations_.find(name);
    if (it == animations_.end()) return;
    if (current_ == &it->second) return;  // already playing
    current_ = &it->second;
    timer_ = 0.0f;
    index_ = 0;
    finished_ = false;
}

void Animator::update(float dt) {
    if (!current_ || current_->frames.empty() || finished_) return;
    timer_ += dt;
    while (timer_ >= current_->frame_duration) {
        timer_ -= current_->frame_duration;
        ++index_;
        if (index_ >= current_->frames.size()) {
            if (current_->loop) {
                index_ = 0;
            } else {
                index_ = current_->frames.size() - 1;
                finished_ = true;
                break;
            }
        }
    }
}

const SDL_FRect* Animator::frame() const {
    if (!current_ || current_->frames.empty()) return nullptr;
    return &current_->frames[index_];
}

}  // namespace bd
