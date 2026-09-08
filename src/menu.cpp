// BoxDead - Menu implementation
#include "boxdead/menu.hpp"

#include "boxdead/font.hpp"

namespace bd {

void Menu::set_items(const std::vector<std::string>& items) {
    items_ = items;
    if (selected_ >= static_cast<int>(items_.size())) selected_ = 0;
}

void Menu::reset() { selected_ = 0; }

int Menu::handle_event(const SDL_Event& e, float window_w, float window_h) {
    const float fy = first_y(window_h);
    const float rh = row_height();
    const float x1 = (window_w - 320.0f) * 0.5f;
    const float x2 = x1 + 320.0f;
    auto item_at = [&](float mx, float my) -> int {
        if (my < fy || mx < x1 || mx > x2) return -1;
        int i = static_cast<int>((my - fy) / rh);
        if (i >= 0 && i < static_cast<int>(items_.size())) return i;
        return -1;
    };
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_UP || e.key.key == SDLK_W) {
                selected_ = (selected_ - 1 +
                             static_cast<int>(items_.size())) %
                            static_cast<int>(items_.size());
            } else if (e.key.key == SDLK_DOWN || e.key.key == SDLK_S) {
                selected_ = (selected_ + 1) %
                            static_cast<int>(items_.size());
            } else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) {
                return selected_;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION: {
            int i = item_at(e.motion.x, e.motion.y);
            if (i >= 0) selected_ = i;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                int i = item_at(e.button.x, e.button.y);
                if (i >= 0) {
                    selected_ = i;
                    return selected_;
                }
            }
            break;
        default:
            break;
    }
    return -1;
}

void Menu::render(Font& font, const std::string& title, float window_w,
                 float window_h) const {
    const int tw = font.text_width(title);
    font.draw(title, (window_w - tw) * 0.5f, window_h * 0.18f,
              SDL_Color{255, 255, 255, 255});

    const float fy = first_y(window_h);
    const float rh = row_height();
    float y = fy;
    for (size_t i = 0; i < items_.size(); ++i) {
        const bool sel = static_cast<int>(i) == selected_;
        std::string label = (sel ? "> " : "  ") + items_[i];
        const int iw = font.text_width(label);
        const SDL_Color c =
            sel ? SDL_Color{255, 220, 60, 255}
                : SDL_Color{200, 200, 210, 255};
        font.draw(label, (window_w - iw) * 0.5f, y, c);
        y += rh;
    }
}

}  // namespace bd
