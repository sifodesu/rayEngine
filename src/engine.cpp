#include <set>
#include <string>
#include "engine.h"
#include "raylib.h"
#include "definitions.h"
#include "rigidBody.h"
#include "input.h"
#include "basicEnt.h"
#include "character.h"
#include "clock.h"
#include "raymath.h"
#include "sound_m.h"
#include "raycam_m.h"
#include "ldtk_m.h"
#include <filesystem>

Engine::Engine()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(1920, 1080, "rayEngine");
    MaximizeWindow();
    InitAudioDevice();

    SetTargetFPS(120);

    Raycam_m::init();
    Texture_m::load(); // Ensure inv.png exists as a fallback
    Sound_m::load();
    InputMap::init();
    // Load LDtk project
    Ldtk_m::loadLevel("ldtk_test.ldtk");
}

void Engine::game_loop()
{

    while (!WindowShouldClose())
    {
        // Tick game time before rendering
        Clock::lap();

        BeginDrawing();
        ClearBackground(CLITERAL(Color){50, 50, 50, 255});
        hotReloadLevelIfChanged();
        Object_m::routine();
        Raycam_m::getRayCam().routine();

        DrawFPS(10, 10);

        BeginMode2D(Raycam_m::getCam());

        render();

        EndMode2D();
        EndDrawing();
    }
}

void Engine::render()
{
    auto comp = [](CollisionRect* a, CollisionRect* b) {
        return a->getFather()->layer_ <= b->getFather()->layer_;
    };
    std::set<CollisionRect*, decltype(comp)> sorted_bodies;
    
    std::vector<CollisionRect*> to_render = CollisionRect::query(Raycam_m::getRayCam().getRect(), true, false);
    for (CollisionRect* body : to_render) {
        sorted_bodies.insert(body);
    }
    to_render = CollisionRect::query(Raycam_m::getRayCam().getRect(), false, false);
    for (CollisionRect* body : to_render) {
        sorted_bodies.insert(body);
    }

    for (CollisionRect* body : sorted_bodies) {
        body->getFather()->draw();

        //debug blit hitboxes
        if (!body->is_static)
            DrawRectangleRec(body->getSurface(), Fade(RED, 0.4));
    }
}

static std::filesystem::file_time_type s_lastLevelWriteTime{};

void Engine::hotReloadLevelIfChanged()
{
    const std::string levelPath = std::string(LDTK_PATH) + "ldtk_test.ldtk";
    std::error_code ec;
    auto cur = std::filesystem::last_write_time(levelPath, ec);
    if (ec) return;
    if (s_lastLevelWriteTime.time_since_epoch().count() == 0) {
        s_lastLevelWriteTime = cur;
        return;
    }
    if (cur != s_lastLevelWriteTime) {
        s_lastLevelWriteTime = cur;
        // Keep character position: clear only tiles and reload level without characters
        // Reload tiles only; avoid realloc storms in same frame
        try {
            Object_m::clearTiles();
            Ldtk_m::loadLevel("ldtk_test.ldtk", /*skipCharacters=*/true);
        } catch (...) {
            // Swallow to keep program running
        }
    }
}

Engine::~Engine()
{
    Texture_m::unload();
    Object_m::unload();
    CloseWindow();
}