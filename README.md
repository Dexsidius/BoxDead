# BoxDead

A re-edition of Boxhead built in C++ with SDL3.

## Prerequisites

- CMake 3.24+
- A C++20 compiler (MSVC, Clang, or GCC)
- SDL3 (3.2.x) and SDL3_ttf (3.2.x)

### Installing SDL3

- **macOS (Homebrew):** `brew install sdl3 sdl3-ttf`
- **Debian/Ubuntu:** `apt install libsdl3-dev libsdl3-ttf-dev`
- **Arch:** `pacman -S sdl3 sdl3_ttf`
- **Windows (vcpkg):** `vcpkg install sdl3 sdl3-ttf`
- **No package manager?** Pass `-DBOXDEAD_FETCH_SDL3=ON` and CMake fetches
  SDL3 from source automatically. SDL3_ttf still needs to be installed
  separately (it is only used for text, so you can also remove the
  `find_package(SDL3_ttf ...)` line and font rendering if you want a
  dependency-free build).

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

A 1280x720 window opens to a **main menu** (New Game / Options / Exit).
Choose New Game to start: a blue player, red enemies that spawn at the screen
edges and chase the player, and yellow projectiles. Move with WASD/arrows, aim
with the mouse, and fire with Space or left-click to destroy enemies. Enemies
deal contact damage; the player has 5 HP shown in a health bar. Pick up items
(weapons and consumables) by walking over them. When health hits zero, the
game ends with a GAME OVER screen — press R to restart or Esc for the menu.

## Character select

Between **New Game** and spawning in, a **SELECT CHARACTER** screen lets you
pick one of three playable survivors, each with its own color palette: Blue
Survivor, Green Ranger, or Red Brawler. A live isometric preview of the
highlighted character idle-walks below the menu so you can see the palette
before you commit. Navigate with Up/Down (or W/S), confirm with Enter/Space,
or click an entry directly; pick Back to return to the main menu. The chosen
palette is applied to the player on spawn.

## Items, weapons, and consumables

Items are static pickups collected on contact. The `Item` base exposes a
single `on_pickup(Game&, Player&)` hook; concrete items live the side effects
there, not in `Game`.

- **Health pickup** (green cross) — restores 2 HP, capped at 5.
- **Weapon pickup** (colored box per weapon) — equips a new weapon with finite
  ammo. When ammo runs out, the player reverts to the infinite pistol.

Weapons are value types, not polymorphic: a `WeaponSpec` (cooldown, projectile
count, spread cone, speed, damage, ammo) is looked up from a `WeaponKind`.
`Game::fire_projectile()` asks the player for the current spec and emits the
spread of projectiles in one path, so adding a weapon is just a new enum entry
plus a spec.

- **Pistol** — infinite ammo, 1 projectile, fast cooldown.
- **Shotgun** — 6 rounds, 5-projectile spread.
- **Machine Gun** — 30 rounds, rapid single shots.

Items drop from killed enemies (≈25% chance) and also spawn on the floor
every few seconds (up to 3 at once). A toast message flashes near the top when
you pick something up, and the HUD shows the current weapon and remaining
ammo.

## Project structure

```
include/boxdead/        - public headers (one responsibility each)
  math.hpp              - Vec2
  texture.hpp           - Texture (RAII SDL_Texture)
  sprite.hpp            - Sprite + draw_sprite + procedural art
  entity.hpp            - Entity base + GameContext + overlap test
  player.hpp            - Player (movement, health, i-frame flash, weapons)
  enemy.hpp             - Enemy (chase AI)
  projectile.hpp        - Projectile (bullet + hit damage)
  weapon.hpp            - WeaponKind + WeaponSpec profiles
  item.hpp              - Item base + HealthPickup + WeaponPickup
  animation.hpp         - Animation (sprite-sheet frames) + Animator
  animated_entity.hpp   - AnimatedEntity base (Player/Enemy derive from it)
  font.hpp              - Font (RAII TTF_Font + cached text textures)
  menu.hpp              - Menu (navigable text menu)
  game.hpp              - Game class (state machine + loop + systems)
src/                    - implementations
  main.cpp              - entry point (thin bootstrap)
  game.cpp              - state machine, loop, spawning, firing, collisions,
                          item pickups, rendering, HUD
  player.cpp / enemy.cpp / projectile.cpp
  weapon.cpp / item.cpp / menu.cpp / font.cpp
  animation.cpp / animated_entity.cpp
  texture.cpp / sprite.cpp / entity.cpp
assets/dejavu-sans.ttf   - bundled TrueType font for text rendering
```

