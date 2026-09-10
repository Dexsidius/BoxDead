// BoxDead - Game implementation: owns the main loop, menu, spawning, firing,
// collisions, item pickups, and rendering. main.cpp is a thin bootstrap.
#define _USE_MATH_DEFINES
#include "boxdead/game.hpp"

#include "boxdead/barrel.hpp"
#include "boxdead/explosion.hpp"
#include "boxdead/item.hpp"
#include "boxdead/sprite.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace bd {

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "BoxDead";
constexpr float kSpawnInterval = 1.2f;    // seconds between enemy spawns
constexpr size_t kMaxEnemies = 50;
constexpr float kFireInterval = 0.18f;    // fallback cooldown (unused now)
constexpr float kProjectileSpeed = 600.0f;
constexpr float kInvulnDuration = 1.2f;   // seconds of i-frames after a hit
constexpr int kPlayerMaxHp = 5;
constexpr float kItemSpawnInterval = 12.0f;  // seconds between floor item spawns
constexpr int kMaxFloorItems = 3;

const std::vector<std::string> kMainMenuItems = {"New Game", "Options", "Exit"};

// Which weapon a pickup contains. Weighted so the workhorses turn up often and
// the ordnance stays a treat: the rocket launcher is the rarest thing to find.
WeaponKind random_weapon_drop() {
    static const WeaponKind kTable[] = {
        WeaponKind::Shotgun,    WeaponKind::Shotgun,   WeaponKind::Shotgun,
        WeaponKind::MachineGun, WeaponKind::MachineGun, WeaponKind::MachineGun,
        WeaponKind::Grenade,    WeaponKind::Grenade,
        WeaponKind::Concussion, WeaponKind::Lure,
        WeaponKind::RocketLauncher,
    };
    constexpr int n = sizeof(kTable) / sizeof(kTable[0]);
    return kTable[std::rand() % n];
}
}  // namespace

// Scene definitions: each map loads a LevelEdit++ ".mx" tileset and has a
// fallback floor color if the tileset is missing. Levels cycle (wrap around)
// so the game always has somewhere to go next.
const Game::Level Game::kLevels[] = {
    {"The Courtyard",  "assets/maps/Courtyard/Courtyard.mx",  {28, 30, 24, 255},  {52, 56, 44, 255}},
    {"The Asylum",     "assets/maps/Asylum/Asylum.mx",        {32, 32, 28, 255},  {60, 60, 52, 255}},
    {"The Sewers",     "assets/maps/Sewers/Sewers.mx",       {18, 28, 24, 255},  {34, 50, 42, 255}},
    {"The Graveyard",  "assets/maps/Graveyard/Graveyard.mx", {22, 20, 28, 255},  {40, 38, 50, 255}},
    {"Hell's Gate",     "assets/maps/Hells Gate/Hells Gate.mx",{30, 14, 12, 255},  {58, 26, 22, 255}},
    // The stress-test scene: 103x64 tiles (3296x2048), roughly 7000 placements.
    {"The Sprawl",     "assets/maps/The Sprawl/The Sprawl.mx",{26, 26, 28, 255},  {48, 48, 52, 255}},
};
const int Game::kLevelCount =
    sizeof(Game::kLevels) / sizeof(Game::kLevels[0]);

Game::Game(bool smoke_test, std::string screenshot_path, bool menu_shot,
           int smoke_frames, int start_level)
    : smoke_test_(smoke_test),
      smoke_frames_(smoke_frames),
      start_level_(start_level),
      screenshot_mode_(!screenshot_path.empty()),
      menu_shot_(menu_shot),
      screenshot_path_(std::move(screenshot_path)) {}

Game::~Game() { shutdown(); }

bool Game::init() {
    window_ = SDL_CreateWindow(kWindowTitle, kWindowWidth, kWindowHeight,
                               SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        return false;
    }
    // Keep gameplay coordinates at a fixed 1280x720 logical view; SDL scales it
    // to the (resizable / fullscreen) window with letterboxing. The camera
    // scrolls within worlds larger than this view.
    SDL_SetRenderLogicalPresentation(renderer_, kViewWidth, kViewHeight,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    player_walk_sheet_ = make_walk_sheet_texture(
        renderer_, SDL_Color{60, 160, 255, 255}, 4, 32, true);
    player_idle_sheet_ = make_walk_sheet_texture(
        renderer_, SDL_Color{60, 160, 255, 255}, 2, 32, false);

    // Boxhead-style zombie: pale green skin, dark hair, white shirt with a
    // red tie, blood splatter.
    CreaturePalette zombie_pal;
    zombie_pal.skin = SDL_Color{158, 194, 90, 255};
    zombie_pal.hair = SDL_Color{51, 51, 51, 255};
    zombie_pal.torso = SDL_Color{232, 232, 232, 255};
    zombie_pal.tie = SDL_Color{198, 40, 40, 255};
    zombie_pal.pants = SDL_Color{58, 58, 58, 255};
    zombie_pal.blood = true;
    zombie_walk_sheet_ = make_creature_sheet_texture(
        renderer_, zombie_pal, 4, 40, true);

    // Boxhead-style red devil: red skin, dark red torso, two horns. Inherits
    // the zombie's behavior but is tougher (2 HP).
    CreaturePalette devil_pal;
    devil_pal.skin = SDL_Color{211, 47, 47, 255};
    devil_pal.torso = SDL_Color{139, 26, 26, 255};
    devil_pal.pants = SDL_Color{74, 16, 16, 255};
    devil_pal.horn = SDL_Color{26, 16, 16, 255};
    devil_walk_sheet_ = make_creature_sheet_texture(
        renderer_, devil_pal, 4, 40, true);

    projectile_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{255, 220, 60, 255}, 8);
    fireball_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{255, 120, 30, 255}, 16);
    health_tex_ = make_cross_sprite_texture(
        renderer_, SDL_Color{40, 170, 70, 255}, SDL_Color{255, 255, 255, 255},
        22);
    weapon_tex_pistol_ = make_solid_sprite_texture(
        renderer_, SDL_Color{200, 200, 210, 255}, 22);
    weapon_tex_shotgun_ = make_solid_sprite_texture(
        renderer_, SDL_Color{230, 140, 40, 255}, 22);
    weapon_tex_machinegun_ = make_solid_sprite_texture(
        renderer_, SDL_Color{70, 200, 230, 255}, 22);

    // In-hand gun sprites (side view, muzzle +x); one per weapon kind.
    for (int i = 0; i < Player::kSlotCount; ++i) {
        gun_hand_tex_[i] =
            make_gun_texture(renderer_, static_cast<WeaponKind>(i));
    }
    // Projectiles: rockets and grenades are bigger and colour-coded so a
    // thrown charge is readable on the floor while its fuse burns.
    rocket_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{240, 170, 60, 255}, 12);
    grenade_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{96, 128, 72, 255}, 12);
    concussion_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{92, 156, 220, 255}, 12);
    lure_tex_ = make_solid_sprite_texture(
        renderer_, SDL_Color{226, 190, 70, 255}, 12);

    if (!TTF_Init()) {
        std::cerr << "TTF_Init failed: " << SDL_GetError() << '\n';
        return false;
    }
    // assets/dejavu-sans.ttf is resolved relative to the executable. Try a
    // few candidate locations so it works from the build dir and a deployed
    // layout next to the executable.
    // SDL3's SDL_GetBasePath() returns a string SDL owns and frees itself in
    // SDL_Quit() — do NOT SDL_free() it here (SDL2's version did transfer
    // ownership; freeing it under SDL3 is a double free that corrupts the
    // heap and crashes on exit).
    const char* base = SDL_GetBasePath();
    const std::string base_dir = base ? std::string(base) : std::string("");
    const std::vector<std::string> font_candidates = {
        base_dir + "assets/dejavu-sans.ttf",
        base_dir + "../assets/dejavu-sans.ttf",
        base_dir + "../../assets/dejavu-sans.ttf",
        "assets/dejavu-sans.ttf",
        "../assets/dejavu-sans.ttf",
    };
    bool font_ok = false;
    std::string font_path;
    for (const std::string& p : font_candidates) {
        if (font_.load(renderer_, p, 24)) {
            font_ok = true;
            font_path = p;
            break;
        }
    }
    if (!font_ok) {
        std::cerr << "Font load failed (tried " << font_candidates.size()
                  << " paths under " << base_dir << ")\n";
        return false;
    }

    // The game draws its own crosshair, so the arrow pointer would just be a
    // second, offset cursor on screen.
    if (!smoke_test_ && !screenshot_mode_) SDL_HideCursor();

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    menu_.set_items(kMainMenuItems);
    state_ = menu_shot_ ? GameState::CharacterSelect
                        : ((smoke_test_ || screenshot_mode_) ? GameState::Playing
                                                              : GameState::MainMenu);
    if (menu_shot_) {
        // Character-select preview screen; render one frame and capture it.
        menu_.set_items({character_name(0), character_name(1), character_name(2), "Back"});
        menu_.reset();
    }

    // Smoke/screenshot skip the main menu, so they never go through reset();
    // load the first scene's tileset here so the world size is known before we
    // place the player at the world centre.
    if (smoke_test_ || screenshot_mode_) {
        level_ = ((start_level_ % kLevelCount) + kLevelCount) % kLevelCount;
        load_current_tilemap();
    }

    const float px = (smoke_test_ || screenshot_mode_) ? world_w_ * 0.5f
                                                         : static_cast<float>(kViewWidth) * 0.5f;
    const float py = (smoke_test_ || screenshot_mode_) ? world_h_ * 0.5f
                                                         : static_cast<float>(kViewHeight) * 0.5f;
    auto player =
        std::make_unique<Player>(px, py);
    player->add_animation("walk", player_walk_sheet_.get(), 4, 32, 0.12f, true);
    player->add_animation("idle", player_idle_sheet_.get(), 2, 32, 0.28f, true);
    player->play_animation("idle");
    player_ = player.get();
    apply_gun_textures(*player_);
    player_->set_style(character_style(selected_character_));
    if (smoke_test_ || screenshot_mode_) player_->health = 1000;  // survive the whole smoke run
    entities_.push_back(std::move(player));
    update_camera();
    if (smoke_test_ || screenshot_mode_) {
        // These modes skip the menu (and so reset()), so seed the first wave
        // here. It has to be a wave that *belongs* to the starting scene:
        // level_for_wave() drives the scene transition every frame, so opening
        // on --level 4 at wave 1 would immediately drag the world back to the
        // scene wave 1 belongs to.
        GameContext ctx;
        ctx.world_w = world_w_;
        ctx.world_h = world_h_;
        ctx.tilemap = &tilemap_;
        ctx.obstacles = &obstacles_;
        begin_wave(level_ * kWavesPerLevel + 1, ctx);
    }
    return true;
}

