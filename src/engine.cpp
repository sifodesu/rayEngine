#include <string>
#include "engine.h" 
#include "raylib.h"
#include "definitions.h"
#include "rigidBody.h"


Engine::Engine(const int screenWidth, const int screenHeight) : camera_(screenWidth, screenHeight) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(screenWidth, screenHeight, "rayEngine");
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
    SetTargetFPS(120);

    Texture_m::load();
    // Object_m::loadBlueprints();
    Object_m::loadLevel("test.json");
}

void Engine::game_loop() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode2D(camera_.getCam());
        Object_m::routine();
        camera_.routine();
        render();
        EndMode2D();

        EndDrawing();
    }
}

void Engine::render() {
    auto to_render = RigidBody::query(camera_.getRect());
    for (auto body : to_render) {
        if (body) {
            body->father_->draw(body->getCoord());
            // body->setSpeed({100, 0});
            // camera_.to_follow_ = body;
        }
    }
}

Engine::~Engine() {
    Texture_m::unload();
    Object_m::unloadAll();
    CloseWindow();
}