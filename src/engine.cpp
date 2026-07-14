#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

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
#include "portal_m.h"
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

using json = nlohmann::json;

struct CaptureSuiteStep {
    const char* fileName;
    const char* modeName;
    bool crtEnabled;
    float testPattern;
    double settleSeconds;
    int minimumFrames;
};

constexpr std::array<CaptureSuiteStep, 8> CRT_CAPTURE_STEPS = {{
    {"00-game-raw.png", "game_raw", false, 0.0f, 0.20, 2},
    {"01-game-crt.png", "game_crt", true, 0.0f, 1.00, 8},
    {"02-bars-pluge-crt.png", "bars_pluge", true, 1.0f, 1.00, 8},
    {"03-convergence-crt.png", "convergence", true, 2.0f, 1.00, 8},
    {"04-multiburst-crt.png", "multiburst", true, 3.0f, 1.00, 8},
    {"05-half-screen-crt.png", "half_screen", true, 4.0f, 1.00, 8},
    {"06-zone-plate-crt.png", "zone_plate", true, 5.0f, 1.00, 8},
    {"07-grayscale-ramp-crt.png", "grayscale_ramp", true, 6.0f, 1.00, 8},
}};

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

Engine::Engine(int argc, char** argv)
{
    configureCaptureSuite(argc, argv);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT, "rayEngine");
    SetTargetFPS(120);
    
    if (captureSuiteEnabled_) {
        SetWindowSize(captureWidth_, captureHeight_);
    } else {
        int scale = std::max(1, int(
            GetMonitorHeight(GetCurrentMonitor()) * 0.90 /
            NATIVE_RES_HEIGHT));
        SetWindowSize(NATIVE_RES_WIDTH * scale,
            NATIVE_RES_HEIGHT * scale);
    }
    SetWindowPosition(
        std::max(0, (GetMonitorWidth(GetCurrentMonitor()) -
            GetScreenWidth()) / 2),
        std::max(0, (GetMonitorHeight(GetCurrentMonitor()) -
            GetScreenHeight()) / 2));
    
    InitAudioDevice();

    loadGameContent();

    rlImGuiSetup(true);
    ImGui::GetStyle().ScaleAllSizes(IMGUI_SCALE);
    ImGui::GetStyle().FontScaleMain = IMGUI_SCALE;

    if (captureSuiteEnabled_) beginCaptureSuite();
}

void Engine::configureCaptureSuite(int argc, char** argv)
{
    captureSuiteDirectory_ = std::filesystem::path("screenshots") /
        "crt-suite-latest";
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] ? argv[index] : "";
        if (argument == "--capture-crt-suite") {
            captureSuiteEnabled_ = true;
            if (index + 1 < argc && argv[index + 1] &&
                std::string(argv[index + 1]).rfind("--", 0) != 0) {
                captureSuiteDirectory_ = argv[++index];
            }
        } else if (argument == "--capture-size" && index + 1 < argc &&
                   argv[index + 1]) {
            const std::string dimensions = argv[++index];
            const std::size_t separator = dimensions.find_first_of("xX");
            try {
                if (separator == std::string::npos) throw std::invalid_argument("size");
                const int width = std::stoi(dimensions.substr(0, separator));
                const int height = std::stoi(dimensions.substr(separator + 1));
                if (width <= 0 || height <= 0) throw std::invalid_argument("size");
                captureWidth_ = width;
                captureHeight_ = height;
            } catch (const std::exception&) {
                std::cerr << "Invalid --capture-size '" << dimensions
                          << "'; expected WIDTHxHEIGHT\n";
            }
        }
    }
}

void Engine::beginCaptureSuite()
{
    std::error_code error;
    std::filesystem::create_directories(captureSuiteDirectory_, error);
    if (error) {
        std::cerr << "CRT capture: cannot create "
                  << captureSuiteDirectory_ << ": " << error.message()
                  << '\n';
        captureSuiteEnabled_ = false;
        return;
    }
    std::filesystem::copy_file(
        "crt_params.json",
        captureSuiteDirectory_ / "crt_params.json",
        std::filesystem::copy_options::overwrite_existing,
        error
    );
    if (error) {
        std::cerr << "CRT capture: parameter snapshot failed: "
                  << error.message() << '\n';
    }

    captureOriginalTestPattern_ = Shader_m::crtParams().testPattern;
    Clock::setSimulationPaused(true);
    Shader_m::setSceneTimeFrozen(true);
    captureStepIndex_ = 0;
    beginCaptureStep();
    std::cout << "CRT capture suite: "
              << std::filesystem::absolute(captureSuiteDirectory_).string()
              << '\n';
}

void Engine::beginCaptureStep()
{
    if (captureStepIndex_ < 0 ||
        captureStepIndex_ >= static_cast<int>(CRT_CAPTURE_STEPS.size())) {
        return;
    }
    const CaptureSuiteStep& step = CRT_CAPTURE_STEPS[captureStepIndex_];
    Shader_m::crtParams().testPattern = step.testPattern;
    if (step.crtEnabled) Shader_m::resetCRTHistory();
    captureStepStartedAt_ = GetTime();
    captureStepFrames_ = 0;
    captureQueued_ = false;
    captureAttempts_ = 0;
}

