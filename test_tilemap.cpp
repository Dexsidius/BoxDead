// BoxDead - Tilemap load test: loads every scene's ".mx" tileset under a
// dummy SDL renderer and asserts each parsed with tiles and with explosive
// barrel placements. Run with:
//   SDL_VIDEO_DRIVER=dummy ./test_tilemap
#include "boxdead/tilemap.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>
#include <vector>

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("t", 8, 8, 0);
    SDL_Renderer* r = SDL_CreateRenderer(win, nullptr);
    if (!r) {
        std::printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 1;
    }

    const std::vector<std::string> maps = {
        "assets/maps/Courtyard/Courtyard.mx",
        "assets/maps/Asylum/Asylum.mx",
        "assets/maps/Sewers/Sewers.mx",
        "assets/maps/Graveyard/Graveyard.mx",
        "assets/maps/Hells Gate/Hells Gate.mx",
    };

    int failures = 0;
    for (const auto& path : maps) {
        bd::Tilemap tm;
        const bool ok = tm.load(r, path);
        const int barrels = static_cast<int>(tm.explosive_spawns().size());
        std::printf("  %-40s %s tiles=%d barrels=%d\n", path.c_str(),
                    ok ? "OK " : "FAIL", tm.tile_count(), barrels);
        if (!ok || tm.tile_count() == 0) ++failures;
        // Barrel tiles must be lifted out of the drawn/collided tile list and
        // handed over as spawn points, or the level has no explosives.
        if (barrels == 0) {
            std::printf("    ^ no explosive barrel spawns\n");
            ++failures;
        }
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();

    if (failures != 0) {
        std::printf("tilemap load test: %d FAILURES\n", failures);
        return 1;
    }
    std::printf("tilemap load test: all 5 maps OK\n");
    return 0;
}