void Game::run() {
    if (menu_shot_) {
        // Render the character-select preview screen once and capture it.
        // Capture BEFORE SDL_RenderPresent: after present the back buffer is
        // undefined, so reading it back yields a black frame.
        bool running = true;
        process_input(running);
        const float ww = static_cast<float>(kViewWidth);
        const float wh = static_cast<float>(kViewHeight);
        const Level& lvl = current_level();
        SDL_SetRenderDrawColor(renderer_, lvl.floor.r, lvl.floor.g, lvl.floor.b,
                              SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer_);
        menu_.render(font_, "SELECT CHARACTER", ww, wh);
        const int idx = std::clamp(menu_.selected_index(), 0, 2);
        const float preview_t = static_cast<float>(SDL_GetTicks()) / 1000.0f;
        draw_iso_character(renderer_, ww * 0.5f, wh * 0.82f, 80.0f, 80.0f,
                           0.0f, -1.0f, preview_t * 6.0f,
                           character_style(idx), nullptr, 0.0f);
        capture_screenshot_to(screenshot_path_);
        SDL_RenderPresent(renderer_);
        return;
    }
    if (smoke_test_ || screenshot_mode_) {
        // Drive a short fixed-step scenario so combat, enemy drops, and item
        // pickups are all exercised. In screenshot mode this runs under a real
        // renderer (e.g. Xvfb) so the framebuffer can be captured.
        bool running = true;
        const float dt = 1.0f / 60.0f;
        const int frames = screenshot_mode_
                               ? 220
                               : (smoke_frames_ > 0 ? smoke_frames_ : 900);
        for (int i = 0; i < frames && running; ++i) {
            process_input(running);
            update(dt);
            // Capture requested frames via the pre-present path (reliable):
            // set pending_capture_ before render() so render() reads the back
            // buffer right before SDL_RenderPresent.
            if (screenshot_mode_ &&
                (i == 90 || i == 110 || i == 160 || i == 185 ||
                 i == 32 || i == 34 || i == 36 || i == 38 ||
                 i == 52 || i == 60 || i == 66 || i == 74 ||
                 i == 20 || i == 26 || i == 205)) {
                pending_capture_ =
                    screenshot_path_ + "." + std::to_string(i) + ".bmp";
            }
            render();
            // For verification: at frame 30 force a Devil just within fire
            // range of the player so the ranged fireball attack can be
            // captured in flight.
            if (screenshot_mode_ && i == 30 && player_) {
                const float dx = 40.0f;  // within kDevilFireRange (50), clear sight
                auto d = std::make_unique<Enemy>(
                    EnemyKind::Devil, player_->pos.x + dx, player_->pos.y);
                d->add_animation("walk", devil_walk_sheet_.get(), 4, 40, 0.14f, true);
                d->play_animation("walk");
                entities_.push_back(std::move(d));
            }
            // For verification: at frame 45 shoot out the barrel nearest the
            // player so the fuse flash, the blast, and any chain reaction can
            // be captured (the fuse burns ~13 frames, the fireball ~27).
            if (screenshot_mode_ && i == 45 && player_) {
                Barrel* nearest = nullptr;
                float best = 1e30f;
                for (auto& e : entities_) {
                    auto* b = dynamic_cast<Barrel*>(e.get());
                    if (!b || !b->alive) continue;
                    const float dx = b->pos.x - player_->pos.x;
                    const float dy = b->pos.y - player_->pos.y;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 < best) {
                        best = d2;
                        nearest = b;
                    }
                }
                if (nearest) nearest->hit(Barrel::kMaxHealth);
            }
            // For verification: hand the player a shotgun early so the aim
            // overlay's spread cone is visible in the captures.
            if (screenshot_mode_ && i == 8 && player_) {
                player_->acquire_weapon(WeaponKind::Shotgun,
                                        weapon_spec(WeaponKind::Shotgun).ammo);
                player_->switch_to(WeaponKind::Shotgun);
            }
            // For verification: pause near the end of the run (after every
            // other capture) so the pause overlay lands in a screenshot.
            if (screenshot_mode_ && i == 200) paused_ = true;
            // For verification: force a yellow demon (the every-5th-wave
            // mini-boss) next to the player at frame 12 so its palette and
            // ranged attack can be captured without waiting for wave 5.
            if (screenshot_mode_ && i == 12 && player_) {
                auto d = std::make_unique<Enemy>(
                    EnemyKind::YellowDemon, player_->pos.x - 150.0f,
                    player_->pos.y - 90.0f);
                d->add_animation("walk", devil_walk_sheet_.get(), 4, 40, 0.14f, true);
                d->play_animation("walk");
                entities_.push_back(std::move(d));
            }
            // For verification: force the level boss out at frame 6 so its
            // floating name and the Dark Souls health bar can be captured.
            if (screenshot_mode_ && i == 6) {
                GameContext sctx;
                sctx.world_w = world_w_;
                sctx.world_h = world_h_;
                sctx.tilemap = &tilemap_;
                sctx.obstacles = &obstacles_;
                spawn_boss(sctx);
            }
            // For screenshot verification: force a scene transition at frame
            // 100 so the fade + map swap + banner can be captured (the real
            // game triggers this every 15 waves automatically).
            if (screenshot_mode_ && i == 100) {
                // Move the wave counter with the scene: level_for_wave() drives
                // the transition every frame, so forcing a swap without also
                // advancing the wave leaves the gate disagreeing and the world
                // ping-ponging between the two scenes.
                const int next = (level_ + 1) % kLevelCount;
                GameContext sctx;
                sctx.world_w = world_w_;
                sctx.world_h = world_h_;
                sctx.tilemap = &tilemap_;
                sctx.obstacles = &obstacles_;
                begin_wave(next * kWavesPerLevel + 1, sctx);
                begin_level_transition(next);
            }
        }
        if (screenshot_mode_) {
            // Final capture already handled per-frame above via pre-present;
            // nothing extra to do (a trailing render() here crashes on some
            // X11 drivers after the scene has torn down).
        } else {
            std::cerr << "smoke: kills=" << score_ << " wave=" << wave_
                      << " hp=" << player_->health
                      << " weapon=" << player_->weapon_name()
                      << " pickups=" << pickups_collected_;
            // Report the highest enemy animation frame seen during the run to
            // prove the animator ticked (robust even if all enemies die late).
            std::cerr << " enemy_frame=" << max_enemy_anim_frame_
                      << " barrels=" << barrels_exploded_
                      << " blast_kills=" << blast_kills_
                      << " demons=" << demons_spawned_
                      << " bosses=" << bosses_spawned_
                      << " blasts=" << projectile_blasts_
                      << " stunned=" << enemies_stunned_
                      << " lured=" << enemies_lured_
                      << " wave_gate="
                      << (wave_gate_violations_ == 0 ? "OK" : "LEAKED")
                      << '\n';
        }
        return;
    }

    Uint64 last_time = SDL_GetTicksNS();
    bool running = true;
    while (running) {
        const Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - last_time) / 1.0e9f;
        last_time = now;
        if (dt > 0.25f) dt = 0.25f;  // avoid spiral of death after stalls

        process_input(running);
        update(dt);
        render();

        SDL_Delay(1);  // yield CPU
    }
}

void Game::process_input(bool& running) {
    // Logical view size for menu/HUD layout (window may be resized/fullscreen).
    const float ww = static_cast<float>(kViewWidth);
    const float wh = static_cast<float>(kViewHeight);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
            return;
        }
        // Global: F11 toggles fullscreen (borderless) on any screen. The
        // logical presentation keeps the 1280x720 view scaled to fit.
        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.key == SDLK_F11) {
            const uint32_t current = SDL_GetWindowFlags(window_);
            const bool fs = (current & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(window_, !fs);
            continue;
        }
        switch (state_) {
            case GameState::MainMenu: {
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    running = false;
                    break;
                }
                int c = menu_.handle_event(event, ww, wh);
                if (c >= 0) on_main_menu_select(c, running);
                break;
            }
            case GameState::CharacterSelect: {
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    return_to_menu();
                    break;
                }
                int c = menu_.handle_event(event, ww, wh);
                if (c >= 0) on_character_select(c);
                break;
            }
            case GameState::Options: {
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    return_to_menu();
                    break;
                }
                int c = menu_.handle_event(event, ww, wh);
                if (c >= 0) on_options_select(c);
                break;
            }
            case GameState::Playing:
                if (event.type == SDL_EVENT_KEY_DOWN &&
                    event.key.key == SDLK_ESCAPE) {
                    return_to_menu();
                    break;
                }
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    const SDL_Keycode k = event.key.key;
                    if (k == SDLK_P) paused_ = !paused_;
                    // Number keys select inventory slots in enum order.
                    else if (k >= SDLK_1 &&
                             k < SDLK_1 + Player::kSlotCount) {
                        player_->switch_to(
                            static_cast<WeaponKind>(k - SDLK_1));
                    }
                    else if (k == SDLK_Q) player_->cycle(-1);
                    else if (k == SDLK_E) player_->cycle(1);
                }
                break;
            case GameState::GameOver:
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_ESCAPE) {
                        return_to_menu();
                    } else if (event.key.key == SDLK_R) {
                        reset();
                    }
                }
                break;
        }
    }
}