void Engine::queueCaptureForCurrentFrame()
{
    if (!captureSuiteEnabled_ || captureQueued_ || captureStepIndex_ < 0 ||
        captureStepIndex_ >= static_cast<int>(CRT_CAPTURE_STEPS.size())) {
        return;
    }
    const CaptureSuiteStep& step = CRT_CAPTURE_STEPS[captureStepIndex_];
    if (GetTime() - captureStepStartedAt_ < step.settleSeconds) return;
    if (captureStepFrames_ < step.minimumFrames) return;
    Shader_m::requestScreenshotTo(captureSuiteDirectory_ / step.fileName);
    captureQueued_ = true;
}

bool Engine::finishCaptureFrame()
{
    if (!captureSuiteEnabled_ || !captureQueued_) return false;
    const CaptureSuiteStep& step = CRT_CAPTURE_STEPS[captureStepIndex_];
    const std::filesystem::path expected = captureSuiteDirectory_ / step.fileName;
    if (!Shader_m::lastScreenshotSucceeded() ||
        Shader_m::lastScreenshotPath() != expected) {
        ++captureAttempts_;
        captureQueued_ = false;
        captureStepStartedAt_ = GetTime();
        if (captureAttempts_ < 3) return false;
        std::cerr << "CRT capture: failed after 3 attempts: "
                  << expected.string() << '\n';
        Shader_m::crtParams().testPattern = captureOriginalTestPattern_;
        Clock::setSimulationPaused(false);
        Shader_m::setSceneTimeFrozen(false);
        writeCaptureManifest(false);
        captureSuiteEnabled_ = false;
        return true;
    }

    std::cout << "CRT capture: " << step.modeName << " -> "
              << expected.string() << '\n';
    ++captureStepIndex_;
    if (captureStepIndex_ >= static_cast<int>(CRT_CAPTURE_STEPS.size())) {
        Shader_m::crtParams().testPattern = captureOriginalTestPattern_;
        Clock::setSimulationPaused(false);
        Shader_m::setSceneTimeFrozen(false);
        writeCaptureManifest(true);
        captureSuiteEnabled_ = false;
        return true;
    }
    beginCaptureStep();
    return false;
}

bool Engine::captureSuiteUsesCRT() const
{
    if (!captureSuiteEnabled_ || captureStepIndex_ < 0 ||
        captureStepIndex_ >= static_cast<int>(CRT_CAPTURE_STEPS.size())) {
        return true;
    }
    return CRT_CAPTURE_STEPS[captureStepIndex_].crtEnabled;
}

void Engine::writeCaptureManifest(bool complete) const
{
    json entries = json::array();
    for (const CaptureSuiteStep& step : CRT_CAPTURE_STEPS) {
        const std::filesystem::path imagePath =
            captureSuiteDirectory_ / step.fileName;
        entries.push_back({
            {"file", step.fileName},
            {"mode", step.modeName},
            {"crtEnabled", step.crtEnabled},
            {"testPattern", step.testPattern},
            {"settleSeconds", step.settleSeconds},
            {"minimumFrames", step.minimumFrames},
            {"captured", std::filesystem::is_regular_file(imagePath)}
        });
    }
    json manifest = {
        {"schema", "rayengine.crt-capture-suite.v1"},
        {"complete", complete},
        {"overlayFree", true},
        {"gameSimulationFrozen", true},
        {"sceneShaderTimeFrozen", true},
        {"crtTemporalStateResetPerMode", true},
        {"captureWidth", captureWidth_},
        {"captureHeight", captureHeight_},
        {"referenceRasterWidth", 3840},
        {"referenceRasterHeight", 2160},
        {"parameterSnapshot", "crt_params.json"},
        {"entries", entries}
    };
    std::ofstream file(captureSuiteDirectory_ / "manifest.json",
        std::ios::trunc);
    if (file.good()) file << manifest.dump(2);
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

        if (InputMap::checkPressed("reload")) {
            reloadGame();
        }
        const bool screenshotRequested = InputMap::checkPressed("screenshot");
        queueCaptureForCurrentFrame();

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
            if (Shader_m::has("crt") && captureSuiteUsesCRT()) {
                Shader_m::addFullscreen("crt");
            }
            if (screenshotRequested) Shader_m::requestScreenshot("manual");
            Shader_m::present();
            DrawFPS(10, 10);

            // ImGui overlay
            ImGuiLayer::BeginFrame();
            ImGuiLayer::DrawWindows();
            ImGuiLayer::EndFrame();
        EndDrawing();
        if (captureSuiteEnabled_) ++captureStepFrames_;
        if (finishCaptureFrame()) break;
    }
}

void Engine::render()
{
    if (InputMap::checkPressed("toggle_collision")) {
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

                if (body->isRenderProxy() || Portal_m::isInTransit(obj)) continue;
                if (Portal* portal = dynamic_cast<Portal*>(obj)) {
                    portals.push_back(portal);
                    continue;
                }
                obj->draw();
            }
            Portal_m::drawForLayer(layer);
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
    if (Clock::isSimulationPaused()) Clock::setSimulationPaused(false);
    Shader_m::setSceneTimeFrozen(false);
    unloadGameContent();
    rlImGuiShutdown();
    CloseWindow();
}
