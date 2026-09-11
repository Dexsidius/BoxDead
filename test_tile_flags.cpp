// BoxDead - Tile flags test: ".mx" format version 3 lets a map say what each
// tile means with a "flags" list instead of leaving the game to guess from the
// tile's name. This checks every branch of the reading rule: an explicit list is
// the whole answer (even when empty, and even when the name says otherwise),
// flags match case-insensitively, flags BoxDead does not know are ignored, and a
// map with no "flags" key still falls back to the name. Run from the repo root:
//   SDL_VIDEO_DRIVER=dummy ./test_tile_flags
#include "boxdead/tilemap.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::printf("  %-62s %s\n", what.c_str(), cond ? "ok" : "FAIL");
    if (!cond) ++failures;
}

const char* kTempMap = "test_tile_flags_tmp.mx";

// One tile type per rule, each on its own column so a solid probe at x+10 can
// only ever land on that one tile. Every entry reuses art shipped with the
// Courtyard scene, so the test needs no fixture checked in beside it.
bool write_temp_map() {
    std::ofstream f(kTempMap);
    if (!f.is_open()) return false;
    f << R"({
    "formatVersion": 3,
    "name": "FlagsFixture",
    "tiles": {
        "Grass":        { "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
                          "flags": ["solid"],
                          "locations": [[100, 100, 32, 32, 0]] },
        "Hedge Block":  { "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
                          "flags": [],
                          "locations": [[200, 100, 32, 32, 0]] },
        "Wall":         { "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
                          "locations": [[300, 100, 32, 32, 0]] },
        "Mystery Prop": { "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
                          "flags": ["explosive"],
                          "locations": [[400, 100, 32, 32, 0]] },
        "Loud Thing":   { "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
                          "flags": ["SOLID"],
                          "locations": [[500, 100, 32, 32, 0]] },
        "Puddle":       { "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
                          "flags": ["hazard", "water"],
                          "locations": [[600, 100, 32, 32, 0]] },
        "Old Barrel":   { "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
                          "locations": [[700, 100, 32, 32]] }
    }
})";
    return true;
}

}  // namespace

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

    if (!write_temp_map()) {
        std::printf("could not write %s (run from the repo root)\n", kTempMap);
        return 1;
    }

    bd::Tilemap tm;
    const bool ok = tm.load(r, kTempMap);

    std::printf("loading:\n");
    check(ok, "loads");
    // Grass, Hedge Block, Wall, Loud Thing, Puddle draw as tiles; the two
    // explosive entries become barrel spawns instead.
    check(tm.tile_count() == 5, "five entries drawn as tiles");
    check(tm.explosive_spawns().size() == 2, "two entries became barrel spawns");

    std::printf("an explicit flags list is the whole answer:\n");
    check(tm.is_solid(110.0f, 110.0f),
          "flags [solid] makes a tile solid whatever it is called");
    check(!tm.is_solid(210.0f, 110.0f),
          "flags [] is walkable even though its name has \"block\"");
    check(tm.is_solid(510.0f, 110.0f), "flags match case-insensitively");
    check(!tm.is_solid(610.0f, 110.0f),
          "flags BoxDead does not act on are ignored, not treated as solid");

    std::printf("a map with no flags key falls back to the name:\n");
    check(tm.is_solid(310.0f, 110.0f), "\"Wall\" with no flags is still solid");
    check(!tm.is_solid(710.0f, 110.0f),
          "\"Old Barrel\" with no flags is a barrel, not a solid tile");

    std::remove(kTempMap);

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();

    if (failures != 0) {
        std::printf("\ntile flags test: %d FAILURES\n", failures);
        return 1;
    }
    std::printf("\ntile flags test: all checks passed\n");
    return 0;
}
