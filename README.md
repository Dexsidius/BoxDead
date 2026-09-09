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
- **Windows (MSYS2, the easiest route):** install MSYS2 (`winget install
  MSYS2.MSYS2`), then from the **UCRT64** shell:

  ```bash
  pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
            mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sdl3 \
            mingw-w64-ucrt-x86_64-sdl3-ttf
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  ```

  Run `./build/BoxDead.exe` from that same UCRT64 shell (it needs
  `C:\msys64\ucrt64\bin` on PATH for `SDL3.dll` / `SDL3_ttf.dll`), or copy
  those DLLs next to the exe.
- **Windows (vcpkg):** `vcpkg install sdl3 sdl3-ttf`
- **No package manager?** Pass `-DBOXDEAD_FETCH_SDL3=ON` and CMake fetches
  SDL3 from source automatically. SDL3_ttf still needs to be installed
  separately (it is only used for text, so you can also remove the
  `find_package(SDL3_ttf ...)` line and font rendering if you want a
  dependency-free build).

## Assets and Git LFS

`assets/dejavu-sans.ttf` is stored in **Git LFS**. GitHub's "Download ZIP"
button does *not* fetch LFS content, so a zip download leaves a 131-byte
pointer file there and the game exits at startup with `Font load failed`.
Either clone with `git lfs` installed (`git lfs pull`), or drop any real
`DejaVuSans.ttf` at that path.

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
  barrel.hpp            - Barrel (explosive prop: health + fuse)
  explosion.hpp         - Explosion (the fireball a blast leaves behind)
  iso_sprite.hpp        - procedural isometric art (characters, barrels, fx)
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
  barrel.cpp / explosion.cpp / iso_sprite.cpp
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

There are two enemy types, both Boxhead-style box figures drawn by the
isometric renderer (see "Boxhead-style characters" above):

- **Zombie** — the general enemy. Pale green skin, dark hair, white shirt with a
  red tie and blood splatter. 1 HP, speed 120 px/s. Spawns every wave.
- **Red Devil** — the special enemy. Red skin, dark red body, two horns.
  Inherits the zombie's behavior (same speed and chase AI) but is tougher at
  2 HP. Starts appearing from wave 2 onward (25% chance per spawn). **Devils
  also fire fireballs**: when a devil has a direct line of sight to the player
  at 50px or less (no solid tile between them), it lobs an orange fireball
  toward the player on a per-devil cooldown. A fireball that hits the player
  deals contact damage on the same invulnerability window as a melee hit.

## Boxhead-style characters

Characters (player and enemies) are drawn per frame with `SDL_RenderGeometry`
as a stack of shaded, black-outlined 3D boxes -- the Boxhead look: a chunky
square head, a slab of hair, a coloured shirt, stubby legs with boots, and both
arms held straight out front. Each figure is built from:

- a **ground shadow**,
- **two legs with shoes** that stride forward/back along the facing direction
  and lift only while swinging, so one leg bears weight while the other swings,
- a **torso** carrying decals painted on its front face (a tie stripe, blood),
- **two arms** reaching forward; the player's gun is drawn at the hands,
  rotated to the exact aim angle,
- a **head** with eyes (and a mouth on zombies) painted on its front face,
- a **hair slab** and optional **horns** on top.

Every box is shaded per visible face (the face squarest to the view is
lightest) and traced with a dark outline, which is what makes the figures read
as crisp Boxhead cutouts rather than untextured blobs. Decals are placed in a
box face's own UV space by `fill_face_rect`, so they follow the body as it
turns. The whole figure yaws to the aim direction -- limbs included, so the
arms and the stride swing around with the facing and the eyes disappear when a
character turns its back to the camera. Entities are drawn back-to-front by
ground position so closer characters overlap further ones.

A palette is authored as flat base colors (`IsoCharStyle`: skin, hair, shirt,
pants, shoe, tie, horn, eye, outline) and the renderer derives the per-face
shading itself, so adding a character is a handful of colors:

- **Survivors** - tan skin, black hair, denim legs, black boots; the three
  playable characters differ only by shirt color (blue / green / red).
- **Zombie** - sickly green skin, black hair, a bloodied white office shirt
  with a red tie, dark slacks, and a gaping mouth.
- **Red Devil** - red hide, bald, two black horns, glowing yellow eyes.

`style.tint` multiplies the whole palette, which is how the player flashes red
during i-frames and how a barrel flashes white on its fuse.

## Inventory & weapons

The player carries a 3-slot inventory: Pistol (slot 1, infinite ammo), Shotgun
(slot 2), and Machine Gun (slot 3). Weapon pickups are added to the inventory
(or refill ammo if already owned). Switch with **1/2/3** or cycle with **Q/E**;
the HUD lists every owned weapon, highlights the selected one, and greys out
dry weapons. Running out of ammo on a finite weapon auto-switches back to the
Pistol so you are never stuck. The aim mode (Face Mouse vs Face Movement) can
be toggled in the Options menu.

