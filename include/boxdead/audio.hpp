// BoxDead - Audio: one-shot sound effects on plain SDL3 audio.
//
// No SDL_mixer: each sound is a WAV loaded once into memory and played through
// a small pool of SDL_AudioStreams. SDL mixes every stream bound to the device
// for us, so a pool of N streams per sound means up to N of that sound can
// overlap - which matters when a barrel chain fires half a dozen explosions in
// the same second.
//
// Audio is optional. If the device will not open (headless CI, no sound card)
// init() returns false and play() becomes a no-op, so nothing else has to care.
#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace bd {

enum class Sound {
    BulletWall,   // a round chipping stone
    EnemyHit,     // a shot landing on a body
    Explosion,    // barrels, rockets, grenades
    Count,
};

class Audio {
public:
    Audio() = default;
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // Loads the WAVs under `asset_dir` (e.g. "assets/sfx"). Returns false if
    // audio is unavailable; the game carries on silently.
    bool init(const std::string& asset_dir);
    void shutdown();

    // Fire and forget. `gain` scales this one shot (1.0 = as recorded).
    void play(Sound s, float gain = 1.0f);

    bool ready() const { return ready_; }

private:
    // Voices per sound: how many copies can overlap before the oldest is cut.
    static constexpr int kVoices = 4;

    struct Clip {
        Uint8* buffer = nullptr;   // owned, SDL_free'd
        Uint32 length = 0;
        SDL_AudioSpec spec{};
        SDL_AudioStream* voices[kVoices] = {};
        int next = 0;              // round-robin cursor
    };

    Clip clips_[static_cast<int>(Sound::Count)];
    bool ready_ = false;
};

}  // namespace bd
