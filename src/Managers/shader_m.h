// Refactored Shader Manager - minimal, multi-pass, area-aware
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include "raylib.h"

class Shader_m {
public:
    static void load(const std::filesystem::path& dir = std::filesystem::path("Data")/"Shaders");
    static void unload();
    static void reload();

    static void begin();
    static void end();

    static void addFullscreen(const std::string& shader);
    static void addScreenArea(const std::string& shader, Rectangle screenRect);
    static void addWorldArea(const std::string& shader, Rectangle& worldRect);
    static void present();

    static void routine();

    static bool has(const std::string& name);
    static Shader get(const std::string& name);

private:
    struct ShaderPair { std::filesystem::path vs; std::filesystem::path fs; };
    struct Pass { enum Type { Fullscreen, ScreenArea, WorldArea } type; std::string shader; Rectangle rect; };

    static std::unordered_map<std::string, Shader> shaders_;
    static std::filesystem::path dir_;
    static RenderTexture2D sceneRT_;
    static RenderTexture2D prevSceneRT_; // previous presented frame (for persistence shaders)
    static RenderTexture2D ping_[2];
    static int pingIndex_;
    static int lastW_, lastH_;
    static std::vector<Pass> queue_;

    // Hot reload
    static std::unordered_map<std::string, std::filesystem::file_time_type> fileTimes_;
    static bool detectChanges();
    static void snapshot();

    // Internal helpers
    static void ensureTargets();
    static void swapPing();
    static Texture2D applyQueue();
    static std::vector<std::pair<std::string, ShaderPair>> collect();
};