void Game::on_main_menu_select(int index, bool& running) {
    if (index == 0) {
        enter_character_select();  // New Game -> pick a character
    } else if (index == 1) {
        enter_options();
    } else if (index == 2) {
        running = false;  // Exit
    }
}

void Game::enter_character_select() {
    state_ = GameState::CharacterSelect;
    menu_.set_items({character_name(0), character_name(1), character_name(2), "Back"});
    menu_.reset();
}

void Game::on_character_select(int index) {
    if (index == 3) {
        return_to_menu();
        return;
    }
    selected_character_ = std::clamp(index, 0, 2);
    reset();  // spawn into gameplay with the chosen character
}

IsoCharStyle Game::character_style(int index) {
    // Three playable survivors sharing the Boxhead build — tan skin, black
    // hair, denim legs, black boots — separated by shirt color.
    IsoCharStyle s;
    s.skin = SDL_Color{236, 196, 152, 255};
    s.hair = SDL_Color{34, 28, 30, 255};
    s.pants = SDL_Color{54, 62, 92, 255};
    s.shoe = SDL_Color{30, 28, 32, 255};
    s.eye = SDL_Color{28, 24, 26, 255};
    switch (index) {
        case 1:  // Green ranger
            s.shirt = SDL_Color{62, 148, 74, 255};
            s.pants = SDL_Color{58, 66, 52, 255};
            break;
        case 2:  // Red brawler
            s.shirt = SDL_Color{198, 58, 52, 255};
            s.pants = SDL_Color{66, 52, 52, 255};
            break;
        default:  // Blue survivor
            s.shirt = SDL_Color{58, 116, 196, 255};
            break;
    }
    return s;
}

const char* Game::character_name(int index) {
    switch (index) {
        case 1: return "Green Ranger";
        case 2: return "Red Brawler";
        default: return "Blue Survivor";
    }
}

void Game::enter_options() {
    state_ = GameState::Options;
    menu_.set_items({"Difficulty: " + difficulty_label(),
                     "Aim: " + aim_label(),
                     "Back"});
    menu_.reset();
}

void Game::on_options_select(int index) {
    if (index == 0) {
        difficulty_ = (difficulty_ + 1) % 3;  // cycle Easy/Normal/Hard
    } else if (index == 1) {
        aim_mode_ = (aim_mode_ == AimMode::FaceMouse ? AimMode::FaceMovement
                                                    : AimMode::FaceMouse);
    } else if (index == 2) {
        return_to_menu();
    }
    if (index == 0 || index == 1) {
        menu_.set_items({"Difficulty: " + difficulty_label(),
                         "Aim: " + aim_label(),
                         "Back"});
    }
}

void Game::return_to_menu() {
    state_ = GameState::MainMenu;
    paused_ = false;
    menu_.set_items(kMainMenuItems);
    menu_.reset();
}

std::string Game::difficulty_label() {
    switch (difficulty_) {
        case 0: return "Easy";
        case 2: return "Hard";
        default: return "Normal";
    }
}

std::string Game::aim_label() {
    return aim_mode_ == AimMode::FaceMouse ? "Mouse" : "Movement";
}

float Game::difficulty_factor() {
    switch (difficulty_) {
        case 0: return 1.4f;  // Easy: slower spawns
        case 2: return 0.7f;  // Hard: faster spawns
        default: return 1.0f;
    }
}

void Game::update_player_aim(const GameContext& ctx) {
    float dx = 0.0f;
    float dy = -1.0f;
    if (smoke_test_ || screenshot_mode_) {
        bool found = false;
        float best = 1e30f;
        for (const auto& e : entities_) {
            auto* en = dynamic_cast<Enemy*>(e.get());
            if (!en || !en->alive) continue;
            const float ax = en->pos.x - player_->pos.x;
            const float ay = en->pos.y - player_->pos.y;
            const float dd = ax * ax + ay * ay;
            if (dd < best) {
                best = dd;
                dx = ax;
                dy = ay;
                found = true;
            }
        }
        const float l = std::sqrt(dx * dx + dy * dy);
        if (found && l > 0.001f) {
            dx /= l;
            dy /= l;
        } else {
            dx = 0.0f;
            dy = -1.0f;
        }
    } else if (aim_mode_ == AimMode::FaceMovement) {
        // Aim where the player is heading (last non-zero movement dir).
        dx = player_->last_move_dir().x;
        dy = player_->last_move_dir().y;
        const float l = std::sqrt(dx * dx + dy * dy);
        if (l < 0.001f) {
            dx = 0.0f;
            dy = -1.0f;
        } else {
            dx /= l;
            dy /= l;
        }
    } else {
        // FaceMouse: aim from the player toward the cursor. Convert the window
        // mouse position into logical render coords (handles letterbox scaling
        // from SDL_SetRenderLogicalPresentation), then to world coords by
        // adding the camera offset.
        float mx = 0.0f;
        float my = 0.0f;
        SDL_GetMouseState(&mx, &my);
        float lx = 0.0f;
        float ly = 0.0f;
        SDL_RenderCoordinatesFromWindow(renderer_, mx, my, &lx, &ly);
        cursor_view_ = Vec2{lx, ly};
        const float world_mx = lx + camera_.x;
        const float world_my = ly + camera_.y;
        dx = world_mx - player_->pos.x;
        dy = world_my - player_->pos.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) {
            dx = 0.0f;
            dy = -1.0f;
        } else {
            dx /= len;
            dy /= len;
        }
    }
    player_->set_facing(dx, dy);
    if (smoke_test_ || screenshot_mode_) {
        // No real pointer in the headless modes; park the crosshair on the aim
        // line so screenshots show it where a player's mouse would be.
        cursor_view_ = Vec2{player_->pos.x - camera_.x + dx * 220.0f,
                            player_->pos.y - camera_.y + dy * 220.0f};
    }
}

void Game::update(float dt) {
    if (state_ != GameState::Playing) return;
    if (paused_) return;  // frozen: no movement, spawning, firing or timers

    GameContext ctx;
    ctx.keys = SDL_GetKeyboardState(nullptr);
    // World size comes from the loaded map (not the window), so maps larger
    // than the viewport scroll under the camera.
    ctx.world_w = world_w_;
    ctx.world_h = world_h_;
    ctx.tilemap = &tilemap_;
    // Living barrels block movement like walls; refresh the list before
    // anything moves this frame.
    rebuild_obstacles();
    ctx.obstacles = &obstacles_;

    // Update the player first so enemies can chase its new position.
    player_->update(dt, ctx);
    // Scroll the camera to follow the player, clamped to the world bounds.
    update_camera();
    ctx.player_pos = player_->pos;

    // Compute the player's aim direction once: the gun render and the firing
    // path both read this stored vector, so they can never diverge.
    update_player_aim(ctx);

    // When the player is dead, freeze the world (no spawns, movement, etc.).
    if (game_over_) return;

    // Tick a level transition (fade to black, swap map at the midpoint, fade
    // back in). While fading we hold spawns so the new scene starts clean.
    const bool transitioning = transition_timer_ > 0.0f;
    if (transitioning) {
        const float before = transition_timer_;
        transition_timer_ = std::max(0.0f, transition_timer_ - dt);
        // Swap at the midpoint (when crossing below half the duration).
        if (pending_level_ >= 0 &&
            before > kTransitionDur * 0.5f &&
            transition_timer_ <= kTransitionDur * 0.5f) {
            apply_level_swap(ctx);
        }
    }
    if (banner_timer_ > 0.0f) banner_timer_ = std::max(0.0f, banner_timer_ - dt);

    // Move to the next scene once the wave count says so. The wave gate below
    // already guarantees a boss is dead before its wave ends, so this can only
    // fire on a clear field; the boss check stays as a guard so a future change
    // to wave pacing cannot resurrect the "map swap deletes the boss" bug.
    const int want_level = level_for_wave(wave_);
    if (want_level != level_ && !transitioning && pending_level_ < 0 &&
        !find_boss()) {
        begin_level_transition(want_level);
    }

    // --- Wave lifecycle ----------------------------------------------------
    // A wave is *cleared*, not timed: it spawns a fixed roster, and the next
    // wave only begins once every enemy from it is dead. Nothing spawns during
    // a scene transition.
    if (wave_banner_timer_ > 0.0f) {
        wave_banner_timer_ = std::max(0.0f, wave_banner_timer_ - dt);
    }
    if (!transitioning) {
        if (wave_break_timer_ > 0.0f) {
            // Breather after a clear, then the next wave rolls in.
            wave_break_timer_ -= dt;
            if (wave_break_timer_ <= 0.0f) {
                wave_break_timer_ = 0.0f;
                begin_wave(wave_ + 1, ctx);
            }
        } else if (wave_spawns_left_ > 0) {
            // Still feeding this wave's roster onto the field. The rate scales
            // with the wave number and difficulty, and respects the concurrent
            // enemy cap.
            const float spawn_interval =
                std::max(0.35f, (kSpawnInterval -
                                 0.08f * static_cast<float>(wave_ - 1)) *
                                difficulty_factor());
            spawn_timer_ += dt;
            if (spawn_timer_ >= spawn_interval) {
                spawn_timer_ = 0.0f;
                if (living_enemies() < static_cast<int>(kMaxEnemies)) {
                    spawn_enemy(ctx);
                    --wave_spawns_left_;
                }
            }
        } else if (living_enemies() == 0) {
            // Roster exhausted and the field is clear: the wave is over.
            show_toast("Wave " + std::to_string(wave_) + " cleared");
            wave_break_timer_ = kWaveBreak;
        }
    }

    fire_projectile(dt, ctx);

    // Update everything except the player (already updated).
    for (auto& e : entities_) {
        if (e.get() != player_) e->update(dt, ctx);
    }

    // Devils within line of sight fire fireballs at the player.
    update_enemy_ranged_attacks(dt, ctx);

    // Barrels whose fuse burned out this frame blow up (and may light other
    // barrels, which detonate on their own fuse a moment later).
    update_barrels();
    // Rockets that hit something and grenades whose fuse ran out.
    update_projectile_blasts();

    // In smoke, record the highest enemy animation frame seen so the summary
    // proves the animator ticked even if every enemy dies before the run ends.
    if (smoke_test_) {
        for (const auto& e : entities_) {
            auto* en = dynamic_cast<Enemy*>(e.get());
            if (en && en->alive) {
                const int f = static_cast<int>(en->animator_frame_index());
                if (f > max_enemy_anim_frame_) max_enemy_anim_frame_ = f;
            }
        }
    }

    // Tick the player's post-hit invulnerability window.
    if (invuln_timer_ > 0.0f) {
        invuln_timer_ -= dt;
        if (invuln_timer_ < 0.0f) invuln_timer_ = 0.0f;
    }

    check_collisions();
    check_item_pickups();

    // In smoke, force items onto the player periodically so the pickup path
    // (health + weapons) is exercised even though the player never moves.
    if (smoke_test_ || screenshot_mode_) {
        ++smoke_frame_;
        if (smoke_frame_ % 90 == 0) spawn_item_on_player();
    }

    // Occasionally drop a fresh item on the floor.
    item_spawn_timer_ += dt;
    if (item_spawn_timer_ >= kItemSpawnInterval) {
        item_spawn_timer_ = 0.0f;
        spawn_floor_item(ctx);
    }

    // Fade the pickup toast.
    if (pickup_toast_timer_ > 0.0f) {
        pickup_toast_timer_ -= dt;
        if (pickup_toast_timer_ < 0.0f) pickup_toast_timer_ = 0.0f;
    }

    // Reap dead entities (projectiles that hit/expired, dead enemies, used
    // items). The player is kept even when dead: player_ points into this
    // vector and the HUD/game-over screen still reads it.
    entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                   [this](const std::unique_ptr<Entity>& e) {
                                       return !e->alive && e.get() != player_;
                                   }),
                    entities_.end());

    if (game_over_) state_ = GameState::GameOver;
}

