// BoxDead - Tile elevation test: ".mx" format version 2 gives each placement a
// fifth element, the height it stands off the floor. This checks that the
// elevation is read, that raised tiles come back sorted back-to-front by the
// line where they meet the ground, that standing a tile up does not move its
// collision, and that a map written in the original four-element form still
// loads flat. Run from the repo root with:
//   SDL_VIDEO_DRIVER=dummy ./test_tile_elevation
#include "boxdead/tilemap.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::printf("  %-58s %s\n", what.c_str(), cond ? "ok" : "FAIL");
    if (!cond) ++failures;
}

// Write a throwaway .mx next to the working directory. Its tile art points at a
// .bmp that already ships with the Courtyard scene, so the test needs no
// fixture of its own checked in beside it.
const char* kTempMap = "test_tile_elevation_tmp.mx";
// The flat counterpart. This used to reuse a shipped scene, but the scenes now
// declare elevation, so the "legacy maps load flat" check has to own a fixture
// that cannot drift out from under it.
const char* kTempFlatMap = "test_tile_elevation_flat_tmp.mx";

bool write_temp_map() {
    std::ofstream f(kTempMap);
    if (!f.is_open()) return false;
    f << R"({
    "formatVersion": 2,
    "name": "ElevationFixture",
    "tiles": {
        "Wall": {
            "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
            "locations": [
                [100, 100, 32, 32, 40],
                [200, 200, 32, 32, 16],
                [300, 300, 32, 32, 0],
                [400, 400, 32, 32]
            ]
        }
    }
})";
    return true;
}

// A map in the original four-element form: every location is [x, y, w, h].
bool write_temp_flat_map() {
    std::ofstream f(kTempFlatMap);
    if (!f.is_open()) return false;
    f << R"({
    "name": "FlatFixture",
    "tiles": {
        "Wall": {
            "filepath": "assets/maps/Courtyard/assets/Wall.bmp",
            "locations": [
                [100, 100, 32, 32],
                [200, 200, 32, 32],
                [300, 300, 32, 32]
            ]
        }
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

    if (!write_temp_flat_map()) {
        std::printf("could not write %s\n", kTempFlatMap);
        return 1;
    }
    if (!write_temp_map()) {
        std::printf("could not write %s (run from the repo root)\n", kTempMap);
        return 1;
    }

    // Four placements, two of them standing off the floor. The one that sorts
    // first is written first, so the ordering check is not passing by accident
    // of file order alone - it is also verified by value below.
    std::printf("format version 2 map:\n");
    {
        bd::Tilemap tm;
        const bool ok = tm.load(r, kTempMap);
        check(ok, "loads");
        check(tm.tile_count() == 4, "all four placements kept");
        check(tm.raised_count() == 2, "two tiles reported as raised");

        // Ground line is y + h and ignores elevation, so a tall tile still
        // sorts by the floor it stands on rather than by how tall it is.
        if (tm.raised_count() == 2) {
            check(tm.raised_ground_line(0) <= tm.raised_ground_line(1),
                  "raised tiles sorted back to front");
            check(tm.raised_ground_line(0) == 132.0f,
                  "nearest raised ground line is y+h = 132");
            check(tm.raised_ground_line(1) == 232.0f,
                  "furthest raised ground line is y+h = 232");
        }

        // Out of range indices must not walk off the vector.
        check(tm.raised_ground_line(-1) == 0.0f, "negative index is harmless");
        check(tm.raised_ground_line(99) == 0.0f, "past-the-end index is harmless");

        // Elevation is a drawing concern only. "Wall" is a solid name, so the
        // footprint still blocks exactly where the level author placed it, and
        // the lifted top face blocks nothing.
        check(tm.is_solid(110.0f, 110.0f),
              "raised solid tile blocks at its footprint");
        check(!tm.is_solid(110.0f, 60.0f),
              "it does not block where it was drawn lifted");
    }

    // A map in the original four-element form has no elevations at all.
    std::printf("format version 1 map (no fifth element):\n");
    {
        bd::Tilemap tm;
        const bool ok = tm.load(r, kTempFlatMap);
        check(ok, "still loads");
        check(tm.tile_count() == 3, "has tiles");
        check(tm.raised_count() == 0, "every tile is flat");
    }

    // Loading a second map must not leave the first map's raised indices
    // behind, pointing into a tile list that has since been rebuilt.
    std::printf("reload:\n");
    {
        bd::Tilemap tm;
        tm.load(r, kTempMap);
        const int before = tm.raised_count();
        tm.load(r, kTempFlatMap);
        check(before == 2 && tm.raised_count() == 0,
              "raised list is rebuilt, not appended to");
    }

    std::remove(kTempMap);
    std::remove(kTempFlatMap);

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();

    if (failures != 0) {
        std::printf("\ntile elevation test: %d FAILURES\n", failures);
        return 1;
    }
    std::printf("\ntile elevation test: all checks passed\n");
    return 0;
}
