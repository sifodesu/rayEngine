#include <set>
#include <string>
#include "engine.h"
#include "raylib.h"
#include "definitions.h"
#include "rigidBody.h"
#include "input.h"
#include "runes.h"
#include "basicEnt.h"
#include "bullet_m.h"
#include "character.h"
#include "clock.h"
#include "raymath.h"
#include "sound_m.h"

Engine::Engine() {
#ifdef NDEBUG
    SetTraceLogLevel(LOG_WARNING);
#endif
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_UNDECORATED);
    InitWindow(0, 0, "rayEngine");
    camera_ = Raycam();
    MaximizeWindow();
    InitAudioDevice();
    // ToggleFullscreen();
    SetTargetFPS(420);
    screenWidth_ = GetScreenWidth();
    screenHeight_ = GetScreenHeight();

    Texture_m::load();
    Sound_m::load();
    InputMap::init();
    Object_m::loadBlueprints();
    Object_m::loadLevel("test.json");
    Bullet_m::init();
    Runes::init();


}

void Engine::game_loop() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(CLITERAL(Color) { 50, 50, 50, 255 });

        Clock::lap();
        Object_m::routine();
        Bullet_m::routine();
        camera_.routine();
        Runes::routine();
        DrawFPS(10, 10);

        BeginMode2D(camera_.getCam());

        render();

        //temp
        Vector2 pos = camera_.getCam().target;
        pos.x += 32;
        Runes::draw(pos);
        //

        EndMode2D();
        EndDrawing();
    }
}

void Engine::render() {
    std::vector<CollisionRect*> to_render = CollisionRect::query(camera_.getRect());

    auto comp = [](CollisionRect* a, CollisionRect* b) {
        return a->getCoord().y <= b->getCoord().y;
    };
    std::set<CollisionRect*, decltype(comp)> sorted_bodies;

    for (CollisionRect* body : to_render) {
        //temp
        if (t(*body->getFather()) == t(Character))
            camera_.to_follow_ = (RigidBody*)body;

        sorted_bodies.insert(body);
    }

    for (CollisionRect* body : sorted_bodies) {
        body->getFather()->draw();

        //debug blit hitboxes
        DrawRectangleRec(body->getSurface(), Fade(RED, 0.4));
    }
    Bullet_m::draw();
}

Engine::~Engine() {
    Texture_m::unload();
    Object_m::unload();
    CloseWindow();
}