void Game::pick_spawn_point(const GameContext& ctx, float& out_x,
                            float& out_y) {
    // Spawn just inside the wall border (the border tiles are solid, so an
    // enemy spawning at the true edge would be trapped inside the wall and
    // pile up stuck). On a large world, spawn near the current camera view
    // edges (not the far world edges) so enemies approach the player from the
    // sides of the visible play area, Boxhead-style.
    const float wall = 32.0f;           // wall border thickness (one tile)
    const float margin = wall + 16.0f;  // spawn band just inside the wall
    const float vx0 = camera_.x;
    const float vy0 = camera_.y;
    const float vx1 = camera_.x + static_cast<float>(kViewWidth);
    const float vy1 = camera_.y + static_cast<float>(kViewHeight);
    float x = 0.0f;
    float y = 0.0f;
    const int edge = std::rand() % 4;
    const auto lo = [&](float v, float lo0) { return std::max(lo0, v); };
    const auto hi = [&](float v, float hi0) { return std::min(hi0, v); };
    const float mx_lo = margin;
    const float mx_hi = std::max(margin + 1.0f, ctx.world_w - margin);
    const float my_lo = margin;
    const float my_hi = std::max(margin + 1.0f, ctx.world_h - margin);
    for (int attempt = 0; attempt < 8; ++attempt) {
        switch (edge) {
            case 0: {  // top edge of the view
                x = lo(vx0 + std::rand() % std::max(1, static_cast<int>(vx1 - vx0)), mx_lo);
                y = lo(vy0 + margin, my_lo);
                break;
            }
            case 1: {  // bottom edge of the view
                x = lo(vx0 + std::rand() % std::max(1, static_cast<int>(vx1 - vx0)), mx_lo);
                y = hi(vy1 - margin, my_hi);
                break;
            }
            case 2: {  // left edge of the view
                x = lo(vx0 + margin, mx_lo);
                y = lo(vy0 + std::rand() % std::max(1, static_cast<int>(vy1 - vy0)), my_lo);
                break;
            }
            default: {  // right edge of the view
                x = hi(vx1 - margin, mx_hi);
                y = lo(vy0 + std::rand() % std::max(1, static_cast<int>(vy1 - vy0)), my_lo);
                break;
            }
        }
        if (!ctx.blocked(x, y)) break;
        // landed on a wall tile or a barrel: loop and try a different
        // position along the same edge.
    }
    out_x = x;
    out_y = y;
}

void Game::spawn_enemy(const GameContext& ctx) {
    float x = 0.0f;
    float y = 0.0f;
    pick_spawn_point(ctx, x, y);

    // Pick the enemy kind. Zombies are the default; the tougher red Devil
    // appears from wave 2 onward (25% chance). On every kDemonWaveInterval-th
    // wave each spawn additionally rolls a 1-in-kDemonOdds chance of being a
    // yellow demon, the faster fireball-slinging mini-boss.
    EnemyKind kind = EnemyKind::Zombie;
    if (wave_ % kDemonWaveInterval == 0 && (std::rand() % kDemonOdds) == 0) {
        kind = EnemyKind::YellowDemon;
        ++demons_spawned_;
    } else if (wave_ >= 2 && (std::rand() % 4) == 0) {
        kind = EnemyKind::Devil;
    }
    auto e = std::make_unique<Enemy>(kind, x, y);
    const Texture* sheet = (kind == EnemyKind::Zombie) ? zombie_walk_sheet_.get()
                                                       : devil_walk_sheet_.get();
    e->add_animation("walk", const_cast<Texture*>(sheet), 4, 40, 0.14f, true);
    e->play_animation("walk");
    entities_.push_back(std::move(e));
}

const char* Game::boss_name(int level) {
    // One named boss per scene, cycling with the levels.
    static const char* kNames[] = {
        "Grimthar, Warden of the Courtyard",
        "Malkyra, Matron of the Asylum",
        "Ghulmaw, the Sunken Glutton",
        "Vaskel, Keeper of Bones",
        "Ashmodai, the Gate Unbarred",
    };
    const int n = static_cast<int>(sizeof(kNames) / sizeof(kNames[0]));
    return kNames[((level % n) + n) % n];
}

Enemy* Game::find_boss() const {
    for (const auto& e : entities_) {
        auto* en = dynamic_cast<Enemy*>(e.get());
        if (en && en->alive && en->is_boss()) return en;
    }
    return nullptr;
}

void Game::spawn_boss(const GameContext& ctx) {
    if (find_boss()) return;  // one boss at a time
    float x = 0.0f;
    float y = 0.0f;
    pick_spawn_point(ctx, x, y);
    auto boss = std::make_unique<Enemy>(EnemyKind::Boss, x, y);
    boss->set_name(boss_name(level_));
    boss->add_animation("walk", devil_walk_sheet_.get(), 4, 40, 0.18f, true);
    boss->play_animation("walk");
    show_toast(std::string(boss->name()) + " awakens");
    entities_.push_back(std::move(boss));
    ++bosses_spawned_;
}

int Game::enemies_for_wave(int wave) {
    const int n = kWaveBaseEnemies + kWaveEnemyStep * (wave - 1);
    return std::min(kWaveEnemyCap, std::max(1, n));
}

int Game::living_enemies() const {
    int n = 0;
    for (const auto& e : entities_) {
        const auto* en = dynamic_cast<const Enemy*>(e.get());
        if (en && en->alive) ++n;
    }
    return n;
}

int Game::wave_enemies_left() const {
    return wave_spawns_left_ + living_enemies();
}

void Game::begin_wave(int wave, const GameContext& ctx) {
    // The field must be clear before a wave starts; anything else means the
    // clear-gate leaked.
    if (living_enemies() != 0) ++wave_gate_violations_;
    wave_ = wave;
    wave_spawns_left_ = enemies_for_wave(wave_);
    spawn_timer_ = 0.0f;
    wave_banner_timer_ = 1.8f;
    // The last wave of every scene is a boss wave. The boss is extra to the
    // roster, and because the wave cannot end until the field is clear, it
    // must be killed before the next wave (and the next scene) arrives.
    if (wave_ % kWavesPerLevel == 0) spawn_boss(ctx);
}

void Game::fire_projectile(float dt, const GameContext& ctx) {
    fire_cooldown_ -= dt;
    bool want_fire =
        (ctx.keys && ctx.keys[SDL_SCANCODE_SPACE]) ||
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);
    // Smoke-test auto-fire so combat + pickups are exercised headlessly.
    if ((smoke_test_ || screenshot_mode_) && !want_fire) want_fire = true;
    if (!want_fire || fire_cooldown_ > 0.0f) return;

    // Use the player's current weapon profile (one firing path, no switches).
    const WeaponSpec w = player_->current_weapon();
    if (!player_->consume_ammo()) return;
    fire_cooldown_ = w.cooldown;

    // Aim direction comes from the shared aim state computed in update()
    // (smoke targets the nearest enemy; otherwise mouse or movement).
    const Vec2 aim = player_->facing();
    const float dx = aim.x;
    const float dy = aim.y;

    // Emit N projectiles spread across the fire cone.
    const int n = w.projectile_count;
    const float spread_rad = w.spread_degrees * (static_cast<float>(M_PI) / 180.0f);
    for (int i = 0; i < n; ++i) {
        float angle = 0.0f;
        if (n > 1) angle = -spread_rad * 0.5f + spread_rad * (i / (n - 1.0f));
        const float ca = std::cos(angle);
        const float sa = std::sin(angle);
        const float dirx = dx * ca - dy * sa;
        const float diry = dx * sa + dy * ca;
        auto proj = std::make_unique<Projectile>(
            player_->pos.x, player_->pos.y,
            dirx * w.projectile_speed, diry * w.projectile_speed, w.damage);
        proj->set_texture(projectile_tex_for(w.kind));
        if (w.blast_radius > 0.0f) {
            Payload load;
            load.blast_radius = w.blast_radius;
            load.blast_damage = w.blast_damage;
            load.stun_seconds = w.stun_seconds;
            load.lure_radius = w.lure_radius;
            load.lure_seconds = w.lure_seconds;
            proj->arm(load, w.fuse);
        }
        entities_.push_back(std::move(proj));
    }
}

