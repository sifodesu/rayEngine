#include <map>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

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
#include "portal.h"
#include "water.h"
#include "fog.h"
#include "particle_m.h"
#include "light_m.h"
#include "sprite.h"
#include "sprite_m.h"
#include "model_m.h"
#include "rlgl.h"
#include "rlImGui.h"
#include "imgui.h"
#include "imgui_layer.h"
#include "save_manager.h"

namespace {

// Debug visualization state
bool showCollisionBoxes = false;
constexpr float IMGUI_SCALE = 2.0f;

struct LayerBucket {
    std::vector<CollisionRect*> twod;
    std::vector<GObject*> threed;
};

void clearDepthBufferOnly()
{
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlColorMask(false, false, false, false);
    rlClearScreenBuffers();
    rlColorMask(true, true, true, true);
}

} // namespace

Engine::Engine()
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT, "rayEngine");
    SetTargetFPS(120);
    
    int scale = std::max(1, int(GetMonitorHeight(GetCurrentMonitor())*0.90 / NATIVE_RES_HEIGHT));
    SetWindowSize(NATIVE_RES_WIDTH * scale, NATIVE_RES_HEIGHT * scale);
    SetWindowPosition((GetMonitorWidth(GetCurrentMonitor()) - GetScreenWidth()) / 2, (GetMonitorHeight(GetCurrentMonitor()) - GetScreenHeight()) / 2);
    
    InitAudioDevice();

    loadGameContent();

    rlImGuiSetup(true);
    ImGui::GetStyle().ScaleAllSizes(IMGUI_SCALE);
    ImGui::GetStyle().FontScaleMain = IMGUI_SCALE;
}

void Engine::loadGameContent()
{
    Raycam_m::init();
    Texture_m::load();
    Model_m::load();
    Sprite_m::load();
    Sound_m::load();
    InputMap::init();
    UpgradeRegistry::initDefaults();
    Ldtk_m::loadLevel("ldtk_test.ldtk");
    // SaveManager::load(); // load save file (if exists)
    // SaveManager::applyToWorld(); // move player to saved checkpoint
    Shader_m::load();
    Sprite::resetGlitchParams();
    showCollisionBoxes = false;
}

void Engine::unloadGameContent()
{
    Object_m::unload();
    Ldtk_m::unload();
    Shader_m::unload();
    Sprite_m::unload();
    Model_m::unload();
    Texture_m::unload();
    Sound_m::unload();
    Ldtk_m::clearIdMapping();
}

void Engine::reloadGame()
{
    unloadGameContent();
    loadGameContent();
}

void Engine::game_loop()
{
    while (!WindowShouldClose()) {
        Clock::lap();

        if (IsKeyPressed(KEY_R)) {
            reloadGame();
        }

        Shader_m::routine();

        Shader_m::begin();
            ClearBackground(CLITERAL(Color){0, 0, 0, 255});
            Object_m::routine();
            Particle_m::update(static_cast<float>(Clock::getLap()));
            Raycam_m::getRayCam().routine();
            render();
        Shader_m::end();

        BeginDrawing();
            ClearBackground(BLACK);
            // if (Shader_m::has("roundpixels")) Shader_m::addFullscreen("roundpixels");
            if (Shader_m::has("lighting") && Light_m::hasActiveLights()) Shader_m::addFullscreen("lighting");
            if (Shader_m::has("crt")) Shader_m::addFullscreen("crt");
            Shader_m::present();
            DrawFPS(10, 10);

            // ImGui overlay
            ImGuiLayer::BeginFrame();
            ImGuiLayer::DrawWindows();
            ImGuiLayer::EndFrame();
        EndDrawing();
    }
}

void Engine::render()
{
    if (IsKeyPressed(KEY_C)) {
        showCollisionBoxes = !showCollisionBoxes;
    }

    Water::beginFrame();
    Fog::beginFrame();
    
    auto comp = [](CollisionRect* a, CollisionRect* b) {
        int layerA = a->getFather()->layer_;
        int layerB = b->getFather()->layer_;

        if (layerA == layerB) {
            return a < b;
        }

        return layerA < layerB;
    };
    std::set<CollisionRect*, decltype(comp)> sorted_bodies;
    std::vector<CollisionRect*> to_render = CollisionRect::query(Raycam_m::getRayCam().getRect());
    
    for (CollisionRect* body : to_render) sorted_bodies.insert(body);

    std::map<int, LayerBucket> layers;

    for (CollisionRect* body : sorted_bodies) {
        GObject* obj = body->getFather();
        if (!obj) continue;

        if (obj->is3DRenderable()) {
            layers[obj->layer_].threed.push_back(obj);
        } else {
            layers[obj->layer_].twod.push_back(body);
        }
    }

    bool in2D = false;
    bool in3D = false;
    bool rendered3DLayer = false;

    auto begin2D = [&]() {
        if (in2D) return;
        if (in3D) {
            EndMode3D();
            in3D = false;
        }
        BeginMode2D(Raycam_m::getCam());
        in2D = true;
    };

    auto begin3D = [&]() {
        if (in3D) return;
        if (in2D) {
            EndMode2D();
            in2D = false;
        }
        BeginMode3D(Raycam_m::getCam3D());
        in3D = true;
    };

    auto endDrawModes = [&]() {
        if (in3D) {
            EndMode3D();
            in3D = false;
        }
        if (in2D) {
            EndMode2D();
            in2D = false;
        }
    };

    begin2D();
    Ldtk_m::drawBackgrounds(Raycam_m::getRayCam().getRect());

    for (auto& [layer, bucket] : layers) {
        if (!bucket.twod.empty()) {
            begin2D();
            std::vector<Portal*> portals;
            for (CollisionRect* body : bucket.twod) {
                GObject* obj = body->getFather();
                if (!obj) continue;

                if (body->isRenderProxy() || Portal::isEntityInTransit(obj)) continue;
                if (Portal* portal = dynamic_cast<Portal*>(obj)) {
                    portals.push_back(portal);
                    continue;
                }
                obj->draw();
            }
            Portal::drawTransitsForLayer(layer);
            for (Portal* portal : portals) portal->draw();

            Water::drawOcclusionMasks();
            endDrawModes();
            Water::flushRefractionPasses();
            Fog::flushFogPasses();
        }

        if (!bucket.threed.empty()) {
            begin3D();
            if (rendered3DLayer) {
                clearDepthBufferOnly();
            }
            for (GObject* obj : bucket.threed) obj->draw3D();
            rendered3DLayer = true;
        }
    }

    begin2D();
    Particle_m::draw();
    Light_m::drawDebug();

    if (showCollisionBoxes) {
        begin2D();
        for (CollisionRect* body : sorted_bodies) {
            DrawRectangleRec(body->getSurface(), Fade(RED, 0.3));
        }
    }

    endDrawModes();
}

Engine::~Engine()
{
    unloadGameContent();
    rlImGuiShutdown();
    CloseWindow();
}
