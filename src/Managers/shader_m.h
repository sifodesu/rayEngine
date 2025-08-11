#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include "raylib.h"

class Shader_m {
public:
    static void load(const std::filesystem::path& dir = std::filesystem::path("Data")/"Shaders");
    static void unload();
    static void reload();

    // Scene pass helpers
    static void beginScenePass();
    static void endScenePass();
    static void present();
    static void handleInput();
    static void routine(); // per-frame update (input, resize)
    static void blit(); // draw postprocessed scene inside an active BeginDrawing

    // Cycle controls
    static void next();
    static void prev();
    static void disable();

    static bool active();
    static Shader current();
    static const std::string& currentName();

private:
    struct Pair { std::filesystem::path vs; std::filesystem::path fs; };
    static std::vector<Shader> shaders;
    static std::vector<std::string> names;
    static int currentIndex;
    static std::filesystem::path shaderDir;
    static RenderTexture2D sceneTarget;
    static int lastW, lastH;

    static std::vector<std::pair<std::string, Pair>> collectPairs();
};
