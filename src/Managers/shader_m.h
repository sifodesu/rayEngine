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
        float persistenceR{0.0f};
        float persistenceG{0.0f};
        float persistenceB{0.0f};
        float ntscArtifacts{0.0f};
        float ntscAlternation{0.0f};
        float overscan{1.0f};
        float pixelRatio{1.0f};
        float dimming{0.0f};
        float saturation{1.0f};
        float maskBrightness{0.0f};
        float maskOpacity{0.0f};
        float maskScale{1.0f};
        float bloomIntensity{0.0f};
        float bloomDownsampleSpread{0.025f};
        float bloomUpsampleSpread{0.025f};
        float bloomSpread{0.025f};
        float bloomPower{2.0f};
        float geometry3D{0.0f};
        float frameEnabled{0.0f};
        float reflectionScalar{0.0f};
        float diffuseBrightness{0.0f};
        float specBrightness{0.0f};
        float specPower{50.0f};
        float fresnelBrightness{0.0f};
        Vector3 lightPos{-10.0f, -5.0f, 10.0f};
        Color frameColor{15, 15, 15, 255};
        float trueCRT{0.0f};
        float trueCRTDebugView{0.0f};
        float signalMode{0.0f};
        float lumaBandwidth{1.0f};
        float chromaBandwidth{1.0f};
        float chromaDelay{0.0f};
        float ntscPhase{0.0f};
        float dotCrawl{0.0f};
        float beamWidthX{0.0f};
        float beamWidthY{0.0f};
        float beamFocus{1.0f};
        float beamBloom{0.0f};
        float beamScanlineStrength{0.0f};
        float edgeDefocus{0.0f};
        Vector3 convergenceR{0.0f, 0.0f, 0.0f};
        Vector3 convergenceG{0.0f, 0.0f, 0.0f};
        Vector3 convergenceB{0.0f, 0.0f, 0.0f};
        float phosphorLayout{0.0f};
        float phosphorPitchPx{8.0f};
        float phosphorRoundness{0.8f};
        float phosphorGap{0.0f};
        float phosphorGain{1.0f};
        float blackMatrix{0.0f};
        float phosphorBleed{0.0f};
        float afterglowR{0.0f};
        float afterglowG{0.0f};
        float afterglowB{0.0f};
        float afterglowThreshold{0.0f};
        float glassDiffusion{0.0f};
        float halation{0.0f};
        float tubeGlow{0.0f};
        float whitePoint{1.0f};
        float inputGamma{1.0f};
        float outputGamma{1.0f};
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
    static void loadCRTSimPreset();
    static void loadTrueCRTPreset();
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
    static RenderTexture2D crtComposite_[2];
    static RenderTexture2D crtFullRT_;
    static RenderTexture2D crtDownsampleRT_;
    static RenderTexture2D crtUpsampleRT_;
    static RenderTexture2D trueSignalRT_;
    static RenderTexture2D trueBeamRT_;
    static RenderTexture2D trueEmissionRT_;
    static RenderTexture2D truePhosphor_[2];
    static Texture2D crtMaskTexture_;
    static Texture2D crtArtifactsTexture_;
    static Model crtScreenModel_;
    static Model crtFrameModel_;
    static bool crtScreenModelLoaded_;
    static bool crtFrameModelLoaded_;
    static int pingIndex_;
    static int crtCompositeIndex_;
    static bool crtEvenFrame_;
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
    static Texture2D applyCRTSim(Texture2D base);
    static Texture2D applyTrueCRT(Texture2D base);
    static bool renderCRTSim3D(Texture2D composite);
    static bool renderTrueCRT3D(Texture2D composite);
    static std::vector<std::pair<std::string, ShaderPair>> collect();
};
