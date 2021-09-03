#include <string>
#include "engine.h" 
#include "raylib.h"
#include "definitions.h"
#include "rigidBody.h"
#include "input.h"
#include "runes.h"

Engine::Engine(const int screenWidth, const int screenHeight) : camera_(screenWidth, screenHeight) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(screenWidth, screenHeight, "rayEngine");
    // ToggleFullscreen();
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
    SetTargetFPS(120);

    Texture_m::load();
    InputMap::init();
    // Object_m::loadBlueprints();
    Object_m::loadLevel("test.json");
    Runes::init();
}

void Engine::game_loop() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        Object_m::routine();
        camera_.routine();
        Runes::routine();
        BeginMode2D(camera_.getCam());
        render();
        EndMode2D();

        EndDrawing();
    }
}

void Engine::render() {
    // auto rrr = camera_.getRect();
    auto to_render = RigidBody::query(camera_.getRect());
    for (auto body : to_render) {
        if (body) {
            body->father_->draw(body->getCoord());
            if(body->father_->id_ == 1)
                camera_.to_follow_ = body;
        }
    }
    Runes::draw({0, 0});
}

Engine::~Engine() {
    Texture_m::unload();
    Object_m::unloadAll();
    CloseWindow();
}