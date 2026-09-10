// BoxDead - Audio test: the generated WAVs exist, parse, and can be opened as
// playback streams. Skips cleanly on a machine with no sound device. Run with:
//   ./test_audio
#include "boxdead/audio.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

int main() {
    if (!SDL_Init(0)) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Every sound must at least be a readable WAV, whether or not a device
    // exists - that part is pure file parsing and should never regress.
    int failures = 0;
    const char* files[] = {"assets/sfx/bullet_wall.wav",
                          "assets/sfx/enemy_hit.wav",
                          "assets/sfx/explosion.wav"};
    for (const char* f : files) {
        SDL_AudioSpec spec{};
        Uint8* buf = nullptr;
        Uint32 len = 0;
        const bool ok = SDL_LoadWAV(f, &spec, &buf, &len);
        std::printf("  %-30s %s", f, ok ? "OK " : "FAIL");
        if (ok) {
            std::printf(" %d Hz %d ch %u bytes (%.0f ms)", spec.freq,
                        spec.channels, len,
                        1000.0 * len / (spec.freq * spec.channels * 2.0));
            SDL_free(buf);
        } else {
            std::printf(" %s", SDL_GetError());
            ++failures;
        }
        std::printf("\n");
    }

    // Opening the device is allowed to fail (headless CI); that is a skip.
    bd::Audio audio;
    if (audio.init("assets/sfx")) {
        audio.play(bd::Sound::BulletWall, 0.4f);
        audio.play(bd::Sound::EnemyHit, 0.4f);
        audio.play(bd::Sound::Explosion, 0.4f);
        SDL_Delay(120);  // let the mixer actually consume some of it
        std::printf("  playback device                OK  3 sounds queued\n");
        audio.shutdown();
    } else {
        std::printf("  playback device                SKIP (no audio device)\n");
    }

    SDL_Quit();
    if (failures) {
        std::printf("audio test: %d FAILURES\n", failures);
        return 1;
    }
    std::printf("audio test: all checks OK\n");
    return 0;
}
