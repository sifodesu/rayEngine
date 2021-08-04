#include <string>
#include "engine.h" 
#include "raylib.h"

std::string levelPath = "../Data/Levels/";
std::string texturesPath = "../Data/Textures/";
std::string asciiPath = "../Data/Polices/";
std::string blueprintsPath = "../Data/Entities/blueprints.json";
std::string saveData = "../Data/Saves/"; // save.txt"
std::string musicPath = "../Data/Music/";

Engine::Engine(const int screenWidth, const int screenHeight) {
    InitWindow(screenWidth, screenHeight, "rayEngine");
    SetTargetFPS(120);
    texture_m = Texture_m(texturesPath);
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
    CloseWindow();
}