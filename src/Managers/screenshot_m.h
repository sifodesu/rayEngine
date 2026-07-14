#pragma once

#include <filesystem>
#include <string>

#include "raylib.h"

class Screenshot_m {
public:
    enum class Stage {
        BeforeDisplayShaders,
        Final,
    };

    struct OutputSize {
        int width;
        int height;
    };

    static void configure(int argc, char** argv);
    static void initialize();
    static void shutdown();

    static OutputSize outputSize(int fallbackWidth, int fallbackHeight);

    static void request(const std::string& label = "game");
    static const std::filesystem::path& lastPath();
    static bool lastSucceeded();

    static void beginFrame();
    static void capture(Stage stage, Texture2D output, bool crtApplied);
    static bool endFrame();

    static bool shouldClose();
    static int exitCode();
};
