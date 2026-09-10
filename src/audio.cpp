// BoxDead - Audio implementation
#include "boxdead/audio.hpp"

#include <iostream>

namespace bd {

namespace {
const char* file_for(Sound s) {
    switch (s) {
        case Sound::BulletWall: return "bullet_wall.wav";
        case Sound::EnemyHit: return "enemy_hit.wav";
        case Sound::Explosion: return "explosion.wav";
        default: return nullptr;
    }
}
}  // namespace

Audio::~Audio() { shutdown(); }

bool Audio::init(const std::string& asset_dir) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        std::cerr << "audio unavailable: " << SDL_GetError()
                  << " (continuing without sound)\n";
        return false;
    }

    for (int i = 0; i < static_cast<int>(Sound::Count); ++i) {
        Clip& c = clips_[i];
        const std::string path =
            asset_dir + "/" + file_for(static_cast<Sound>(i));
        if (!SDL_LoadWAV(path.c_str(), &c.spec, &c.buffer, &c.length)) {
            std::cerr << "could not load " << path << ": " << SDL_GetError()
                      << '\n';
            shutdown();
            return false;
        }
        // One logical device stream per voice. SDL mixes them all onto the
        // physical device, which is what lets the same sound overlap itself.
        for (int v = 0; v < kVoices; ++v) {
            c.voices[v] = SDL_OpenAudioDeviceStream(
                SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &c.spec, nullptr, nullptr);
            if (!c.voices[v]) {
                std::cerr << "could not open an audio stream: "
                          << SDL_GetError() << '\n';
                shutdown();
                return false;
            }
            SDL_ResumeAudioStreamDevice(c.voices[v]);
        }
    }
    ready_ = true;
    return true;
}

void Audio::play(Sound s, float gain) {
    if (!ready_) return;
    const int idx = static_cast<int>(s);
    if (idx < 0 || idx >= static_cast<int>(Sound::Count)) return;
    Clip& c = clips_[idx];

    // Round-robin the voices. Clearing first cuts whatever that voice was
    // still playing, so the oldest overlapping copy is the one that gets
    // dropped rather than the new sound being swallowed.
    SDL_AudioStream* v = c.voices[c.next];
    c.next = (c.next + 1) % kVoices;
    SDL_ClearAudioStream(v);
    SDL_SetAudioStreamGain(v, gain);
    SDL_PutAudioStreamData(v, c.buffer, static_cast<int>(c.length));
}

void Audio::shutdown() {
    for (int i = 0; i < static_cast<int>(Sound::Count); ++i) {
        Clip& c = clips_[i];
        for (int v = 0; v < kVoices; ++v) {
            if (c.voices[v]) {
                SDL_DestroyAudioStream(c.voices[v]);
                c.voices[v] = nullptr;
            }
        }
        if (c.buffer) {
            SDL_free(c.buffer);
            c.buffer = nullptr;
        }
        c.length = 0;
    }
    if (ready_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        ready_ = false;
    }
}

}  // namespace bd
