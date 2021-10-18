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

Engine::Engine(const int screenWidth, const int screenHeight) : camera_(screenWidth, screenHeight) {
    SetTraceLogLevel(LOG_WARNING);
    // SetConfigFlags(FLAG_VSYNC_HINT); //FLAG_WINDOW_HIGHDPI |
    InitWindow(screenWidth, screenHeight, "rayEngine");
    InitAudioDevice();
    // ToggleFullscreen();
    SetTargetFPS(420);
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;

    Texture_m::load();
    Sound_m::load();
    InputMap::init();
    Object_m::loadBlueprints();
    Object_m::loadLevel("test.json");
    Bullet_m::init();
    Runes::init();

 
}

void Engine::game_loop() {
    // Music neila = Sound_m::getSound("NEILA2.mp3");
    // PlayMusicStream(neila);

    while (!WindowShouldClose()) {
        // UpdateMusicStream(neila);

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
        pos.x -= 32;
        pos.y += 40;
        Runes::draw(pos);
        //
        
        EndMode2D();
        EndDrawing();
    }
}

void Engine::render() {
    auto to_render = RigidBody::query(camera_.getRect());
    auto comp = [](RigidBody* a, RigidBody* b) {
        return a->getCoord().y < b->getCoord().y;
    };
    std::set<RigidBody*, decltype(comp)> sorted_bodies;

    for (auto body : to_render) {
        //temp
        if (t(*body->father_) == t(Character))
            camera_.to_follow_ = body;

        sorted_bodies.emplace(body);
    }
    for (auto body : sorted_bodies) {
        body->father_->draw();

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