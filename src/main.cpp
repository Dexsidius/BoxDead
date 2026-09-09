// BoxDead - entry point. Initializes SDL, runs the Game, cleans up.
#include "boxdead/game.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    bool smoke_test = false;
    int smoke_frames = 0;  // 0 = the default run length
    bool menu_shot = false;
    int start_level = 0;
    std::string screenshot_path;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--smoke-test") {
            smoke_test = true;
            // Optional frame count: "--smoke-test 15000" runs a long soak,
            // which is what it takes to march through the wave ladder to a
            // boss wave now that waves end on a clear rather than a timer.
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                smoke_frames = std::atoi(argv[++i]);
            }
        }
        else if (arg == "--level" && i + 1 < argc)
            start_level = std::atoi(argv[++i]);
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshot_path = argv[++i];
        else if (arg == "--menu-shot" && i + 1 < argc) {
            menu_shot = true;
            screenshot_path = argv[++i];
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    int rc = 0;
    {
        bd::Game game(smoke_test, screenshot_path, menu_shot, smoke_frames,
                      start_level);
        if (!game.init()) {
            rc = 1;
        } else {
            game.run();
        }
    }

    SDL_Quit();
    return rc;
}