void Game::check_collisions() {
    // Projectile vs barrel: any bullet (the player's or a devil's fireball)
    // stops on a barrel and damages it. Enough hits light its fuse.
    for (auto& a : entities_) {
        auto* proj = dynamic_cast<Projectile*>(a.get());
        if (!proj || !proj->alive) continue;
        for (auto& b : entities_) {
            auto* barrel = dynamic_cast<Barrel*>(b.get());
            if (!barrel || !barrel->alive) continue;
            if (entities_overlap(*proj, *barrel)) {
                barrel->hit(std::max(1, proj->damage_amount));
                proj->on_hit();  // rockets go off against the barrel
                break;
            }
        }
    }

    // Projectile vs enemy: a player-fired (non-hostile) projectile is
    // destroyed and the enemy takes hit damage from the weapon. Dead enemies
    // may drop an item. Hostile (devil) fireballs ignore enemies.
    // Drops are queued rather than spawned inline: maybe_drop_item() pushes
    // into entities_, which would invalidate the loops walking it.
    std::vector<Vec2> kill_drops;
    for (auto& a : entities_) {
        auto* proj = dynamic_cast<Projectile*>(a.get());
        if (!proj || !proj->alive || proj->hostile) continue;
        for (auto& b : entities_) {
            auto* en = dynamic_cast<Enemy*>(b.get());
            if (!en || !en->alive) continue;
            if (entities_overlap(*proj, *en)) {
                // A thrown charge bounces off bodies and keeps its fuse; a
                // bullet or rocket ends here.
                if (proj->thrown()) continue;
                const Vec2 drop_pos = en->pos;
                if (proj->damage_amount > 0) en->damage(proj->damage_amount);
                proj->on_hit();
                if (!en->alive) {
                    ++score_;  // count the kill
                    kill_drops.push_back(drop_pos);
                }
                break;
            }
        }
    }
    for (const Vec2& p : kill_drops) maybe_drop_item(p);

    // Hostile projectile vs player: a devil fireball that hits the player deals
    // contact damage on the same invulnerability window as melee.
    if (invuln_timer_ <= 0.0f && player_->alive) {
        for (auto& a : entities_) {
            auto* proj = dynamic_cast<Projectile*>(a.get());
            if (!proj || !proj->alive || !proj->hostile) continue;
            if (entities_overlap(*proj, *player_)) {
                proj->on_hit();
                player_->damage(proj->damage_amount);
                invuln_timer_ = kInvulnDuration;
                if (!player_->alive) game_over_ = true;
                break;
            }
        }
    }

    // Enemy vs player: contact damage on a short cooldown.
    if (invuln_timer_ <= 0.0f && player_->alive) {
        for (auto& e : entities_) {
            auto* en = dynamic_cast<Enemy*>(e.get());
            if (!en || !en->alive) continue;
            if (entities_overlap(*en, *player_)) {
                player_->damage(1);
                invuln_timer_ = kInvulnDuration;
                if (!player_->alive) game_over_ = true;
                break;
            }
        }
    }
}

void Game::check_item_pickups() {
    if (!player_->alive) return;
    for (auto& e : entities_) {
        auto* item = dynamic_cast<Item*>(e.get());
        if (!item || !item->alive) continue;
        if (entities_overlap(*item, *player_)) {
            item->on_pickup(*this, *player_);
            item->alive = false;
            ++pickups_collected_;
        }
    }
}

void Game::maybe_drop_item(Vec2 pos) {
    const int r = std::rand() % 100;
    if (r < 15) {
        auto it = std::make_unique<HealthPickup>(pos.x, pos.y);
        it->set_texture(health_tex_.get());
        entities_.push_back(std::move(it));
    } else if (r < 25) {
        // Only drop real upgrades, never the default pistol.
        const WeaponKind k = random_weapon_drop();
        const int ammo = weapon_spec(k).ammo;
        auto it = std::make_unique<WeaponPickup>(pos.x, pos.y, k, ammo);
        it->set_texture(weapon_pickup_tex(k));
        entities_.push_back(std::move(it));
    }
}

void Game::spawn_floor_item(const GameContext& ctx) {
    int count = 0;
    for (const auto& e : entities_)
        if (dynamic_cast<Item*>(e.get())) ++count;
    if (count >= kMaxFloorItems) return;

    // Spawn floor items within the current camera view (plus a margin) so
    // pickups appear near the player on large worlds, not at the far edges.
    const float vx0 = camera_.x, vy0 = camera_.y;
    const float vx1 = camera_.x + static_cast<float>(kViewWidth);
    const float vy1 = camera_.y + static_cast<float>(kViewHeight);
    const float x = std::clamp(vx0 + 60.0f +
                               static_cast<float>(std::rand() % std::max(1, static_cast<int>(vx1 - vx0 - 120.0f))),
                               60.0f, std::max(61.0f, ctx.world_w - 60.0f));
    const float y = std::clamp(vy0 + 60.0f +
                               static_cast<float>(std::rand() % std::max(1, static_cast<int>(vy1 - vy0 - 120.0f))),
                               60.0f, std::max(61.0f, ctx.world_h - 60.0f));
    if (std::rand() % 2 == 0) {
        auto it = std::make_unique<HealthPickup>(x, y);
        it->set_texture(health_tex_.get());
        entities_.push_back(std::move(it));
    } else {
        const WeaponKind k = random_weapon_drop();
        const int ammo = weapon_spec(k).ammo;
        auto it = std::make_unique<WeaponPickup>(x, y, k, ammo);
        it->set_texture(weapon_pickup_tex(k));
        entities_.push_back(std::move(it));
    }
}

void Game::spawn_item_on_player() {
    // Alternate health and weapon pickups directly on the player.
    if (smoke_frame_ % 180 == 0) {
        auto it = std::make_unique<HealthPickup>(player_->pos.x, player_->pos.y);
        it->set_texture(health_tex_.get());
        entities_.push_back(std::move(it));
    } else {
        // Cycle deterministically through every weapon but the pistol rather
        // than rolling the drop table: the point of this path is that a
        // headless run exercises *all* of them, including the grenades whose
        // stun and lure the summary reports on.
        static int next_kind = 1;
        const WeaponKind k = static_cast<WeaponKind>(next_kind);
        next_kind = next_kind + 1;
        if (next_kind >= Player::kSlotCount) next_kind = 1;
        const int ammo = weapon_spec(k).ammo;
        auto it =
            std::make_unique<WeaponPickup>(player_->pos.x, player_->pos.y,
                                           k, ammo);
        it->set_texture(weapon_pickup_tex(k));
        entities_.push_back(std::move(it));
    }
}

Texture* Game::weapon_pickup_tex(WeaponKind k) {
    switch (k) {
        case WeaponKind::Shotgun: return weapon_tex_shotgun_.get();
        case WeaponKind::MachineGun: return weapon_tex_machinegun_.get();
        case WeaponKind::RocketLauncher: return rocket_tex_.get();
        case WeaponKind::Grenade: return grenade_tex_.get();
        case WeaponKind::Concussion: return concussion_tex_.get();
        case WeaponKind::Lure: return lure_tex_.get();
        case WeaponKind::Pistol:
        default: return weapon_tex_pistol_.get();
    }
}

