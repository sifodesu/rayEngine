#include <string>
#include "engine.h" 
#include "raylib.h"
#include "definitions.h"


Engine::Engine(const int screenWidth, const int screenHeight) {
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(screenWidth, screenHeight, "rayEngine");
    SetTargetFPS(120);

    texture_m = new Texture_m();
    texture_m->load(TEXTURES_PATH);

    object_m = new Object_m();
}

void Engine::game_loop() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

        EndDrawing();
    }
}

Engine::~Engine() {
    delete texture_m;
    delete object_m;
    CloseWindow();
}