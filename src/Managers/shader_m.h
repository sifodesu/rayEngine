// Refactored Shader Manager - minimal, multi-pass, area-aware
#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <utility>
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
        float phosphorTrail{0.42f};
        float phosphorDecayR{0.0060f};
        float phosphorDecayG{0.0030f};
        float phosphorDecayB{0.0012f};
        float phosphorSpread{0.55f};
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

    struct WaterParams {
        float stillReflectionShiftAmplitude{4.0f};
        float stillRippleSlowScale{0.43f};
        float stillRippleFastScale{0.91f};
        float stillRippleSlowSpeed{2.1f};
        float stillRippleFastSpeed{-4.4f};
        float stillRippleSlowWeight{0.70f};
        float stillRippleFastWeight{0.30f};
        float stillReflectionLineOffset{1.0f};

        Color stillOcclusionColor{0, 0, 0, 255};

        float waterfallShiftAmplitude{3.0f};
        float waterfallSegmentHeight{8.0f};
        float waterfallFlowSpeed{72.0f};
        float waterfallLineSpacing{5.0f};
        float waterfallLineWidth{1.0f};
        float waterfallLineLength{13.0f};
        float waterfallLinePeriod{28.0f};
        float waterfallLineIntensity{0.78f};
        float waterfallLineRandomSeed{13.7f};
    };

    static void load(const std::filesystem::path& dir = std::filesystem::path("Data")/"Shaders");
    static void unload();
    static void reload();

    static void begin();
    static void end();

    static void addFullscreen(const std::string& shader);
    static void addScreenArea(const std::string& shader, Rectangle screenRect);
    static void addWorldArea(const std::string& shader, Rectangle& worldRect);
    static void addWaterArea(Rectangle worldRect, int waterKind);
    static void applyWaterAreasToScene(const std::vector<std::pair<Rectangle, int>>& areas);
    static void present();

    static void routine();

    static bool has(const std::string& name);
    static Shader get(const std::string& name);
    static CRTParams& crtParams();
    static void resetCRTParams();
    static bool saveCRTParams();
    static bool loadCRTParams();
    static WaterParams& waterParams();
    static void resetWaterParams();
    static bool saveWaterParams();
    static bool loadWaterParams();

private:
    struct ShaderPair { std::filesystem::path vs; std::filesystem::path fs; };
    struct Pass { enum Type { Fullscreen, ScreenArea, WorldArea, WaterArea } type; std::string shader; Rectangle rect; int mode{0}; };

    static std::unordered_map<std::string, Shader> shaders_;
    static std::filesystem::path dir_;
    static RenderTexture2D sceneRT_;
    static RenderTexture2D postRT_;
    static RenderTexture2D prevSceneRT_; // previous presented post-scaled frame (for temporal shaders)
    static RenderTexture2D phosphorRT_[2];
    static RenderTexture2D nativePing_[2];
    static RenderTexture2D ping_[2];
    static Texture2D crtMaskTexture_;
    static Texture2D crtArtifactsTexture_;
    static int nativePingIndex_;
    static int pingIndex_;
    static int phosphorIndex_;
    static int lastW_, lastH_;
    static int lastScreenW_, lastScreenH_;
    static std::vector<Pass> queue_;

    // Hot reload
    static std::unordered_map<std::string, std::filesystem::file_time_type> fileTimes_;
    static bool detectChanges();
    static void snapshot();

    // Internal helpers
    static void ensureTargets();
    static void uploadPassUniforms(Shader shader, const std::string& name, Texture2D source);
    static void bindTemporalTextures(Shader shader, const std::string& name);
    static Texture2D updatePhosphorState(Texture2D source);
    static Texture2D applyPasses(Texture2D base, const std::vector<Pass>& passes, RenderTexture2D targets[2], int& targetIndex);
    static std::vector<std::pair<std::string, ShaderPair>> collect();
};
