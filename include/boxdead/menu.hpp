// BoxDead - Menu: a navigable text menu (main menu / options / etc.)
#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace bd {

class Font;

// A simple text menu. Items are selected with Up/Down + Enter, or by
// hovering/clicking with the mouse. The owning code decides what each
// item index means.
class Menu {
public:
    void set_items(const std::vector<std::string>& items);
    void reset();  // move selection back to the first item

    // Handle one event. Returns the index of the activated item, or -1.
    int handle_event(const SDL_Event& e, float window_w, float window_h);

    // Index of the currently highlighted item (not yet activated). The
    // character-select screen reads this to preview the highlighted survivor.
    int selected_index() const { return selected_; }

    // Render the title centered near the top, items listed below it.
    void render(Font& font, const std::string& title, float window_w,
               float window_h) const;

private:
    float first_y(float window_h) const { return window_h * 0.4f; }
    float row_height() const { return 40.0f; }

    std::vector<std::string> items_;
    int selected_ = 0;
};

}  // namespace bd