## Explosive barrels

Every scene is littered with red fuel drums that go up when shot and take the
horde with them.

- **Shoot them.** A barrel has 3 HP and blocks movement like a wall until it is
  destroyed. Any bullet stops on a barrel -- your shots and devil fireballs
  both count.
- **Fuse.** At 0 HP the barrel flashes white-hot for 0.22s before detonating,
  which is your window to get clear.
- **Blast.** Everything within 110px takes 5 damage -- enough to kill zombies
  (1 HP) and devils (2 HP) outright. Blast kills count toward your score and
  can drop items just like a shot kill.
- **Chain reaction.** Barrels caught in a blast light their own fuses, so one
  well-placed shot walks the explosion down a row of drums.
- **It hurts you too.** Standing in your own blast costs 1 HP on the normal
  invulnerability window, so a chain can never delete the whole health bar --
  but leading a pack of zombies past a barrel and shooting it is the intended
  play, not standing next to it.

Barrels come from the level tileset, so they are level design rather than
hardcoded positions. A tile whose name contains `barrel`, `drum`, `explosive`,
or `tnt` is never drawn or collided as a tile: the tilemap hands its placements
to the game, which spawns a destructible `Barrel` entity at each one (drawn as
a 3D drum by `draw_iso_barrel`, so the tile's `.bmp` only exists for the
editor's preview). Add barrels to a map by adding a `Barrel` layer in
LevelEdit++ and painting them in.

Code: `include/boxdead/barrel.hpp` / `src/barrel.cpp` (the prop and its fuse),
`include/boxdead/explosion.hpp` / `src/explosion.cpp` (the fireball), and
`Game::update_barrels()` / `Game::detonate()` in `src/game.cpp` (the blast
sweep, which is the only thing that can see every entity).

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
        "Block": { "filepath": "assets/Block.bmp", "locations": [...] },
        "Barrel": { "filepath": "assets/Barrel.bmp", "locations": [...] }
    }
}
```

- `filepath` is relative to the `.mx` file's directory (the editor writes
  `assets/<TileName>.bmp`).
- `locations` is a list of `[x, y, w, h]` world-pixel placements (default tile
  size 32x32). The player/enemies collide with solid tiles.

### Explosive tiles (barrels)

A tile is an **explosive barrel** if its name contains (case-insensitive):
`barrel`, `drum`, `explosive`, `tnt`. This is checked before the solid test, so
"Explosive Barrel" becomes a barrel rather than a wall. Those placements are
lifted out of the tile list entirely and become `Barrel` entities -- see
"Explosive barrels" above.

### Solid tiles (collision)

The `.mx` format is purely visual, so BoxDead infers collision from the tile
**name**. A tile is solid (blocks movement) if its name contains any of
(case-insensitive): `wall`, `block`, `rock`, `stone`, `barrier`, `fence`,
`crate`, `pillar`, `obstacle`. Name your blocking tiles accordingly in the
editor and the player/enemies will slide along them instead of walking through.

### Adding a new scene

1. Build a map in LevelEdit++ using `.bmp` tiles (32x32). Name walls/blocks with a
   solid keyword if they should block movement, and name a layer `Barrel` for
   explosive drums.
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
environments. The summary includes `enemy_frame` (a live enemy's current animation frame,
proving the animator ticks), `barrels` (how many barrels detonated), and
`blast_kills` (enemies killed by explosions rather than bullets), so a run
that reports `barrels=0` means the level's explosive tiles never loaded.

The screenshot harness (`--screenshot <path>`) is the visual counterpart: it
runs 220 frames against a real renderer, forces a devil into fireball range at
frame 30, shoots out the barrel nearest the player at frame 45, and swaps
scenes at frame 100, dumping BMPs around each event. There are also pure unit tests:

```bash
# Animator: frame advance + looping.
g++ -std=c++20 -I include test_animator.cpp src/animation.cpp -o test_animator
./test_animator

# Player weapon state: equip / consume / auto-revert.
g++ -std=c++20 -I include test_weapon.cpp src/player.cpp src/weapon.cpp \
    src/entity.cpp src/animated_entity.cpp src/animation.cpp \
    src/sprite.cpp src/texture.cpp src/iso_sprite.cpp src/tilemap.cpp \
    -lSDL3 -o test_weapon
./test_weapon

# Tilemap loader: every scene .mx parses, yields tiles, and lists barrels.
g++ -std=c++20 -I include -I /usr/include/SDL3 test_tilemap.cpp \
    src/tilemap.cpp src/texture.cpp -lSDL3 -o test_tilemap
SDL_VIDEO_DRIVER=dummy ./test_tilemap
```
