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

A 1280x720 window opens with a dark background and a red placeholder square.
Close the window or press **Esc** to exit.

## Smoke test (no display required)

```bash
./build/BoxDead --smoke-test
```

Runs a single frame and exits — useful for CI or headless environments.
