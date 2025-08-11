#include <set>
#include "engine.h"
#include "raylib.h"
#include "clock.h"
#include "sound_m.h"
#include "input.h"
#include "raycam_m.h"
#include "texture_m.h"
#include "object_m.h"
#include "definitions.h"
#include "ldtk_m.h"
#include "shader_m.h"
#include "collisionRect.h"

Engine::Engine()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(1920, 1080, "rayEngine");
    MaximizeWindow();
    InitAudioDevice();

    SetTargetFPS(120);

    Raycam_m::init();
    Texture_m::load();
    Sound_m::load();
    InputMap::init();
    // Load LDtk project
    Ldtk_m::loadLevel("ldtk_test.ldtk");

    // Shader_m::load();
}

void Engine::game_loop()
{
    while (!WindowShouldClose()) {
        // Shader_m::routine();

        Clock::lap();
        Ldtk_m::routine();

        Shader_m::beginScenePass();
            ClearBackground(CLITERAL(Color){50, 50, 50, 255});
            Object_m::routine();
            Raycam_m::getRayCam().routine();
            BeginMode2D(Raycam_m::getCam());
                render();
            EndMode2D();
        Shader_m::endScenePass();

        BeginDrawing();
            ClearBackground(BLACK);
            Shader_m::blit();
            DrawFPS(10, 10);
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
    for (CollisionRect* body : to_render) sorted_bodies.insert(body);

    for (CollisionRect* body : sorted_bodies) {
        body->getFather()->draw();
        if (!body->is_static) DrawRectangleRec(body->getSurface(), Fade(RED, 0.4));
    }
}

Engine::~Engine()
{
    // Shader_m::unload();
    Texture_m::unload();
    Object_m::unload();
    CloseWindow();
}
