#include <string>
#include "engine.h" 
#include "raylib.h"
#include "definitions.h"
#include "rigidBody.h"
#include "input.h"
#include "runes.h"
#include "basicEnt.h"

Engine::Engine(const int screenWidth, const int screenHeight) : camera_(screenWidth, screenHeight) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT); //FLAG_WINDOW_HIGHDPI |
    InitWindow(screenWidth, screenHeight, "rayEngine");
    // ToggleFullscreen();
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;    

    Texture_m::load();
    InputMap::init();
    Object_m::loadBlueprints();
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
        DrawFPS(10, 10);
        
        BeginMode2D(camera_.getCam());
        render();
        EndMode2D();

        EndDrawing();
    }
}

void Engine::render() {
    // auto to_render = RigidBody::query(camera_.getRect());
    // for (auto body : to_render) {
    //     if (body) {
    //         body->father_->draw();
    //         if(body->father_->id_ == 1)
    //             camera_.to_follow_ = body;
    //     }
    // }

    //temp print everybody
    for(auto [id, obj] : Object_m::level_ents_){
        obj->draw();
        if (obj->id_ == 1)
            camera_.to_follow_ = ((BasicEnt*)obj)->body_;
    }
    Runes::draw({0, 0});
}

Engine::~Engine() {
    Texture_m::unload();
    Object_m::unload();
    CloseWindow();
}