void Game::render() {
    // Logical view size — the window may be resized/fullscreen, but gameplay
    // coordinates stay fixed (SDL scales the view to the window). Use these
    // for all screen-space layout (menu, HUD, banners).
    const float ww = static_cast<float>(kViewWidth);
    const float wh = static_cast<float>(kViewHeight);

    const Level& lvl = current_level();
    SDL_SetRenderDrawColor(renderer_, lvl.floor.r, lvl.floor.g, lvl.floor.b,
                          SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer_);

    // Draw the scene tileset (LevelEdit++ ".mx"). On load failure the floor
    // stays the solid fallback color above. Offset by the camera so a world
    // larger than the viewport scrolls.
    if (tilemap_.loaded()) {
        // In the world, only the floor goes down here: raised tiles are drawn
        // further below, interleaved with the entities by depth so a wall can
        // cover whatever is standing behind it. The menu screens have no
        // entities to interleave with, so they take the whole tilemap at once.
        const bool in_world = state_ != GameState::MainMenu &&
                              state_ != GameState::CharacterSelect &&
                              state_ != GameState::Options;
        if (in_world) {
            tilemap_.render_floor(renderer_, camera_.x, camera_.y, ww, wh);
        } else {
            tilemap_.render(renderer_, camera_.x, camera_.y, ww, wh);
        }
    }

    if (state_ == GameState::MainMenu) {
        menu_.render(font_, "BOXDEAD", ww, wh);
        SDL_RenderPresent(renderer_);
        return;
    }
    if (state_ == GameState::CharacterSelect) {
        menu_.render(font_, "SELECT CHARACTER", ww, wh);
        // Live preview of the highlighted character, idle-walking in place.
        // Placed below the menu list so it never overlaps the items.
        const int idx = std::clamp(menu_.selected_index(), 0, 2);
        const float preview_t = static_cast<float>(SDL_GetTicks()) / 1000.0f;
        draw_iso_character(renderer_, ww * 0.5f, wh * 0.82f, 80.0f, 80.0f,
                           0.0f, -1.0f, preview_t * 6.0f,
                           character_style(idx), nullptr, 0.0f);
        SDL_RenderPresent(renderer_);
        return;
    }
    if (state_ == GameState::Options) {
        menu_.render(font_, "OPTIONS", ww, wh);
        SDL_RenderPresent(renderer_);
        return;
    }

    // Playing / GameOver: world + HUD.
    const bool flashing = invuln_timer_ > 0.0f &&
                          static_cast<int>(invuln_timer_ * 10.0f) % 2 == 0;
    if (flashing) player_->set_color_override(SDL_Color{220, 60, 60, 255});

    // Draw entities back-to-front by ground position so closer (lower)
    // characters correctly overlap further (higher) ones in the iso view.
    std::vector<Entity*> order;
    order.reserve(entities_.size());
    for (const auto& e : entities_) order.push_back(e.get());
    std::sort(order.begin(), order.end(), [](const Entity* a, const Entity* b) {
        return (a->pos.y + a->size.y * 0.5f) < (b->pos.y + b->size.y * 0.5f);
    });
    // Walk the raised tiles and the entities together, always drawing whichever
    // sits further back. That is what puts a character behind a wall *behind*
    // it, while one standing in front of the same wall is drawn over it.
    int next_tile = 0;
    const int raised = tilemap_.loaded() ? tilemap_.raised_count() : 0;

    for (const auto* e : order) {
        const float entity_ground = e->pos.y + e->size.y * 0.5f;
        while (next_tile < raised &&
               tilemap_.raised_ground_line(next_tile) <= entity_ground) {
            tilemap_.render_raised(renderer_, next_tile, camera_.x, camera_.y,
                                   ww, wh);
            ++next_tile;
        }
        e->render(renderer_, camera_.x, camera_.y);
    }

    // Whatever is left stands in front of every entity on screen.
    while (next_tile < raised) {
        tilemap_.render_raised(renderer_, next_tile, camera_.x, camera_.y, ww,
                               wh);
        ++next_tile;
    }

    // The boss carries its name over its head, in world space.
    if (const Enemy* boss = find_boss()) {
        if (!boss->name().empty()) {
            const float tw = static_cast<float>(font_.text_width(boss->name()));
            // Centred over the boss, but kept inside the viewport so the name
            // stays readable when it is fighting at the edge of the screen.
            const float nx = std::clamp(boss->pos.x - camera_.x - tw * 0.5f,
                                        8.0f, ww - tw - 8.0f);
            font_.draw(boss->name(), nx,
                       boss->pos.y - camera_.y - boss->size.y * 1.15f - 26.0f,
                       SDL_Color{236, 214, 160, 255});
        }
    }

    if (flashing) player_->set_color_override(SDL_Color{255, 255, 255, 255});

    // Aim ray + crosshair sit above the world but below the HUD, and are
    // pointless while the world is frozen or the run is over.
    if (state_ == GameState::Playing && !paused_) render_aim_overlay();

    render_hud();

    // Wave announcement, just under the level banner slot.
    if (wave_banner_timer_ > 0.0f) {
        const std::string label = "WAVE " + std::to_string(wave_);
        const float tw = static_cast<float>(font_.text_width(label));
        const float alpha = std::min(1.0f, wave_banner_timer_ * 2.0f) * 255.0f;
        font_.draw(label, (ww - tw) * 0.5f, wh * 0.30f,
                   SDL_Color{235, 225, 200, static_cast<Uint8>(alpha)});
    }

    // Level banner (shown briefly after entering a scene).
    if (banner_timer_ > 0.0f) {
        const std::string label = std::string(current_level().name) +
                                  "  -  Level " + std::to_string(level_ + 1);
        const float tw = static_cast<float>(font_.text_width(label));
        const float x = (ww - tw) * 0.5f;
        const float y = wh * 0.22f;
        const float alpha =
            std::min(1.0f, banner_timer_ * 1.5f) * 255.0f;  // fades out at the end
        font_.draw(label, x, y,
                   SDL_Color{235, 235, 235, static_cast<Uint8>(alpha)});
    }

    // Pause overlay, over the world and the HUD.
    if (paused_) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        // Light enough that the frozen scene still reads behind it.
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 105);
        SDL_RenderFillRect(renderer_, nullptr);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        const std::string t1 = "PAUSED";
        const std::string t2 = "P to resume  -  Esc for menu";
        const float w1 = static_cast<float>(font_.text_width(t1));
        const float w2 = static_cast<float>(font_.text_width(t2));
        font_.draw(t1, (ww - w1) * 0.5f, wh * 0.44f,
                   SDL_Color{245, 235, 210, 255});
        font_.draw(t2, (ww - w2) * 0.5f, wh * 0.52f,
                   SDL_Color{190, 185, 175, 255});
    }

    // Level-transition fade overlay (covers everything during the swap).
    if (transition_timer_ > 0.0f) {
        // 0 -> midpoint: fade to black; midpoint -> end: fade back in.
        const float t = 1.0f - (transition_timer_ / kTransitionDur);
        const float a = 1.0f - std::abs(2.0f * t - 1.0f);  // triangle, peaks at 0.5
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0,
                               static_cast<Uint8>(a * 255.0f));
        SDL_RenderFillRect(renderer_, nullptr);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    }

    if (!pending_capture_.empty()) {
        capture_screenshot_to(pending_capture_);
        pending_capture_.clear();
    }

    SDL_RenderPresent(renderer_);
}

void Game::render_hud() {
    // Health bar at the top-left.
    constexpr float bar_x = 16.0f;
    constexpr float bar_y = 44.0f;
    constexpr float bar_w = 180.0f;
    constexpr float bar_h = 18.0f;
    const int hp = std::clamp(player_->health, 0, kPlayerMaxHp);
    const float fill_w = bar_w * (static_cast<float>(hp) / kPlayerMaxHp);

    font_.draw("HP", bar_x, 16.0f);

    // Background.
    SDL_SetRenderDrawColor(renderer_, 60, 60, 70, SDL_ALPHA_OPAQUE);
    SDL_FRect bg{bar_x, bar_y, bar_w, bar_h};
    SDL_RenderFillRect(renderer_, &bg);
    // Fill.
    SDL_SetRenderDrawColor(renderer_, 220, 45, 45, SDL_ALPHA_OPAQUE);
    SDL_FRect fill{bar_x, bar_y, fill_w, bar_h};
    SDL_RenderFillRect(renderer_, &fill);
    // Border.
    SDL_SetRenderDrawColor(renderer_, 200, 200, 210, SDL_ALPHA_OPAQUE);
    SDL_FRect border{bar_x, bar_y, bar_w, bar_h};
    SDL_RenderRect(renderer_, &border);

    // Score + wave, top-right.
    font_.draw("Kills: " + std::to_string(score_), 980.0f, 16.0f);
    font_.draw("Wave: " + std::to_string(wave_), 980.0f, 40.0f);
    // Waves end on a clear, so show what is still owed: unspawned roster plus
    // everything still breathing. Without this the pause between waves reads
    // as the game having stopped spawning.
    if (wave_break_timer_ > 0.0f) {
        font_.draw("Cleared!", 1120.0f, 40.0f, SDL_Color{255, 220, 60, 255});
    } else {
        font_.draw("Left: " + std::to_string(wave_enemies_left()), 1120.0f,
                   40.0f, SDL_Color{200, 200, 210, 255});
    }

    // Inventory: one row per owned weapon slot. The selected weapon is
    // highlighted; empty (finite) weapons are dimmed. Keys 1/2/3 select,
    // Q/E cycle.
    // Slot keys are 1..N in enum order.
    float iy = 64.0f;
    for (int i = 0; i < Player::kSlotCount; ++i) {
        const WeaponKind wk = static_cast<WeaponKind>(i);
        if (!player_->owns(wk)) continue;  // hide unowned slots
        const std::string name = weapon_spec(wk).name;
        const int ammo = player_->ammo(wk);
        std::string label = "[" + std::to_string(i + 1) + "] " + name;
        if (ammo < 0) label += " (inf)";
        else if (ammo == 0) label += " (empty)";
        else label += " x" + std::to_string(ammo);
        const bool sel = (wk == player_->current_kind());
        const bool empty = (ammo == 0);
        SDL_Color c = sel ? SDL_Color{255, 220, 60, 255}
                          : (empty ? SDL_Color{110, 110, 120, 255}
                                   : SDL_Color{200, 200, 210, 255});
        if (sel) label = "> " + label;
        font_.draw(label, 980.0f, iy, c);
        iy += 22.0f;
    }

    // Boss name + health bar across the bottom (Dark Souls style).
    render_boss_bar();

    // Pickup toast, centered near the top.
    if (pickup_toast_timer_ > 0.0f && !pickup_toast_.empty()) {
        const int tw = font_.text_width(pickup_toast_);
        font_.draw(pickup_toast_, (1280 - tw) * 0.5f, 110.0f,
                   SDL_Color{255, 230, 120, 255});
    }

    if (game_over_) {
        // Dim the screen.
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 160);
        const SDL_FRect dim{0.0f, 0.0f, 1280.0f, 720.0f};
        SDL_RenderFillRect(renderer_, &dim);
        // GAME OVER + score + restart prompt, centered.
        font_.draw("GAME OVER", 560.0f, 320.0f,
                   SDL_Color{220, 45, 45, 255});
        font_.draw("Kills: " + std::to_string(score_), 568.0f, 360.0f);
        font_.draw("Press R to restart, Esc for menu", 500.0f, 400.0f);
    }
}

void Game::show_toast(const std::string& text) {
    pickup_toast_ = text;
    pickup_toast_timer_ = 1.2f;
}

void Game::reset() {
    entities_.clear();
    spawn_timer_ = 0.0f;
    fire_cooldown_ = 0.0f;
    invuln_timer_ = 0.0f;
    game_over_ = false;
    score_ = 0;
    wave_ = 1;
    wave_spawns_left_ = 0;
    wave_break_timer_ = 0.0f;
    wave_banner_timer_ = 0.0f;
    item_spawn_timer_ = 0.0f;
    pickups_collected_ = 0;
    max_enemy_anim_frame_ = -1;
    barrels_exploded_ = 0;
    blast_kills_ = 0;
    demons_spawned_ = 0;
    bosses_spawned_ = 0;
    wave_gate_violations_ = 0;
    projectile_blasts_ = 0;
    enemies_stunned_ = 0;
    enemies_lured_ = 0;
    pickup_toast_timer_ = 0.0f;
    pickup_toast_.clear();
    paused_ = false;
    state_ = GameState::Playing;

    // Load the scene first so the world size is known, then spawn the player
    // at the world centre (maps can be larger than the viewport).
    level_ = 0;
    transition_timer_ = 0.0f;
    pending_level_ = -1;
    banner_timer_ = 1.6f;  // greet the player with the level name
    load_current_tilemap();

    auto player =
        std::make_unique<Player>(world_w_ * 0.5f, world_h_ * 0.5f);
    player->add_animation("walk", player_walk_sheet_.get(), 4, 32, 0.12f, true);
    player->add_animation("idle", player_idle_sheet_.get(), 2, 32, 0.28f, true);
    player->play_animation("idle");
    player_ = player.get();
    apply_gun_textures(*player_);
    player_->set_style(character_style(selected_character_));
    entities_.push_back(std::move(player));
    update_camera();

    // Roll wave 1 in through the same path every later wave uses.
    GameContext ctx;
    ctx.world_w = world_w_;
    ctx.world_h = world_h_;
    ctx.tilemap = &tilemap_;
    ctx.obstacles = &obstacles_;
    begin_wave(1, ctx);
}

