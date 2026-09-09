// BoxDead - Entity base implementation
#include "boxdead/entity.hpp"

#include "boxdead/tilemap.hpp"

#include <cmath>

namespace bd {

bool GameContext::blocked(float px, float py) const {
    if (tilemap && tilemap->is_solid(px, py)) return true;
    if (obstacles) {
        for (const Obstacle& o : *obstacles) {
            if (px >= o.pos.x - o.size.x * 0.5f &&
                px < o.pos.x + o.size.x * 0.5f &&
                py >= o.pos.y - o.size.y * 0.5f &&
                py < o.pos.y + o.size.y * 0.5f) {
                return true;
            }
        }
    }
    return false;
}

void Entity::render(SDL_Renderer* r, float cam_x, float cam_y) const {
    draw_sprite(r, sprite_, pos.x - cam_x, pos.y - cam_y, size.x, size.y);
}

bool entities_overlap(const Entity& a, const Entity& b) {
    const float dx = std::abs(a.pos.x - b.pos.x);
    const float dy = std::abs(a.pos.y - b.pos.y);
    return dx < (a.size.x + b.size.x) * 0.5f &&
           dy < (a.size.y + b.size.y) * 0.5f;
}

}  // namespace bd
