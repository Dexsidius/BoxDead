// BoxDead - Projectile test: bullets stop at walls, rockets detonate on them,
// and thrown grenades come to rest and go off on their fuse. Run with:
//   SDL_VIDEO_DRIVER=dummy ./test_projectile
#include "boxdead/projectile.hpp"
#include "boxdead/tilemap.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %-52s %s\n", what, ok ? "OK" : "FAIL");
    if (!ok) ++failures;
}

// Step a projectile until it dies, asks to explode, or the step budget runs
// out. Returns the number of steps taken.
int run(bd::Projectile& p, const bd::GameContext& ctx, int max_steps = 400) {
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < max_steps; ++i) {
        if (!p.alive || p.blast_pending) return i;
        p.update(dt, ctx);
    }
    return max_steps;
}

}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("t", 8, 8, 0);
    SDL_Renderer* r = SDL_CreateRenderer(win, nullptr);

    bd::Tilemap tm;
    if (!tm.load(r, "assets/maps/Courtyard/Courtyard.mx")) {
        std::printf("could not load the Courtyard map\n");
        return 1;
    }
    float ww = 0.0f;
    float wh = 0.0f;
    tm.world_bounds(ww, wh);

    bd::GameContext ctx;
    ctx.tilemap = &tm;
    ctx.world_w = ww;
    ctx.world_h = wh;

    // The map's border ring is solid, so a bullet fired left from open floor
    // has to stop against it rather than sail out of the world.
    check(tm.is_solid(16.0f, 300.0f), "map border is solid");
    check(!tm.is_solid(300.0f, 300.0f), "open floor at (300,300) is walkable");

    {
        bd::Projectile bullet(300.0f, 300.0f, -600.0f, 0.0f, 1);
        run(bullet, ctx);
        check(!bullet.alive, "bullet dies on the wall");
        check(bullet.pos.x >= 24.0f,
              "bullet stopped at the wall, not past it");
        check(!bullet.blast_pending, "plain bullet raises no blast");
    }

    {
        // A rocket into the same wall should ask for a blast instead of just
        // vanishing.
        bd::Projectile rocket(300.0f, 300.0f, -430.0f, 0.0f, 3);
        bd::Payload load;
        load.blast_radius = 130.0f;
        load.blast_damage = 8;
        rocket.arm(load, 0.0f);
        run(rocket, ctx);
        check(rocket.blast_pending, "rocket detonates against the wall");
        check(rocket.pos.x >= 24.0f, "rocket stopped at the wall");
    }

    {
        // A grenade thrown at a wall keeps its fuse and goes off where it
        // landed rather than punching through.
        bd::Projectile nade(300.0f, 300.0f, -300.0f, 0.0f, 0);
        bd::Payload load;
        load.blast_radius = 105.0f;
        load.blast_damage = 5;
        nade.arm(load, 1.1f);
        const int steps = run(nade, ctx);
        check(nade.blast_pending, "grenade detonates on its fuse");
        check(steps >= 60, "grenade waited out its ~1.1s fuse");
        check(nade.pos.x >= 24.0f, "grenade came to rest short of the wall");
    }

    {
        // Fired into open space away from any wall, a bullet should survive a
        // good while - otherwise the wall test above proves nothing.
        bd::Projectile clear_shot(300.0f, 300.0f, 600.0f, 0.0f, 1);
        clear_shot.update(1.0f / 60.0f, ctx);
        check(clear_shot.alive, "bullet into open floor survives its first step");
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();

    if (failures != 0) {
        std::printf("projectile test: %d FAILURES\n", failures);
        return 1;
    }
    std::printf("projectile test: all checks OK\n");
    return 0;
}