const Game::Level& Game::current_level() const {
    return kLevels[static_cast<size_t>(level_) % kLevelCount];
}

int Game::level_for_wave(int wave) const {
    // Waves are 1-based; every kWavesPerLevel waves advances to the next map.
    return ((wave - 1) / kWavesPerLevel) % kLevelCount;
}

void Game::begin_level_transition(int new_level) {
    pending_level_ = new_level;
    transition_timer_ = kTransitionDur;
}

void Game::apply_level_swap(const GameContext& ctx) {
    level_ = pending_level_;
    pending_level_ = -1;
    banner_timer_ = 2.2f;
    // New map: clear enemies and projectiles, recentre the player, load the
    // new scene's tileset.
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                       [](const std::unique_ptr<Entity>& e) {
                           return dynamic_cast<Enemy*>(e.get()) ||
                                  dynamic_cast<Projectile*>(e.get()) ||
                                  dynamic_cast<Barrel*>(e.get()) ||
                                  dynamic_cast<Explosion*>(e.get());
                       }),
        entities_.end());
    if (player_) {
        // world_w_/h_ still reflect the old map here; load the new tileset
        // first so we can place the player at the new world centre.
        load_current_tilemap();
        player_->pos = {world_w_ * 0.5f, world_h_ * 0.5f};
    } else {
        load_current_tilemap();
    }
    spawn_timer_ = 0.0f;
}

void Game::update_camera() {
    if (!player_) return;
    // Centre the viewport on the player, clamped so the camera never shows
    // past the world edges (no void beyond the map).
    const float max_x = std::max(0.0f, world_w_ - static_cast<float>(kViewWidth));
    const float max_y = std::max(0.0f, world_h_ - static_cast<float>(kViewHeight));
    camera_.x = std::clamp(player_->pos.x - static_cast<float>(kViewWidth) * 0.5f,
                           0.0f, max_x);
    camera_.y = std::clamp(player_->pos.y - static_cast<float>(kViewHeight) * 0.5f,
                           0.0f, max_y);
}

bool Game::line_of_solid_clear(float ax, float ay, float bx, float by) const {
    // March from a to b in small steps; if any sample lands inside a solid
    // tile, the sight line is blocked. (No tilemap -> always clear.)
    const float dx = bx - ax;
    const float dy = by - ay;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return true;
    constexpr float kStep = 8.0f;
    const float nx = dx / dist;
    const float ny = dy / dist;
    const int steps = std::max(1, static_cast<int>(dist / kStep));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float px = ax + nx * dist * t;
        const float py = ay + ny * dist * t;
        if (tilemap_.loaded() && tilemap_.is_solid(px, py)) return false;
    }
    return true;
}

void Game::update_enemy_ranged_attacks(float dt, const GameContext& ctx) {
    (void)ctx;
    if (!player_ || !player_->alive || transition_timer_ > 0.0f) return;
    // Every kind that can shoot lobs fireballs at the player when it has a
    // clear line of sight, using its own range / cadence / volley size: devils
    // spit at point-blank, yellow demons snipe from range, and the boss fans a
    // three-way spread.
    constexpr float kFireballSpeed = 260.0f;
    // Collect new fireballs and add them AFTER the loop: pushing into
    // entities_ while iterating it would invalidate iterators (UB/segfault).
    std::vector<std::unique_ptr<Entity>> spawned;
    for (auto& e : entities_) {
        auto* en = dynamic_cast<Enemy*>(e.get());
        if (!en || !en->alive || !en->can_shoot() || en->stunned()) continue;
        en->tick_ranged_cooldown(dt);
        if (en->ranged_cooldown() > 0.0f) continue;
        const float ddx = player_->pos.x - en->pos.x;
        const float ddy = player_->pos.y - en->pos.y;
        const float dist = std::sqrt(ddx * ddx + ddy * ddy);
        if (dist <= 0.001f || dist > en->fire_range()) continue;
        // Direct sight line: nothing solid between the shooter and the player.
        if (!line_of_solid_clear(en->pos.x, en->pos.y,
                                player_->pos.x, player_->pos.y)) {
            continue;
        }
        const float nx = ddx / dist;
        const float ny = ddy / dist;
        const int n = std::max(1, en->fire_count());
        const float spread =
            en->fire_spread_deg() * (static_cast<float>(M_PI) / 180.0f);
        for (int i = 0; i < n; ++i) {
            float angle = 0.0f;
            if (n > 1) angle = -spread * 0.5f + spread * (i / (n - 1.0f));
            const float ca = std::cos(angle);
            const float sa = std::sin(angle);
            const float dirx = nx * ca - ny * sa;
            const float diry = nx * sa + ny * ca;
            auto fb = std::make_unique<Projectile>(
                en->pos.x, en->pos.y, dirx * kFireballSpeed,
                diry * kFireballSpeed, 1);
            fb->hostile = true;
            fb->set_texture(fireball_tex_.get());
            spawned.push_back(std::move(fb));
        }
        en->reset_ranged_cooldown(en->fire_interval());
    }
    for (auto& s : spawned) entities_.push_back(std::move(s));
}

float Game::ray_distance(float x, float y, float dx, float dy,
                         float max_dist) const {
    if (!tilemap_.loaded()) return max_dist;
    // March in short steps; the tiles are 32px so 6px never skips one.
    constexpr float kStep = 6.0f;
    for (float t = kStep; t <= max_dist; t += kStep) {
        if (tilemap_.is_solid(x + dx * t, y + dy * t)) return t - kStep;
    }
    return max_dist;
}

void Game::render_aim_overlay() {
    if (!player_ || !player_->alive) return;
    const WeaponSpec w = player_->current_weapon();
    const Vec2 aim = player_->facing();

    // Rays start at the gun hand rather than the player's centre so the line
    // leaves the muzzle instead of the character's chest.
    const float ox = player_->pos.x - camera_.x + aim.x * 16.0f;
    const float oy = player_->pos.y - camera_.y + aim.y * 16.0f - 6.0f;
    const float wx = player_->pos.x + aim.x * 16.0f;
    const float wy = player_->pos.y + aim.y * 16.0f;
    constexpr float kMaxRay = 560.0f;

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // One ray down the middle, plus one along each edge of the spread cone so
    // the shotgun visibly fans out and the pistol reads as a single line.
    const float half = w.spread_degrees * 0.5f * (static_cast<float>(M_PI) / 180.0f);
    struct Ray {
        float angle;
        Uint8 alpha;
    };
    const bool spread = (w.projectile_count > 1 && w.spread_degrees > 0.0f);
    const Ray rays[3] = {{0.0f, static_cast<Uint8>(spread ? 110 : 190)},
                        {-half, 200},
                        {half, 200}};
    const int ray_count = spread ? 3 : 1;

    for (int i = 0; i < ray_count; ++i) {
        const float ca = std::cos(rays[i].angle);
        const float sa = std::sin(rays[i].angle);
        const float dx = aim.x * ca - aim.y * sa;
        const float dy = aim.x * sa + aim.y * ca;
        const float len = ray_distance(wx, wy, dx, dy, kMaxRay);
        // Drawn as fading segments so the line reads as a laser sight that
        // trails off rather than a hard-edged stick.
        constexpr int kSegs = 14;
        for (int seg = 0; seg < kSegs; ++seg) {
            const float t0 = len * seg / kSegs;
            const float t1 = len * (seg + 1) / kSegs;
            // Linear falloff: a squared fade made the far half of a 560px
            // ray invisible, which read as the sight being far too short.
            const float fade = 1.0f - 0.75f * static_cast<float>(seg) / kSegs;
            const Uint8 a = static_cast<Uint8>(rays[i].alpha * fade);
            if (a < 6) continue;
            SDL_SetRenderDrawColor(renderer_, 255, 90, 70, a);
            SDL_RenderLine(renderer_, ox + dx * t0, oy + dy * t0,
                          ox + dx * t1, oy + dy * t1);
        }
        // A brighter pip where the ray stops, so a wall hit is obvious.
        if (len < kMaxRay) {
            SDL_SetRenderDrawColor(renderer_, 255, 170, 120, 150);
            const SDL_FRect hit{ox + dx * len - 2.0f, oy + dy * len - 2.0f,
                                4.0f, 4.0f};
            SDL_RenderFillRect(renderer_, &hit);
        }
    }

    // Crosshair, drawn in place of the system cursor (which init() hides).
    const float cx = cursor_view_.x;
    const float cy = cursor_view_.y;
    constexpr float gap = 5.0f;
    constexpr float arm = 11.0f;
    // Dark backing first so the crosshair stays visible on light floors.
    // Three passes: a dark 3px backing so the crosshair survives light floors,
    // then the bright arms drawn 2px thick.
    for (int pass = 0; pass < 3; ++pass) {
        float o = 0.0f;
        if (pass == 0) {
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 190);
            o = 1.0f;
        } else if (pass == 1) {
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 190);
            o = -1.0f;
        } else {
            SDL_SetRenderDrawColor(renderer_, 255, 244, 228, 255);
        }
        SDL_RenderLine(renderer_, cx - gap - arm + o, cy + o, cx - gap + o, cy + o);
        SDL_RenderLine(renderer_, cx + gap + o, cy + o, cx + gap + arm + o, cy + o);
        SDL_RenderLine(renderer_, cx + o, cy - gap - arm + o, cx + o, cy - gap + o);
        SDL_RenderLine(renderer_, cx + o, cy + gap + o, cx + o, cy + gap + arm + o);
    }
    SDL_SetRenderDrawColor(renderer_, 255, 96, 74, 255);
    const SDL_FRect dot{cx - 1.5f, cy - 1.5f, 3.0f, 3.0f};
    SDL_RenderFillRect(renderer_, &dot);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

