#include <string>
#include "engine.h" 
#include "raylib.h"
#include "definitions.h"
#include <string>


Engine::Engine(const int screenWidth, const int screenHeight) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(screenWidth, screenHeight, "rayEngine");
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
    camera_ = { {100, 100}, {100, 100}, 0, 1.0f };
    SetTargetFPS(120);

    Texture_m::load();
    Object_m::loadBlueprints();
    Object_m::loadLevel("test.json");
}

void Engine::game_loop() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode2D(camera_);
        Object_m::routine();
        render();
        EndMode2D();

        EndDrawing();
    }
}

void Engine::render() {
    Rectangle camera = { camera_.offset.x, camera_.offset.y,
    (camera_.target.x - camera_.offset.x) * 2, (camera_.target.y - camera_.offset.y) * 2 };

    std::vector<GObject*> to_render = Object_m::queryQuad(camera);
    std::cout << to_render.size();
}

Engine::~Engine() {
    Texture_m::unload();
    Object_m::unloadAll();
    CloseWindow();
}