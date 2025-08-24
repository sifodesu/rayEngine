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
#include "upgradeRegistry.h"
#include "shader_m.h"
#include "collisionRect.h"
#include "sprite_m.h"
#include <string>

Engine::Engine()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(NATIVE_RES_WIDTH*6, NATIVE_RES_HEIGHT*6, "rayEngine");
    InitAudioDevice();
    //fps limit
    // SetTargetFPS(30);
    Raycam_m::init();
    Texture_m::load();
    Sprite_m::load();
    Sound_m::load();
    InputMap::init();
    UpgradeRegistry::initDefaults();
    Ldtk_m::loadLevel("ldtk_test.ldtk");

    // Shader_m::load();
}

void Engine::game_loop()
{
    while (!WindowShouldClose()) {
        Shader_m::routine();

        Clock::lap();
        Ldtk_m::routine();

    Shader_m::begin();
            ClearBackground(CLITERAL(Color){0, 0, 0, 255});
            Object_m::routine();
            Raycam_m::getRayCam().routine();
            BeginMode2D(Raycam_m::getCam());
                render();
            EndMode2D();
    Shader_m::end();

        BeginDrawing();
            ClearBackground(BLACK);
            // if (Shader_m::has("roundpixels")) Shader_m::addFullscreen("roundpixels");
            Shader_m::present();
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
    std::vector<CollisionRect*> to_render = CollisionRect::query(Raycam_m::getRayCam().getRect());
    
    for (CollisionRect* body : to_render) sorted_bodies.insert(body);

    for (CollisionRect* body : sorted_bodies) {
        body->getFather()->draw();
        // DrawRectangleRec(body->getSurface(), Fade(RED, 0.4));
    }
}

Engine::~Engine()
{
    Shader_m::unload();
    Texture_m::unload();
    Object_m::unload();
    CloseWindow();
}
