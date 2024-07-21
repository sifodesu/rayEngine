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
#include "tiled_m.h"

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
    Tiled_m::loadLevel("untitled.tmj");
    Object_m::loadLevel("test.json");
}

void Engine::game_loop()
{

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(CLITERAL(Color){50, 50, 50, 255});

        Clock::lap();
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

Engine::~Engine()
{
    Texture_m::unload();
    Object_m::unload();
    CloseWindow();
}