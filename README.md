# BoxDead

A re-edition of Boxhead built in C++ with SDL3.

## Prerequisites

- CMake 3.24+
- A C++20 compiler (MSVC, Clang, or GCC)
- SDL3 (3.2.x)

### Installing SDL3

- **macOS (Homebrew):** `brew install sdl3`
- **Debian/Ubuntu:** `apt install libsdl3-dev`
- **Arch:** `pacman -S sdl3`
- **Windows (vcpkg):** `vcpkg install sdl3`
- **No package manager?** Build with `-DBOXDEAD_FETCH_SDL3=ON` and CMake
  fetches SDL3 from source automatically (no preinstall needed).

## Build

```bash
cmake -S . -B build
cmake --build build
```

With auto-fetched SDL3 (no preinstall required):

```bash
cmake -S . -B build -DBOXDEAD_FETCH_SDL3=ON
cmake --build build
```

## Run

```bash
./build/BoxDead        # macOS/Linux
.\build\BoxDead.exe    # Windows
```

A 1280x720 window opens with a dark background, a blue player, red
enemies that spawn at the screen edges and chase the player, and yellow
projectiles. Move with WASD/arrows, aim with the mouse, and fire with
Space or left-click to destroy enemies. Enemies deal contact damage; the
player has 5 HP shown in a health bar. When health hits zero, the game ends.
Press **Esc** or close the window to quit.

## Project structure

```
include/boxdead/        - public headers (one responsibility each)
  math.hpp              - Vec2
  texture.hpp           - Texture (RAII SDL_Texture)
  sprite.hpp             - Sprite + draw_sprite + procedural art
  entity.hpp            - Entity base + GameContext + overlap test
  player.hpp            - Player (keyboard movement, health, i-frame flash)
  enemy.hpp             - Enemy (chase AI)
  projectile.hpp        - Projectile (player-fired bullet)
  game.hpp              - Game class (owns loop, entities, systems)
src/                    - implementations
  main.cpp              - entry point (thin bootstrap)
  game.cpp              - loop, spawning, firing, collisions, rendering
  player.cpp / enemy.cpp / projectile.cpp
  texture.cpp / sprite.cpp / entity.cpp
```

The sprite layer draws textured quads when a texture is set, and falls back
to a solid color otherwise. Placeholder art is generated procedurally with
`make_solid_sprite_texture()` — swap it for `Texture::load(renderer, path)`
to use real BMP assets (add SDL_image for PNG/JPG).

## Controls

- **WASD** or **Arrow keys** - move the player
- **Mouse** - aim
- **Space** or **Left mouse** - fire
- **Esc** or close the window - quit

Projectiles fire toward the mouse cursor with a short cooldown. A projectile
that hits an enemy destroys both. Enemies spawn at the edges and chase the
player; touching the player deals 1 damage with a 1.2-second invulnerability
window (the player flashes red). The player starts with 5 HP shown in a
health bar; reaching 0 ends the game.

Movement is frame-rate-independent: the player moves at a fixed speed in
pixels per second regardless of FPS, so it feels the same on 60 Hz and
144 Hz displays. Diagonal movement is normalized so it isn't faster than
cardinal movement.

## Smoke test (no display required)

```bash
./build/BoxDead --smoke-test
```

Runs a single frame and exits — useful for CI or headless environments.