void Game::render_boss_bar() {
    const Enemy* boss = find_boss();
    if (!boss) return;
    // Dark Souls layout: a long, thin bar low on the screen with the boss's
    // name centred just above it.
    const float ww = static_cast<float>(kViewWidth);
    constexpr float bar_w = 620.0f;
    constexpr float bar_h = 15.0f;
    const float bar_x = (ww - bar_w) * 0.5f;
    constexpr float bar_y = 646.0f;

    const std::string& name = boss->name();
    if (!name.empty()) {
        const float tw = static_cast<float>(font_.text_width(name));
        font_.draw(name, (ww - tw) * 0.5f, bar_y - 30.0f,
                   SDL_Color{218, 206, 178, 255});
    }

    const float frac =
        std::clamp(static_cast<float>(boss->health) /
                       static_cast<float>(std::max(1, boss->max_health())),
                   0.0f, 1.0f);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    // Outer frame, then the dark trough, then the blood-red fill.
    SDL_SetRenderDrawColor(renderer_, 24, 20, 18, 220);
    SDL_FRect frame{bar_x - 3.0f, bar_y - 3.0f, bar_w + 6.0f, bar_h + 6.0f};
    SDL_RenderFillRect(renderer_, &frame);
    SDL_SetRenderDrawColor(renderer_, 52, 44, 38, 255);
    SDL_FRect trough{bar_x, bar_y, bar_w, bar_h};
    SDL_RenderFillRect(renderer_, &trough);
    SDL_SetRenderDrawColor(renderer_, 148, 26, 26, 255);
    SDL_FRect fill{bar_x, bar_y, bar_w * frac, bar_h};
    SDL_RenderFillRect(renderer_, &fill);
    // Highlight along the top of the fill so it reads as lit, not flat.
    SDL_SetRenderDrawColor(renderer_, 196, 60, 48, 255);
    SDL_FRect gloss{bar_x, bar_y, bar_w * frac, 4.0f};
    SDL_RenderFillRect(renderer_, &gloss);
    // Thin gold rule around the trough.
    SDL_SetRenderDrawColor(renderer_, 120, 104, 74, 255);
    SDL_RenderRect(renderer_, &trough);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

void Game::spawn_barrels_from_map() {
    // Every "Barrel"/"Explosive" tile placement in the scene becomes a
    // destructible prop. The tilemap deliberately does not draw or collide
    // those tiles itself, so the Barrel entity is the only thing there.
    for (const Tilemap::Spawn& s : tilemap_.explosive_spawns()) {
        entities_.push_back(std::make_unique<Barrel>(s.cx, s.cy, s.w, s.h));
    }
}

void Game::rebuild_obstacles() {
    obstacles_.clear();
    for (const auto& e : entities_) {
        const auto* b = dynamic_cast<const Barrel*>(e.get());
        if (!b || !b->alive) continue;
        obstacles_.push_back(Obstacle{b->pos, b->size});
    }
}

void Game::update_barrels() {
    // Collect the barrels that reached the end of their fuse, then blow them
    // up outside the loop: detonate() adds entities (fireball, item drops),
    // which would invalidate an iterator over entities_.
    std::vector<Vec2> blasts;
    for (auto& e : entities_) {
        auto* b = dynamic_cast<Barrel*>(e.get());
        if (!b || !b->alive || !b->ready_to_explode()) continue;
        b->alive = false;
        blasts.push_back(b->pos);
    }
    barrels_exploded_ += static_cast<int>(blasts.size());
    for (const Vec2& p : blasts) detonate(p);
}

void Game::detonate(Vec2 pos) {
    explode(pos, Barrel::kBlastRadius, Barrel::kBlastDamage, 0.0f, 0.0f, 0.0f);
}

void Game::explode(Vec2 pos, float radius, int damage, float stun_seconds,
                   float lure_radius, float lure_seconds) {
    constexpr int kPlayerBlastDamage = 1;
    const float r2 = radius * radius;
    const float lure_r2 = lure_radius * lure_radius;

    // Item drops are queued: dropping inside the sweep would push into
    // entities_ while it is being iterated.
    std::vector<Vec2> drops;
    for (auto& e : entities_) {
        if (!e->alive || e.get() == player_) continue;
        const float dx = e->pos.x - pos.x;
        const float dy = e->pos.y - pos.y;
        const float d2 = dx * dx + dy * dy;

        if (auto* en = dynamic_cast<Enemy*>(e.get())) {
            // The lure reaches much further than the blast: it gathers a crowd
            // that the small explosion deliberately does not wipe out.
            if (lure_seconds > 0.0f && d2 <= lure_r2) {
                en->lure_to(pos, lure_seconds);
                ++enemies_lured_;
            }
            if (d2 > r2) continue;
            if (stun_seconds > 0.0f) {
                en->stun(stun_seconds);
                ++enemies_stunned_;
            }
            if (damage > 0) {
                en->damage(damage);
                if (!en->alive) {
                    ++score_;
                    ++blast_kills_;
                    drops.push_back(en->pos);
                }
            }
        } else if (auto* b = dynamic_cast<Barrel*>(e.get())) {
            if (d2 > r2) continue;
            // Chain reaction: a caught barrel lights its own fuse and goes up
            // a fraction of a second later, walking the blast down a row.
            b->hit(Barrel::kMaxHealth);
        }
    }

    // The player is not immune to their own ordnance, but the blast respects
    // the normal i-frame window so a chain never deletes the whole health bar.
    if (player_ && player_->alive && invuln_timer_ <= 0.0f && damage > 0) {
        const float dx = player_->pos.x - pos.x;
        const float dy = player_->pos.y - pos.y;
        if (dx * dx + dy * dy <= r2) {
            player_->damage(kPlayerBlastDamage);
            invuln_timer_ = kInvulnDuration;
            if (!player_->alive) game_over_ = true;
        }
    }

    for (const Vec2& p : drops) maybe_drop_item(p);
    entities_.push_back(std::make_unique<Explosion>(pos.x, pos.y, radius));
}

void Game::update_projectile_blasts() {
    // Same two-phase shape as update_barrels(): collect first, detonate after,
    // because explode() adds entities.
    struct Blast {
        Vec2 pos;
        Payload load;
    };
    std::vector<Blast> blasts;
    for (auto& e : entities_) {
        auto* p = dynamic_cast<Projectile*>(e.get());
        if (!p || !p->alive || !p->blast_pending) continue;
        p->alive = false;
        blasts.push_back({p->pos, p->payload});
    }
    projectile_blasts_ += static_cast<int>(blasts.size());
    for (const Blast& b : blasts) {
        explode(b.pos, b.load.blast_radius, b.load.blast_damage,
                b.load.stun_seconds, b.load.lure_radius, b.load.lure_seconds);
    }
}

void Game::load_current_tilemap() {
    // Load the ".mx" tileset for the current scene; if it fails the floor
    // falls back to the solid level color (render() checks tilemap_.loaded()).
    tilemap_.clear();
    const bool ok = tilemap_.load(renderer_, current_level().map_path);
    // World size = the map's tile bounds, so worlds can be larger than the
    // viewport and scroll under the camera. Fall back to the view size.
    float mw = static_cast<float>(kViewWidth);
    float mh = static_cast<float>(kViewHeight);
    if (ok) tilemap_.world_bounds(mw, mh);
    world_w_ = std::max(mw, static_cast<float>(kViewWidth));
    world_h_ = std::max(mh, static_cast<float>(kViewHeight));
    // Turn the scene's explosive tile placements into Barrel entities.
    spawn_barrels_from_map();
    // Recentre the camera on the new world immediately.
    update_camera();
}

void Game::apply_gun_textures(Player& p) {
    for (int i = 0; i < Player::kSlotCount; ++i) {
        p.set_gun_texture(static_cast<WeaponKind>(i), gun_hand_tex_[i].get());
    }
}

Texture* Game::projectile_tex_for(WeaponKind k) {
    switch (k) {
        case WeaponKind::RocketLauncher: return rocket_tex_.get();
        case WeaponKind::Grenade: return grenade_tex_.get();
        case WeaponKind::Concussion: return concussion_tex_.get();
        case WeaponKind::Lure: return lure_tex_.get();
        default: return projectile_tex_.get();
    }
}

void Game::capture_screenshot() { capture_screenshot_to(screenshot_path_); }

void Game::capture_screenshot_to(const std::string& path) {
    SDL_Surface* surf = SDL_RenderReadPixels(renderer_, nullptr);
    if (!surf) return;
    SDL_SaveBMP(surf, path.c_str());
    SDL_DestroySurface(surf);
}

void Game::shutdown() {
    // Every SDL_Texture must be destroyed while its renderer is still alive.
    // Game members are destroyed *after* this function returns, so anything
    // holding a texture has to be released here by hand or SDL frees it
    // against a dead renderer and corrupts the heap on exit.
    entities_.clear();
    font_.release();  // cached text textures + the TTF_Font (before TTF_Quit)
    tilemap_.clear();
    player_walk_sheet_.reset();
    player_idle_sheet_.reset();
    zombie_walk_sheet_.reset();
    devil_walk_sheet_.reset();
    projectile_tex_.reset();
    fireball_tex_.reset();
    health_tex_.reset();
    weapon_tex_pistol_.reset();
    weapon_tex_shotgun_.reset();
    weapon_tex_machinegun_.reset();
    for (auto& g : gun_hand_tex_) g.reset();
    rocket_tex_.reset();
    grenade_tex_.reset();
    concussion_tex_.reset();
    lure_tex_.reset();
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    TTF_Quit();
}

}  // namespace bd
