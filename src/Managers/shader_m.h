// Refactored Shader Manager - minimal, multi-pass, area-aware
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include "raylib.h"

class Shader_m {
public:
    struct CRTParams {
        float curvature{0.0f};
        float vignette{0.0f};
        float edgeSoftness{0.0f};
        float glow{0.0f};
        float dotMask{0.0f};
        float dotBlur{0.82f};
        float bleed{0.0f};
        float dotGridSize{1.0f};
        float hexGrid{0.0f};
        float alternateLineShift{0.5f};
        float scanline{0.0f};
        float chromaticAberration{0.0f};
        float brightness{1.0f};
        float sharpness{0.0f};
        float persistence{0.0f};
        float ntscArtifacts{0.0f};
        float overscan{1.0f};
        float saturation{1.0f};
        float maskBrightness{0.0f};
        float maskOpacity{0.0f};
        float maskScale{1.0f};
        float bloomIntensity{0.0f};
        float bloomSpread{0.025f};
        float bloomPower{2.0f};
    };

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
    static CRTParams& crtParams();
    static void resetCRTParams();
    static bool saveCRTParams();
    static bool loadCRTParams();

private:
    struct ShaderPair { std::filesystem::path vs; std::filesystem::path fs; };
    struct Pass { enum Type { Fullscreen, ScreenArea, WorldArea } type; std::string shader; Rectangle rect; };

    static std::unordered_map<std::string, Shader> shaders_;
    static std::filesystem::path dir_;
    static RenderTexture2D sceneRT_;
    static RenderTexture2D postRT_;
    static RenderTexture2D prevSceneRT_; // previous presented post-scaled frame (for persistence shaders)
    static RenderTexture2D ping_[2];
    static Texture2D crtMaskTexture_;
    static Texture2D crtArtifactsTexture_;
    static int pingIndex_;
    static int lastW_, lastH_;
    static int lastScreenW_, lastScreenH_;
    static std::vector<Pass> queue_;

    // Hot reload
    static std::unordered_map<std::string, std::filesystem::file_time_type> fileTimes_;
    static bool detectChanges();
    static void snapshot();

    // Internal helpers
    static void ensureTargets();
    static void swapPing();
    static void uploadPassUniforms(Shader shader, const std::string& name, Texture2D source);
    static Texture2D applyQueue(Texture2D base);
    static std::vector<std::pair<std::string, ShaderPair>> collect();
};
