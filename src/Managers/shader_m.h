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
        float outputGamma{2.2f};

        // Hitachi CT-1358 composite input and LA7621-era NTSC decoder.
        float ntscLumaBandwidthMHz{3.00f};
        float ntscChromaBandwidthIMHz{1.10f};
        float ntscChromaBandwidthQMHz{0.45f};
        float ntscChromaGain{0.92f};
        float ntscChromaDelayNs{75.0f};
        float ntscLumaPeaking{0.075f};
        float ntscNoise{0.0035f};
        float ntscHum{0.0015f};
        Vector3 videoGain{1.015f, 1.000f, 0.985f};
        Vector3 videoCutoff{0.010f, 0.008f, 0.012f};
        Vector3 gunGamma{2.34f, 2.38f, 2.30f};

        float beamMinWidth{0.20f};
        float beamMaxWidth{0.54f};
        float beamShape{2.2f};
        float beamIntensityWeight{0.38f};
        float beamScanlineStrength{0.94f};
        float beamHorizontalSigma{0.72f};
        float focusEdgeSoftness{0.42f};
        float astigmatism{0.18f};
        float misconvergence{0.18f};
        float horizontalJitter{0.08f};

        float maskStrength{0.42f};
        float maskTriadsAcross{430.0f};
        float maskType{1.0f};

        Vector3 phosphorFastDecay{0.0014f, 0.0017f, 0.0010f};
        Vector3 phosphorSlowDecay{0.018f, 0.024f, 0.012f};
        float phosphorSlowWeight{0.028f};
        float phosphorSpread{0.28f};
        float observerIntegration{0.018f};

        float bloomThreshold{0.62f};
        float bloomIntensity{0.18f};
        float wideBloomIntensity{0.075f};
        float bloomRadius{1.35f};
        float halation{0.038f};

        float curvatureX{0.055f};
        float curvatureY{0.075f};
        float pincushion{0.012f};
        float highVoltageBloom{0.014f};
        float overscan{1.035f};
        float cornerRadius{0.105f};
        float vignette{0.20f};
        float glassTransmission{0.82f};
        Vector3 glassTint{0.94f, 0.965f, 0.92f};
        float reflection{0.018f};
        float blackLevel{0.0015f};
        float brightness{1.32f};
        float saturation{1.03f};
        float flicker{0.002f};
        float noise{0.0015f};
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

    struct FogParams {
        Color color{184, 202, 208, 255};
        float opacity{0.28f};
        float scale{0.0028f};
        float speedX{1.1f};
        float speedY{-0.35f};
        float contrast{0.66f};
        float softness{32.0f};
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
    static void addFogArea(Rectangle worldRect);
    static void applyFogAreasToScene(const std::vector<Rectangle>& areas);
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
    static FogParams& fogParams();
    static void resetFogParams();
    static bool saveFogParams();
    static bool loadFogParams();

private:
    struct ShaderPair { std::filesystem::path vs; std::filesystem::path fs; };
    struct Pass { enum Type { Fullscreen, ScreenArea, WorldArea, WaterArea, FogArea } type; std::string shader; Rectangle rect; int mode{0}; };

    static std::unordered_map<std::string, Shader> shaders_;
    static std::filesystem::path dir_;
    static RenderTexture2D sceneRT_;
    static RenderTexture2D postRT_;
    static RenderTexture2D prevSceneRT_; // previous unprocessed video frame
    static RenderTexture2D phosphorRT_[2];
    static RenderTexture2D phosphorSlowRT_[2];
    static RenderTexture2D observerRT_[2];
    static RenderTexture2D nativePing_[2];
    static RenderTexture2D ping_[2];
    static RenderTexture2D bloomRT_[2];
    static RenderTexture2D bloomWideRT_[2];
    static int nativePingIndex_;
    static int pingIndex_;
    static int phosphorIndex_;
    static int observerIndex_;
    static bool videoHistoryValid_;
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
    static Texture2D updateObserverState(Texture2D source);
    static Texture2D runFullscreenPass(
        const std::string& name,
        Texture2D source,
        RenderTexture2D& target,
        const char* extraUniform = nullptr,
        Texture2D extraTexture = {},
        const char* secondExtraUniform = nullptr,
        Texture2D secondExtraTexture = {}
    );
    static Texture2D applyCRTPipeline(Texture2D source);
    static Texture2D applyPasses(Texture2D base, const std::vector<Pass>& passes, RenderTexture2D targets[2], int& targetIndex);
    static std::vector<std::pair<std::string, ShaderPair>> collect();
};
