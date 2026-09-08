cmake_minimum_required(VERSION 3.24)

project(BoxDead VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL3 can be provided by the system/SDK (find_package) or fetched on demand.
# To build out of the box without a preinstalled SDL3, pass -DBOXDEAD_FETCH_SDL3=ON.
option(BOXDEAD_FETCH_SDL3 "Fetch SDL3 from source via FetchContent" OFF)

if(BOXDEAD_FETCH_SDL3)
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.16
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 CONFIG REQUIRED)
endif()

add_executable(BoxDead
    src/main.cpp
    src/game.cpp
    src/entity.cpp
    src/player.cpp
    src/enemy.cpp
    src/projectile.cpp
    src/texture.cpp
    src/sprite.cpp
)

target_include_directories(BoxDead PRIVATE include)
target_link_libraries(BoxDead PRIVATE SDL3::SDL3)

if(MSVC)
    target_compile_options(BoxDead PRIVATE /W4)
else()
    target_compile_options(BoxDead PRIVATE -Wall -Wextra -Wpedantic)
endif()