The sprite layer draws textured quads when a texture is set, and falls back
to a solid color otherwise. Placeholder art is generated procedurally with
`make_solid_sprite_texture()` — swap it for `Texture::load(renderer, path)`
to use real BMP assets (add SDL_image for PNG/JPG).

## Controls

- **WASD** or **Arrow keys** - move the player
- **Mouse** - aim
- **Space** or **Left mouse** - fire
- **1 / 2 / 3** - switch to Pistol / Shotgun / Machine Gun (owned weapons only)
- **Q / E** - cycle to the previous / next owned weapon
- **Up/Down** (or W/S) + **Enter** - navigate menus; mouse hover/click works too
- **Esc** - back / quit (context-dependent)
- **R** - restart after GAME OVER

Projectiles fire toward the aim direction using the player's current weapon
profile (cooldown, projectile count, and spread cone). A projectile that hits
an enemy applies the weapon's damage and may drop an item. Enemies spawn at
the edges and chase the player; touching the player deals 1 damage with a
1.2-second invulnerability window (the player flashes red). The player starts
with 5 HP shown in a health bar; reaching 0 ends the game.

Movement is frame-rate-independent: the player moves at a fixed speed in
pixels per second regardless of FPS, so it feels the same on 60 Hz and
144 Hz displays. Diagonal movement is normalized so it isn't faster than
cardinal movement.

## Animations

