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
        std::string calibrationStatus{"provisional_unmeasured"};
        std::string measuredSerial{};
        std::string anodeVoltageStatus{"unmeasured"};
        float anodeVoltageKv{0.0f};
        float outputGamma{2.2f};
        float tubePeakNits{125.0f};
        float hostPeakNits{160.0f};
        // Mean linear radiance produced by the complete simulated chain for
        // the 50%-APL reference-white pattern. tubePeakNits is measured in
        // front of the faceplate, so this removes the arbitrary internal-unit
        // efficiency without cancelling relative mask, glass or APL effects.
        float referenceWhiteRadiance{0.48f};

        // Hitachi CT-1358 composite input and LA7621-era NTSC decoder.
        float ntscSourceLumaBandwidthMHz{3.00f};
        float ntscSourceIBandwidthMHz{1.30f};
        float ntscSourceQBandwidthMHz{0.50f};
        float ntscLumaBandwidthMHz{3.40f};
        float ntscChromaBandwidthIMHz{0.90f};
        float ntscChromaBandwidthQMHz{0.45f};
        float ntscChromaGain{1.0f};
        float ntscChromaDelayNs{75.0f};
        // Optional two-line presentation comb. The CT-1358 hardware has no
        // comb filter; this explicitly trades strict receiver fidelity for
        // less cross-colour on adversarial 320x192 pixel-art detail.
        float ntscLineCombStrength{0.90f};
        float ntscLumaPeaking{0.180f};
        float ntscDifferentialGain{0.0f};
        float ntscDifferentialPhaseDeg{0.0f};
        float ntscNoise{0.0035f};
        float ntscHum{0.0f};
        float ntscAgcResponse{0.120f};
        float ntscClampResponse{0.006f};
        float ntscBurstPllBandwidthHz{18.0f};
        float ntscHorizontalPllBandwidthHz{120.0f};
        float ntscVerticalPllBandwidthHz{8.0f};
        float ntscAccResponse{0.080f};
        float ntscColorKillerThreshold{0.18f};
        Vector3 videoGain{1.0f, 1.0f, 1.0f};
        Vector3 videoCutoff{0.008f, 0.008f, 0.008f};
        Vector3 gunGamma{2.35f, 2.35f, 2.35f};

        float beamMinWidth{0.20f};
        float beamMaxWidth{0.54f};
        float beamShape{2.2f};
        float beamIntensityWeight{0.38f};
        float beamScanlineStrength{0.88f};
        float beamHorizontalSigma{0.38f};
        float beamCurrentLimit{0.88f};
        float beamCurrentCompression{0.65f};
        float videoOutputBandwidthMHz{6.0f};
        float cathodeDriveHeadroom{1.0f};
        float spaceChargeCompression{0.12f};
        float spotBloom{0.14f};
        float dynamicFocus{0.04f};
        float focusEdgeSoftness{0.14f};
        float astigmatism{0.10f};
        float misconvergence{0.18f};
        float horizontalJitter{0.08f};

        float maskStrength{0.48f};
        float maskTriadsAcross{430.0f};
        float maskType{1.0f};
        float maskHeating{0.055f};
        float maskThermalTau{18.0f};
        float maskThermalDiffusion{0.14f};
        float maskDoming{0.018f};
        float maskCrosstalk{0.012f};

        Vector3 phosphorFastDecay{0.0014f, 0.0017f, 0.0010f};
        Vector3 phosphorMediumDecay{0.0045f, 0.0055f, 0.0035f};
        Vector3 phosphorSlowDecay{0.018f, 0.024f, 0.012f};
        float phosphorMediumWeight{0.065f};
        float phosphorSlowWeight{0.028f};
        float phosphorSpread{0.10f};
        float phosphorSaturation{3.0f};
        float observerIntegration{0.0f};

        float bloomThreshold{0.55f};
        float bloomIntensity{0.18f};
        float wideBloomIntensity{0.050f};
        float bloomRadius{0.95f};
        Vector3 bloomRadiusRGB{0.96f, 1.00f, 1.08f};
        float halation{0.025f};

        float curvatureX{0.055f};
        float curvatureY{0.075f};
        float pincushion{0.012f};
        float highVoltageBloom{0.014f};
        float highVoltageResponse{0.080f};
        float highVoltageSag{0.065f};
        float highVoltageRipple{0.0f};
        float bPlusResponse{0.035f};
        float bPlusSag{0.025f};
        float bPlusRipple{0.0f};
        float cornerRadius{0.105f};
        float vignette{0.20f};
        float glassTransmission{0.82f};
        Vector3 glassTint{1.0f, 1.0f, 1.0f};
        float glassDispersion{0.04f};
        float glassRefractiveIndex{1.52f};
        float glassThicknessMm{11.0f};
        Vector3 glassAbsorption{0.0060f, 0.0045f, 0.0035f};
        float internalReflection{0.012f};
        float ambientIlluminance{0.0f};
        float faceplateCurvatureX{0.012f};
        float faceplateCurvatureY{0.018f};
        Vector3 tubeColorMatrixR{1.0f, 0.0f, 0.0f};
        Vector3 tubeColorMatrixG{0.0f, 1.0f, 0.0f};
        Vector3 tubeColorMatrixB{0.0f, 0.0f, 1.0f};
        float reflection{0.0f};
        float blackLevel{0.0015f};
        float brightness{1.0f};
        float saturation{1.08f};
        float flicker{0.0f};
        float noise{0.0015f};
        float testPattern{0.0f};
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
    static void requestScreenshot(const std::string& label = "game");
    static void requestScreenshotTo(const std::filesystem::path& path);
    static const std::filesystem::path& lastScreenshotPath();
    static bool lastScreenshotSucceeded();
    static void resetCRTHistory();
    static void setSceneTimeFrozen(bool frozen);
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
    static RenderTexture2D heldVideoRT_;
    static RenderTexture2D pendingVideoRT_;
    static RenderTexture2D signalRT_[2];
    static RenderTexture2D sourceYiqRT_;
    static RenderTexture2D burstRT_;
    static RenderTexture2D receiverBurstRT_[2];
    static RenderTexture2D receiverVideoRT_[2];
    static RenderTexture2D receiverVerticalRT_[2];
    static RenderTexture2D phosphorRT_[2];
    static RenderTexture2D phosphorMediumRT_[2];
    static RenderTexture2D phosphorSlowRT_[2];
    static RenderTexture2D previousEmissionRT_;
    static RenderTexture2D currentEmissionRT_;
    static RenderTexture2D observerRT_[2];
    static RenderTexture2D aplRT_[2];
    static RenderTexture2D previousDriveRT_;
    static RenderTexture2D currentDriveRT_;
    static RenderTexture2D maskThermalRT_[2];
    static RenderTexture2D nativePing_[2];
    static RenderTexture2D ping_[2];
    static RenderTexture2D bloomRT_[2];
    static RenderTexture2D bloomWideRT_[2];
    static int nativePingIndex_;
    static int pingIndex_;
    static int phosphorIndex_;
    static int observerIndex_;
    static int aplIndex_;
    static int maskThermalIndex_;
    static int receiverBurstIndex_;
    static int receiverVideoIndex_;
    static int receiverVerticalIndex_;
    static long long heldVideoFrame_;
    static bool pendingVideoValid_;
    static bool emissionHistoryValid_;
    static bool driveHistoryValid_;
    static int lastW_, lastH_;
    static int lastScreenW_, lastScreenH_;
    static std::vector<Pass> queue_;
    static std::vector<std::filesystem::path> screenshotQueue_;
    static std::filesystem::path lastScreenshotPath_;
    static bool lastScreenshotSucceeded_;

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
    static Texture2D updateAPLState(Texture2D source);
    static Texture2D updateMaskThermalState(Texture2D source);
    static Texture2D updateReceiverBurstState(Texture2D source);
    static Texture2D updateReceiverVideoState(Texture2D source);
    static Texture2D updateReceiverVerticalState(Texture2D source);
    static Texture2D runFullscreenPass(
        const std::string& name,
        Texture2D source,
        RenderTexture2D& target,
        const char* extraUniform = nullptr,
        Texture2D extraTexture = {},
        const char* secondExtraUniform = nullptr,
        Texture2D secondExtraTexture = {},
        const char* thirdExtraUniform = nullptr,
        Texture2D thirdExtraTexture = {},
        const char* fourthExtraUniform = nullptr,
        Texture2D fourthExtraTexture = {}
    );
    static Texture2D applyCRTPipeline(Texture2D source);
    static void captureRequestedScreenshots(bool crtApplied,
        Texture2D referenceOutput);
    static Texture2D applyPasses(Texture2D base, const std::vector<Pass>& passes, RenderTexture2D targets[2], int& targetIndex);
    static std::vector<std::pair<std::string, ShaderPair>> collect();
};