Player and enemy characters are animated. An `Animation` is a horizontal
sprite sheet of equal-sized frames played left-to-right at a fixed frame
duration; an `Animator` plays named animations and tracks the current frame.
`Player` and `Enemy` derive from `AnimatedEntity`, which owns an `Animator` and
renders the current frame (keeping the entity color as the tint, so the
player's i-frame flash still works). Lightweight entities (projectiles, items)
stay simple and skip animation overhead.

- **Player** switches between `walk` (4 frames) while moving and `idle`
  (2 frames, body bob) when standing still.
- **Enemies** run a `walk` cycle (4 frames) continuously while chasing.

The walk/idle sprite sheets are generated procedurally (`make_walk_sheet_texture`)
so the boxman has a body, a border, and two legs whose heights alternate per
frame to read as a walk cycle. Swap them for real sprite sheets later by loading
a horizontal strip and slicing it with `make_animation()`.

## Enemies

There are two enemy types, both Boxhead-style creatures drawn procedurally with
`make_creature_sheet_texture` (a boxy head with a hair band or horns, a torso
with a tie and blood, and two legs running a 4-phase walk):

- **Zombie** — the general enemy. Pale green skin, dark hair, white shirt with a
  red tie and blood splatter. 1 HP, speed 120 px/s. Spawns every wave.
- **Red Devil** — the special enemy. Red skin, dark red body, two horns.
  Inherits the zombie's behavior (same speed and chase AI) but is tougher at
  2 HP. Starts appearing from wave 2 onward (25% chance per spawn). **Devils
  also fire fireballs**: when a devil has a direct line of sight to the player
  at 50px or less (no solid tile between them), it lobs an orange fireball
  toward the player on a per-devil cooldown. A fireball that hits the player
  deals contact damage on the same invulnerability window as a melee hit.

## Isometric characters

Characters (player and enemies) render as isometric 3D box figures drawn
per frame with `SDL_RenderGeometry`. Each figure has a ground shadow, two
animated legs that stride forward/back under the body (feet poke out beyond
the torso and lift during the swing phase, alternating left/right so it reads
as a real walk cycle instead of a hop), a torso with three shaded faces (top /
front / side) sitting on top of the legs, and a head. The body rotates to face
the aim direction (8-way yaw) so the visible faces track where the character is
facing. The player additionally holds the current gun in-hand, rotated to the
aim angle. Entities are drawn back-to-front by ground position so closer
characters correctly overlap further ones.

## Inventory & weapons

The player carries a 3-slot inventory: Pistol (slot 1, infinite ammo), Shotgun
(slot 2), and Machine Gun (slot 3). Weapon pickups are added to the inventory
(or refill ammo if already owned). Switch with **1/2/3** or cycle with **Q/E**;
the HUD lists every owned weapon, highlights the selected one, and greys out
dry weapons. Running out of ammo on a finite weapon auto-switches back to the
Pistol so you are never stuck. The aim mode (Face Mouse vs Face Movement) can
be toggled in the Options menu.

## Scenes / levels

The game spans multiple maps. Every 15 waves the world transitions to the next
scene: a fade-to-black, then the new map swaps in (floor palette + level name
banner), enemies and projectiles are cleared, and the player is recentered.
Levels cycle (Courtyard -> Asylum -> Sewers -> Graveyard -> Hell's Gate) so
play continues indefinitely with a changing backdrop.

Each scene's floor, walls, and obstacles are rendered from a tileset authored in
the [LevelEdit++](https://github.com/TheSardonicals/LevelEdit-Plus/tree/dexsidius-dev)
level editor (the `dexsidius-dev` branch). See "Level design (tilesets)" below.

## Level design (tilesets)

Scene layouts are `.mx` JSON tilesets exported by the LevelEdit++ editor. BoxDead
loads them at runtime via a small tilemap system (`src/tilemap.cpp`,
`include/boxdead/tilemap.hpp`) backed by the vendored single-header JSON parser at
`include/nlohmann/json.hpp`.

### Editor export format

An `.mx` file is plain JSON. Each scene is a folder under `assets/maps/<Level>/`
containing the `.mx` plus an `assets/` subfolder of `.bmp` tile images:

```json
{
    "name": "Courtyard",
    "tiles": {
        "Ground": {
            "filepath": "assets/Ground.bmp",
            "locations": [[32, 32, 32, 32], [64, 32, 32, 32], ...]
        },
        "Wall":  { "filepath": "assets/Wall.bmp",  "locations": [...] },
        "Block": { "filepath": "assets/Block.bmp", "locations": [...] }
    }
}
```

- `filepath` is relative to the `.mx` file's directory (the editor writes
  `assets/<TileName>.bmp`).
- `locations` is a list of `[x, y, w, h]` world-pixel placements (default tile
  size 32x32). The player/enemies collide with solid tiles.

### Solid tiles (collision)

The `.mx` format is purely visual, so BoxDead infers collision from the tile
**name**. A tile is solid (blocks movement) if its name contains any of
(case-insensitive): `wall`, `block`, `rock`, `stone`, `barrier`, `fence`,
`crate`, `pillar`, `obstacle`. Name your blocking tiles accordingly in the
editor and the player/enemies will slide along them instead of walking through.

### Adding a new scene

1. Build a map in LevelEdit++ using `.bmp` tiles (32x32). Name walls/blocks with a
   solid keyword if they should block movement.
2. Export to `.mx` into a folder like `assets/maps/MyLevel/` with its tile `.bmp`s
   in `assets/maps/MyLevel/assets/`.
3. Add a `Level` entry in `src/game.cpp` (`kLevels[]`) pointing `map_path` at the
   new `.mx`. Give it a fallback `floor` color used if the file fails to load.

If a map fails to load (missing file, bad JSON), the scene falls back to a solid
floor color and the game keeps running. Regenerate the bundled sample maps with
`python3 tools/gen_maps.py`.

## Windows build

A prebuilt Windows x64 build is included in `dist/` of this repo:
`dist/BoxDead-v0.1.2-windows-x64.zip`. Unzip it and double-click
`BoxDead.exe`; keep `SDL3.dll`, `SDL3_ttf.dll`, and the `assets/` folder next
to the exe. It is cross-compiled from Linux with MinGW-w64 against SDL3 3.4.16
and SDL3_ttf 3.2.2 and bundles the runtime DLLs, font, and scene tilesets.

## Smoke test (no display required)

```bash
SDL_VIDEO_DRIVER=dummy ./build/BoxDead --smoke-test
```

Runs a ~15-second headless scenario (auto-firing at enemies, forcing item
spawns on the player) and prints a summary line — useful for CI or headless
environments. The summary includes `enemy_frame`, a live enemy's current
animation frame, proving the animator ticks. There are also pure unit tests:

```bash
# Animator: frame advance + looping.
g++ -std=c++20 -I include test_animator.cpp src/animation.cpp -o test_animator
./test_animator

# Player weapon state: equip / consume / auto-revert.
g++ -std=c++20 -I include test_weapon.cpp src/player.cpp src/weapon.cpp \
    src/entity.cpp src/animated_entity.cpp src/animation.cpp \
    src/sprite.cpp src/texture.cpp -lSDL3 -o test_weapon
./test_weapon

# Tilemap loader: every scene .mx parses and yields tiles.
g++ -std=c++20 -I include -I /usr/include/SDL3 test_tilemap.cpp \
    src/tilemap.cpp src/texture.cpp -lSDL3 -o test_tilemap
SDL_VIDEO_DRIVER=dummy ./test_tilemap
```
