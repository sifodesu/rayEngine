#include "shader_m.h"
#include "raycam_m.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <rlgl.h>
#include "definitions.h"
#include "light_m.h"

using json = nlohmann::json;

std::unordered_map<std::string, Shader> Shader_m::shaders_;
std::filesystem::path Shader_m::dir_;
RenderTexture2D Shader_m::sceneRT_{};
RenderTexture2D Shader_m::postRT_{};
RenderTexture2D Shader_m::prevSceneRT_{};
RenderTexture2D Shader_m::phosphorRT_[2] = {};
RenderTexture2D Shader_m::phosphorSlowRT_[2] = {};
RenderTexture2D Shader_m::observerRT_[2] = {};
RenderTexture2D Shader_m::nativePing_[2] = {};
RenderTexture2D Shader_m::ping_[2] = {};
RenderTexture2D Shader_m::bloomRT_[2] = {};
RenderTexture2D Shader_m::bloomWideRT_[2] = {};
int Shader_m::nativePingIndex_ = 0;
int Shader_m::pingIndex_ = 0;
int Shader_m::phosphorIndex_ = 0;
int Shader_m::observerIndex_ = 0;
bool Shader_m::videoHistoryValid_ = false;
int Shader_m::lastW_ = 0;
int Shader_m::lastH_ = 0;
int Shader_m::lastScreenW_ = 0;
int Shader_m::lastScreenH_ = 0;
std::vector<Shader_m::Pass> Shader_m::queue_;
std::unordered_map<std::string, std::filesystem::file_time_type> Shader_m::fileTimes_;

namespace {
constexpr const char* CRT_PARAMS_PATH = "crt_params.json";
constexpr const char* WATER_PARAMS_PATH = "water_params.json";
constexpr const char* FOG_PARAMS_PATH = "fog_params.json";
bool crtParamsLoaded = false;
bool waterParamsLoaded = false;
bool fogParamsLoaded = false;

float readFloat(const json& j, const char* key, float fallback) {
    if (!j.contains(key) || !j[key].is_number()) return fallback;
    return j[key].get<float>();
}

json vector3ToJson(Vector3 value) {
    return {value.x, value.y, value.z};
}

Vector3 readVector3(const json& j, const char* key, Vector3 fallback) {
    if (!j.contains(key) || !j[key].is_array() || j[key].size() != 3) {
        return fallback;
    }
    for (const json& component : j[key]) {
        if (!component.is_number()) return fallback;
    }
    return {
        j[key][0].get<float>(),
        j[key][1].get<float>(),
        j[key][2].get<float>()
    };
}

json colorToJson(Color color) {
    return {
        {"r", static_cast<int>(color.r)},
        {"g", static_cast<int>(color.g)},
        {"b", static_cast<int>(color.b)},
        {"a", static_cast<int>(color.a)}
    };
}

unsigned char readByte(const json& j, const char* key, unsigned char fallback) {
    if (!j.contains(key) || !j[key].is_number_integer()) return fallback;
    return static_cast<unsigned char>(std::clamp(j[key].get<int>(), 0, 255));
}

Color readColor(const json& j, const char* key, Color fallback) {
    if (!j.contains(key) || !j[key].is_object()) return fallback;
    const json& value = j[key];
    return {
        readByte(value, "r", fallback.r),
        readByte(value, "g", fallback.g),
        readByte(value, "b", fallback.b),
        readByte(value, "a", fallback.a)
    };
}

} // namespace

Shader_m::CRTParams& Shader_m::crtParams() {
    static CRTParams params;
    return params;
}

Shader_m::WaterParams& Shader_m::waterParams() {
    static WaterParams params;
    return params;
}

Shader_m::FogParams& Shader_m::fogParams() {
    static FogParams params;
    return params;
}

void Shader_m::resetCRTParams() {
    crtParams() = CRTParams{};
}

void Shader_m::resetWaterParams() {
    waterParams() = WaterParams{};
}

void Shader_m::resetFogParams() {
    fogParams() = FogParams{};
}

bool Shader_m::saveCRTParams() {
    const CRTParams& params = crtParams();
    json j = {
        {"profile", {
            {"manufacturer", "Hitachi"},
            {"model", "CT-1358"},
            {"year", 1985},
            {"market", "North America"},
            {"input", "composite NTSC"},
            {"jungleIC", "Sanyo LA7621"},
            {"combFilter", false},
            {"tube", "Philips A34JLN60X"},
            {"tubeSizeInches", 13.0},
            {"mask", "slot"},
            {"deflectionDegrees", 90.0},
            {"anodeVoltageKv", 22.0},
            {"horizontalRateHz", 15734.264},
            {"fieldRateHz", 59.94006},
            {"subcarrierMHz", 3.579545},
            {"note", "tube pitch, phosphor decay and optical coefficients are calibrated estimates"}
        }},
        {"outputGamma", params.outputGamma},
        {"ntscLumaBandwidthMHz", params.ntscLumaBandwidthMHz},
        {"ntscChromaBandwidthIMHz", params.ntscChromaBandwidthIMHz},
        {"ntscChromaBandwidthQMHz", params.ntscChromaBandwidthQMHz},
        {"ntscChromaGain", params.ntscChromaGain},
        {"ntscChromaDelayNs", params.ntscChromaDelayNs},
        {"ntscLumaPeaking", params.ntscLumaPeaking},
        {"ntscNoise", params.ntscNoise},
        {"ntscHum", params.ntscHum},
        {"videoGain", vector3ToJson(params.videoGain)},
        {"videoCutoff", vector3ToJson(params.videoCutoff)},
        {"gunGamma", vector3ToJson(params.gunGamma)},
        {"beamMinWidth", params.beamMinWidth},
        {"beamMaxWidth", params.beamMaxWidth},
        {"beamShape", params.beamShape},
        {"beamIntensityWeight", params.beamIntensityWeight},
        {"beamScanlineStrength", params.beamScanlineStrength},
        {"beamHorizontalSigma", params.beamHorizontalSigma},
        {"focusEdgeSoftness", params.focusEdgeSoftness},
        {"astigmatism", params.astigmatism},
        {"misconvergence", params.misconvergence},
        {"horizontalJitter", params.horizontalJitter},
        {"maskStrength", params.maskStrength},
        {"maskTriadsAcross", params.maskTriadsAcross},
        {"maskType", params.maskType},
        {"phosphorFastDecay", vector3ToJson(params.phosphorFastDecay)},
        {"phosphorSlowDecay", vector3ToJson(params.phosphorSlowDecay)},
        {"phosphorSlowWeight", params.phosphorSlowWeight},
        {"phosphorSpread", params.phosphorSpread},
        {"observerIntegration", params.observerIntegration},
        {"bloomThreshold", params.bloomThreshold},
        {"bloomIntensity", params.bloomIntensity},
        {"wideBloomIntensity", params.wideBloomIntensity},
        {"bloomRadius", params.bloomRadius},
        {"halation", params.halation},
        {"curvatureX", params.curvatureX},
        {"curvatureY", params.curvatureY},
        {"pincushion", params.pincushion},
        {"highVoltageBloom", params.highVoltageBloom},
        {"overscan", params.overscan},
        {"cornerRadius", params.cornerRadius},
        {"vignette", params.vignette},
        {"glassTransmission", params.glassTransmission},
        {"glassTint", vector3ToJson(params.glassTint)},
        {"reflection", params.reflection},
        {"blackLevel", params.blackLevel},
        {"brightness", params.brightness},
        {"saturation", params.saturation},
        {"flicker", params.flicker},
        {"noise", params.noise},
    };

    std::ofstream f(CRT_PARAMS_PATH, std::ios::trunc);
    if (!f.good()) return false;
    f << j.dump(2);
    return f.good();
}

bool Shader_m::loadCRTParams() {
    std::ifstream f(CRT_PARAMS_PATH);
    if (!f.good()) return false;

    try {
        json j;
        f >> j;
        CRTParams& params = crtParams();
        params.outputGamma = readFloat(j, "outputGamma", params.outputGamma);
        params.ntscLumaBandwidthMHz = readFloat(j, "ntscLumaBandwidthMHz", params.ntscLumaBandwidthMHz);
        params.ntscChromaBandwidthIMHz = readFloat(j, "ntscChromaBandwidthIMHz", params.ntscChromaBandwidthIMHz);
        params.ntscChromaBandwidthQMHz = readFloat(j, "ntscChromaBandwidthQMHz", params.ntscChromaBandwidthQMHz);
        params.ntscChromaGain = readFloat(j, "ntscChromaGain", params.ntscChromaGain);
        params.ntscChromaDelayNs = readFloat(j, "ntscChromaDelayNs", params.ntscChromaDelayNs);
        params.ntscLumaPeaking = readFloat(j, "ntscLumaPeaking", params.ntscLumaPeaking);
        params.ntscNoise = readFloat(j, "ntscNoise", params.ntscNoise);
        params.ntscHum = readFloat(j, "ntscHum", params.ntscHum);
        params.videoGain = readVector3(j, "videoGain", params.videoGain);
        params.videoCutoff = readVector3(j, "videoCutoff", params.videoCutoff);
        params.gunGamma = readVector3(j, "gunGamma", params.gunGamma);
        params.beamMinWidth = readFloat(j, "beamMinWidth", params.beamMinWidth);
        params.beamMaxWidth = readFloat(j, "beamMaxWidth", params.beamMaxWidth);
        params.beamShape = readFloat(j, "beamShape", params.beamShape);
        params.beamIntensityWeight = readFloat(j, "beamIntensityWeight", params.beamIntensityWeight);
        params.beamScanlineStrength = readFloat(j, "beamScanlineStrength", params.beamScanlineStrength);
        params.beamHorizontalSigma = readFloat(j, "beamHorizontalSigma", params.beamHorizontalSigma);
        params.focusEdgeSoftness = readFloat(j, "focusEdgeSoftness", params.focusEdgeSoftness);
        params.astigmatism = readFloat(j, "astigmatism", params.astigmatism);
        params.misconvergence = readFloat(j, "misconvergence", params.misconvergence);
        params.horizontalJitter = readFloat(j, "horizontalJitter", params.horizontalJitter);
        params.maskStrength = readFloat(j, "maskStrength", params.maskStrength);
        params.maskTriadsAcross = readFloat(j, "maskTriadsAcross", params.maskTriadsAcross);
        params.maskType = readFloat(j, "maskType", params.maskType);
        params.phosphorFastDecay = readVector3(j, "phosphorFastDecay", params.phosphorFastDecay);
        params.phosphorSlowDecay = readVector3(j, "phosphorSlowDecay", params.phosphorSlowDecay);
        params.phosphorSlowWeight = readFloat(j, "phosphorSlowWeight", params.phosphorSlowWeight);
        params.phosphorSpread = readFloat(j, "phosphorSpread", params.phosphorSpread);
        params.observerIntegration = readFloat(j, "observerIntegration", params.observerIntegration);
        params.bloomThreshold = readFloat(j, "bloomThreshold", params.bloomThreshold);
        params.bloomIntensity = readFloat(j, "bloomIntensity", params.bloomIntensity);
        params.wideBloomIntensity = readFloat(j, "wideBloomIntensity", params.wideBloomIntensity);
        params.bloomRadius = readFloat(j, "bloomRadius", params.bloomRadius);
        params.halation = readFloat(j, "halation", params.halation);
        params.curvatureX = readFloat(j, "curvatureX", params.curvatureX);
        params.curvatureY = readFloat(j, "curvatureY", params.curvatureY);
        params.pincushion = readFloat(j, "pincushion", params.pincushion);
        params.highVoltageBloom = readFloat(j, "highVoltageBloom", params.highVoltageBloom);
        params.overscan = readFloat(j, "overscan", params.overscan);
        params.cornerRadius = readFloat(j, "cornerRadius", params.cornerRadius);
        params.vignette = readFloat(j, "vignette", params.vignette);
        params.glassTransmission = readFloat(j, "glassTransmission", params.glassTransmission);
        params.glassTint = readVector3(j, "glassTint", params.glassTint);
        params.reflection = readFloat(j, "reflection", params.reflection);
        params.blackLevel = readFloat(j, "blackLevel", params.blackLevel);
        params.brightness = readFloat(j, "brightness", params.brightness);
        params.saturation = readFloat(j, "saturation", params.saturation);
        params.flicker = readFloat(j, "flicker", params.flicker);
        params.noise = readFloat(j, "noise", params.noise);
        return true;
    } catch (...) {
        return false;
    }
}

bool Shader_m::saveWaterParams() {
    const WaterParams& params = waterParams();
    json j = {
        {"stillReflectionShiftAmplitude", params.stillReflectionShiftAmplitude},
        {"stillRippleSlowScale", params.stillRippleSlowScale},
        {"stillRippleFastScale", params.stillRippleFastScale},
        {"stillRippleSlowSpeed", params.stillRippleSlowSpeed},
        {"stillRippleFastSpeed", params.stillRippleFastSpeed},
        {"stillRippleSlowWeight", params.stillRippleSlowWeight},
        {"stillRippleFastWeight", params.stillRippleFastWeight},
        {"stillReflectionLineOffset", params.stillReflectionLineOffset},
        {"stillOcclusionColor", colorToJson(params.stillOcclusionColor)},
        {"waterfallShiftAmplitude", params.waterfallShiftAmplitude},
        {"waterfallSegmentHeight", params.waterfallSegmentHeight},
        {"waterfallFlowSpeed", params.waterfallFlowSpeed},
        {"waterfallLineSpacing", params.waterfallLineSpacing},
        {"waterfallLineWidth", params.waterfallLineWidth},
        {"waterfallLineLength", params.waterfallLineLength},
        {"waterfallLinePeriod", params.waterfallLinePeriod},
        {"waterfallLineIntensity", params.waterfallLineIntensity},
        {"waterfallLineRandomSeed", params.waterfallLineRandomSeed},
    };

    std::ofstream f(WATER_PARAMS_PATH, std::ios::trunc);
    if (!f.good()) return false;
    f << j.dump(2);
    return f.good();
}

bool Shader_m::loadWaterParams() {
    std::ifstream f(WATER_PARAMS_PATH);
    if (!f.good()) return false;

    try {
        json j;
        f >> j;
        WaterParams& params = waterParams();
        params.stillReflectionShiftAmplitude = readFloat(j, "stillReflectionShiftAmplitude", params.stillReflectionShiftAmplitude);
        params.stillRippleSlowScale = readFloat(j, "stillRippleSlowScale", params.stillRippleSlowScale);
        params.stillRippleFastScale = readFloat(j, "stillRippleFastScale", params.stillRippleFastScale);
        params.stillRippleSlowSpeed = readFloat(j, "stillRippleSlowSpeed", params.stillRippleSlowSpeed);
        params.stillRippleFastSpeed = readFloat(j, "stillRippleFastSpeed", params.stillRippleFastSpeed);
        params.stillRippleSlowWeight = readFloat(j, "stillRippleSlowWeight", params.stillRippleSlowWeight);
        params.stillRippleFastWeight = readFloat(j, "stillRippleFastWeight", params.stillRippleFastWeight);
        params.stillReflectionLineOffset = readFloat(j, "stillReflectionLineOffset", params.stillReflectionLineOffset);
        params.stillOcclusionColor = readColor(j, "stillOcclusionColor", params.stillOcclusionColor);
        params.waterfallShiftAmplitude = readFloat(j, "waterfallShiftAmplitude", params.waterfallShiftAmplitude);
        params.waterfallSegmentHeight = readFloat(j, "waterfallSegmentHeight", params.waterfallSegmentHeight);
        params.waterfallFlowSpeed = readFloat(j, "waterfallFlowSpeed", params.waterfallFlowSpeed);
        if (params.waterfallFlowSpeed < 0.0f) params.waterfallFlowSpeed = -params.waterfallFlowSpeed;
        params.waterfallLineSpacing = readFloat(j, "waterfallLineSpacing", params.waterfallLineSpacing);
        params.waterfallLineWidth = readFloat(j, "waterfallLineWidth", params.waterfallLineWidth);
        params.waterfallLineLength = readFloat(j, "waterfallLineLength", params.waterfallLineLength);
        params.waterfallLinePeriod = readFloat(j, "waterfallLinePeriod", params.waterfallLinePeriod);
        params.waterfallLineIntensity = readFloat(j, "waterfallLineIntensity", params.waterfallLineIntensity);
        params.waterfallLineRandomSeed = readFloat(j, "waterfallLineRandomSeed", params.waterfallLineRandomSeed);
        return true;
    } catch (...) {
        return false;
    }
}

bool Shader_m::saveFogParams() {
    const FogParams& params = fogParams();
    json j = {
        {"color", colorToJson(params.color)},
        {"opacity", params.opacity},
        {"scale", params.scale},
        {"speedX", params.speedX},
        {"speedY", params.speedY},
        {"contrast", params.contrast},
        {"softness", params.softness},
    };

    std::ofstream f(FOG_PARAMS_PATH, std::ios::trunc);
    if (!f.good()) return false;
    f << j.dump(2);
    return f.good();
}

bool Shader_m::loadFogParams() {
    std::ifstream f(FOG_PARAMS_PATH);
    if (!f.good()) return false;

    try {
        json j;
        f >> j;
        FogParams& params = fogParams();
        params.color = readColor(j, "color", params.color);
        params.opacity = readFloat(j, "opacity", params.opacity);
        params.scale = readFloat(j, "scale", params.scale);
        params.speedX = readFloat(j, "speedX", params.speedX);
        params.speedY = readFloat(j, "speedY", params.speedY);
        params.contrast = readFloat(j, "contrast", params.contrast);
        params.softness = readFloat(j, "softness", params.softness);
        return true;
    } catch (...) {
        return false;
    }
}

static RenderTexture2D loadPointRenderTexture(int width, int height) {
    RenderTexture2D target = LoadRenderTexture(width, height);
    if (target.id) {
        SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);
    }
    return target;
}

static RenderTexture2D loadHDRRenderTexture(int width, int height) {
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    unsigned int textureId = rlLoadTexture(
        nullptr,
        width,
        height,
        RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,
        1
    );
    if (!target.id || !textureId) {
        if (textureId) rlUnloadTexture(textureId);
        if (target.id) rlUnloadFramebuffer(target.id);
        TraceLog(LOG_WARNING, "CRT: RGBA16F target unavailable, using RGBA8");
        return loadPointRenderTexture(width, height);
    }

    target.texture = {
        textureId,
        width,
        height,
        1,
        PIXELFORMAT_UNCOMPRESSED_R16G16B16A16
    };
    rlFramebufferAttach(
        target.id,
        target.texture.id,
        RL_ATTACHMENT_COLOR_CHANNEL0,
        RL_ATTACHMENT_TEXTURE2D,
        0
    );
    if (!rlFramebufferComplete(target.id)) {
        rlUnloadTexture(target.texture.id);
        rlUnloadFramebuffer(target.id);
        TraceLog(LOG_WARNING, "CRT: incomplete RGBA16F framebuffer, using RGBA8");
        return loadPointRenderTexture(width, height);
    }

    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(target.texture, TEXTURE_WRAP_CLAMP);
    return target;
}

static void clearRenderTexture(RenderTexture2D target) {
    if (!target.id) return;
    BeginTextureMode(target);
        ClearBackground(BLACK);
    EndTextureMode();
}

// Helper: collect shader pairs
std::vector<std::pair<std::string, Shader_m::ShaderPair>> Shader_m::collect() {
    std::vector<std::pair<std::string, ShaderPair>> list;
    std::error_code ec;
    if (!std::filesystem::exists(dir_, ec)) return list;
    struct Acc { std::filesystem::path vs; std::filesystem::path fs; };
    std::unordered_map<std::string, Acc> m;
    for (auto &p : std::filesystem::directory_iterator(dir_, ec)) {
        if (!p.is_regular_file()) continue;
        auto ext = p.path().extension().string();
        for (auto &c: ext) c = (char)tolower(c);
        std::string stem = p.path().stem().string();
        if (ext == ".vs") m[stem].vs = p.path();
        else if (ext == ".fs") m[stem].fs = p.path();
    }
    for (auto &kv : m) if (!kv.second.fs.empty() || !kv.second.vs.empty()) list.push_back({kv.first, {kv.second.vs, kv.second.fs}});
    std::sort(list.begin(), list.end(), [](auto &a, auto &b){ return a.first < b.first; });
    return list;
}

void Shader_m::load(const std::filesystem::path& dir) {
    dir_ = dir;
    if (!crtParamsLoaded) {
        loadCRTParams();
        crtParamsLoaded = true;
    }
    if (!waterParamsLoaded) {
        loadWaterParams();
        waterParamsLoaded = true;
    }
    if (!fogParamsLoaded) {
        loadFogParams();
        fogParamsLoaded = true;
    }
    unload();
    lastW_ = NATIVE_RES_WIDTH;
    lastH_ = NATIVE_RES_HEIGHT;
    lastScreenW_ = std::max(GetScreenWidth(), 1);
    lastScreenH_ = std::max(GetScreenHeight(), 1);
    sceneRT_ = loadPointRenderTexture(lastW_, lastH_);
    nativePing_[0] = loadPointRenderTexture(lastW_, lastH_);
    nativePing_[1] = loadPointRenderTexture(lastW_, lastH_);
    postRT_ = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    prevSceneRT_ = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    phosphorRT_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorRT_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorSlowRT_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorSlowRT_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    observerRT_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    observerRT_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    ping_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    ping_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    int bloomW = std::max(lastScreenW_ / 4, 1);
    int bloomH = std::max(lastScreenH_ / 4, 1);
    bloomRT_[0] = loadHDRRenderTexture(bloomW, bloomH);
    bloomRT_[1] = loadHDRRenderTexture(bloomW, bloomH);
    int wideBloomW = std::max(lastScreenW_ / 16, 1);
    int wideBloomH = std::max(lastScreenH_ / 16, 1);
    bloomWideRT_[0] = loadHDRRenderTexture(wideBloomW, wideBloomH);
    bloomWideRT_[1] = loadHDRRenderTexture(wideBloomW, wideBloomH);
    nativePingIndex_ = 0;
    phosphorIndex_ = 0;
    observerIndex_ = 0;
    videoHistoryValid_ = false;
    clearRenderTexture(prevSceneRT_);
    clearRenderTexture(phosphorRT_[0]);
    clearRenderTexture(phosphorRT_[1]);
    clearRenderTexture(phosphorSlowRT_[0]);
    clearRenderTexture(phosphorSlowRT_[1]);
    clearRenderTexture(observerRT_[0]);
    clearRenderTexture(observerRT_[1]);
    clearRenderTexture(bloomRT_[0]);
    clearRenderTexture(bloomRT_[1]);
    clearRenderTexture(bloomWideRT_[0]);
    clearRenderTexture(bloomWideRT_[1]);
    for (auto &pr : collect()) {
        std::string vsPath = pr.second.vs.empty() ? std::string{} : pr.second.vs.string();
        std::string fsPath = pr.second.fs.empty() ? std::string{} : pr.second.fs.string();
        const char* vs = vsPath.empty() ? nullptr : vsPath.c_str();
        const char* fs = fsPath.empty() ? nullptr : fsPath.c_str();
        Shader sh = LoadShader(vs, fs);
        if (sh.id) shaders_[pr.first] = sh;
    }
    snapshot();
}

void Shader_m::unload() {
    if (sceneRT_.id) { UnloadRenderTexture(sceneRT_); sceneRT_.id = 0; }
    if (postRT_.id) { UnloadRenderTexture(postRT_); postRT_.id = 0; }
    if (prevSceneRT_.id) { UnloadRenderTexture(prevSceneRT_); prevSceneRT_.id = 0; }
    for (auto &r : phosphorRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : phosphorSlowRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : observerRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : nativePing_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : ping_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : bloomRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : bloomWideRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &kv : shaders_) if (kv.second.id) UnloadShader(kv.second);
    shaders_.clear();
    queue_.clear();
}

void Shader_m::reload() { load(dir_); }

bool Shader_m::has(const std::string& name) { return shaders_.count(name)!=0; }

Shader Shader_m::get(const std::string& name) { auto it = shaders_.find(name); return it==shaders_.end()? Shader{0} : it->second; }

void Shader_m::ensureTargets() {
    int w = NATIVE_RES_WIDTH;
    int h = NATIVE_RES_HEIGHT;
    int sw = std::max(GetScreenWidth(), 1);
    int sh = std::max(GetScreenHeight(), 1);

    if (w != lastW_ || h != lastH_ || !sceneRT_.id || !nativePing_[0].id || !nativePing_[1].id) {
        if (sceneRT_.id) UnloadRenderTexture(sceneRT_);
        for (auto &r: nativePing_) if (r.id) UnloadRenderTexture(r);
        sceneRT_ = loadPointRenderTexture(w,h);
        nativePing_[0] = loadPointRenderTexture(w,h);
        nativePing_[1] = loadPointRenderTexture(w,h);
        nativePingIndex_ = 0;
        lastW_ = w; lastH_ = h;
    }

    if (sw == lastScreenW_ && sh == lastScreenH_ && postRT_.id &&
        prevSceneRT_.id && phosphorRT_[0].id && phosphorRT_[1].id &&
        phosphorSlowRT_[0].id && phosphorSlowRT_[1].id &&
        observerRT_[0].id && observerRT_[1].id &&
        ping_[0].id && ping_[1].id && bloomRT_[0].id && bloomRT_[1].id &&
        bloomWideRT_[0].id && bloomWideRT_[1].id) {
        return;
    }

    if (postRT_.id) UnloadRenderTexture(postRT_);
    if (prevSceneRT_.id) UnloadRenderTexture(prevSceneRT_);
    for (auto &r: phosphorRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: phosphorSlowRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: observerRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: ping_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: bloomRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: bloomWideRT_) if (r.id) UnloadRenderTexture(r);
    postRT_ = loadPointRenderTexture(sw,sh);
    prevSceneRT_ = loadPointRenderTexture(sw,sh);
    phosphorRT_[0] = loadHDRRenderTexture(sw,sh);
    phosphorRT_[1] = loadHDRRenderTexture(sw,sh);
    phosphorSlowRT_[0] = loadHDRRenderTexture(sw,sh);
    phosphorSlowRT_[1] = loadHDRRenderTexture(sw,sh);
    observerRT_[0] = loadHDRRenderTexture(sw,sh);
    observerRT_[1] = loadHDRRenderTexture(sw,sh);
    ping_[0] = loadHDRRenderTexture(sw,sh);
    ping_[1] = loadHDRRenderTexture(sw,sh);
    int bloomW = std::max(sw / 4, 1);
    int bloomH = std::max(sh / 4, 1);
    bloomRT_[0] = loadHDRRenderTexture(bloomW, bloomH);
    bloomRT_[1] = loadHDRRenderTexture(bloomW, bloomH);
    int wideBloomW = std::max(sw / 16, 1);
    int wideBloomH = std::max(sh / 16, 1);
    bloomWideRT_[0] = loadHDRRenderTexture(wideBloomW, wideBloomH);
    bloomWideRT_[1] = loadHDRRenderTexture(wideBloomW, wideBloomH);
    phosphorIndex_ = 0;
    observerIndex_ = 0;
    videoHistoryValid_ = false;
    clearRenderTexture(prevSceneRT_);
    clearRenderTexture(phosphorRT_[0]);
    clearRenderTexture(phosphorRT_[1]);
    clearRenderTexture(phosphorSlowRT_[0]);
    clearRenderTexture(phosphorSlowRT_[1]);
    clearRenderTexture(observerRT_[0]);
    clearRenderTexture(observerRT_[1]);
    clearRenderTexture(bloomRT_[0]);
    clearRenderTexture(bloomRT_[1]);
    clearRenderTexture(bloomWideRT_[0]);
    clearRenderTexture(bloomWideRT_[1]);
    lastScreenW_ = sw; lastScreenH_ = sh;
}

void Shader_m::begin() { ensureTargets(); BeginTextureMode(sceneRT_); }

void Shader_m::end() { EndTextureMode(); }

void Shader_m::addFullscreen(const std::string& shader) {
    if (shader.empty() || !has(shader)) return; queue_.push_back({Pass::Fullscreen, shader, {0,0,0,0}});
}

void Shader_m::addScreenArea(const std::string& shader, Rectangle screenRect) {
    if (!has(shader)) return; queue_.push_back({Pass::ScreenArea, shader, screenRect});
}

void Shader_m::addWorldArea(const std::string& shader, Rectangle& worldRect) {
    if (!has(shader)) return; queue_.push_back({Pass::WorldArea, shader, worldRect});
}

void Shader_m::addWaterArea(Rectangle worldRect, int waterKind) {
    if (!has("water_refraction")) return;
    queue_.push_back({Pass::WaterArea, "water_refraction", worldRect, waterKind});
}

void Shader_m::applyWaterAreasToScene(const std::vector<std::pair<Rectangle, int>>& areas) {
    if (areas.empty() || !has("water_refraction")) return;

    ensureTargets();

    std::vector<Pass> passes;
    passes.reserve(areas.size());
    for (const auto& [rect, waterKind] : areas) {
        passes.push_back({Pass::WaterArea, "water_refraction", rect, waterKind});
    }

    // Layer-local water must be baked into the scene target before higher layers render.
    EndTextureMode();
    Texture2D waterTex = applyPasses(sceneRT_.texture, passes, nativePing_, nativePingIndex_);

    BeginTextureMode(sceneRT_);
        ClearBackground(BLACK);
        DrawTextureRec(waterTex, {0, 0, (float)waterTex.width, -(float)waterTex.height}, {0, 0}, WHITE);
    EndTextureMode();

    BeginTextureMode(sceneRT_);
}

void Shader_m::addFogArea(Rectangle worldRect) {
    if (!has("fog")) return;
    queue_.push_back({Pass::FogArea, "fog", worldRect});
}

void Shader_m::applyFogAreasToScene(const std::vector<Rectangle>& areas) {
    if (areas.empty() || !has("fog")) return;

    ensureTargets();

    std::vector<Pass> passes;
    passes.reserve(areas.size());
    for (Rectangle rect : areas) {
        passes.push_back({Pass::FogArea, "fog", rect});
    }

    EndTextureMode();
    Texture2D fogTex = applyPasses(sceneRT_.texture, passes, nativePing_, nativePingIndex_);

    BeginTextureMode(sceneRT_);
        ClearBackground(BLACK);
        DrawTextureRec(fogTex, {0, 0, (float)fogTex.width, -(float)fogTex.height}, {0, 0}, WHITE);
    EndTextureMode();

    BeginTextureMode(sceneRT_);
}

static Rectangle getLetterboxRect(int srcW, int srcH, int targetW, int targetH);
static Rectangle getCRTDisplayRect(int targetW, int targetH);

void Shader_m::uploadPassUniforms(Shader shader, const std::string& name, Texture2D source) {
    float time = (float)GetTime();
    float frameTime = std::clamp((float)GetFrameTime(), 1.0f / 240.0f, 1.0f / 24.0f);
    float resolution[2] = { (float)source.width, (float)source.height };
    bool crtStage = name == "crt" || name == "phosphor_state" ||
                    name.rfind("crt_", 0) == 0;
    Rectangle display = crtStage
        ? getCRTDisplayRect(source.width, source.height)
        : getLetterboxRect(NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT, source.width, source.height);
    float nativeResolution[2] = { (float)NATIVE_RES_WIDTH, (float)NATIVE_RES_HEIGHT };
    float displayRect[4] = { display.x, display.y, display.width, display.height };

    int loc = GetShaderLocation(shader, "time");
    if (loc >= 0) SetShaderValue(shader, loc, &time, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "frameTime");
    if (loc >= 0) SetShaderValue(shader, loc, &frameTime, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "resolution");
    if (loc >= 0) SetShaderValue(shader, loc, resolution, SHADER_UNIFORM_VEC2);
    loc = GetShaderLocation(shader, "nativeResolution");
    if (loc >= 0) SetShaderValue(shader, loc, nativeResolution, SHADER_UNIFORM_VEC2);
    loc = GetShaderLocation(shader, "displayRect");
    if (loc >= 0) SetShaderValue(shader, loc, displayRect, SHADER_UNIFORM_VEC4);

    if (name == "lighting") {
        Light_m::upload(shader);
        return;
    }

    if (!crtStage) return;

    CRTParams& params = crtParams();
    auto setFloat = [&](const char* uniform, float value) {
        int uniformLoc = GetShaderLocation(shader, uniform);
        if (uniformLoc >= 0) {
            SetShaderValue(shader, uniformLoc, &value, SHADER_UNIFORM_FLOAT);
        }
    };
    auto setVec3 = [&](const char* uniform, Vector3 value) {
        int uniformLoc = GetShaderLocation(shader, uniform);
        if (uniformLoc >= 0) {
            float components[3] = {value.x, value.y, value.z};
            SetShaderValue(shader, uniformLoc, components, SHADER_UNIFORM_VEC3);
        }
    };

    // Fixed timing and electrical facts for the North-American CT-1358.
    setFloat("ntscSubcarrierMHz", 3.579545f);
    setFloat("ntscLineRateHz", 15734.264f);
    setFloat("ntscFrameRateHz", 59.94006f);
    setFloat("ntscActiveVideoUs", 52.655f);
    setFloat("videoHistoryValid", videoHistoryValid_ ? 1.0f : 0.0f);
    setFloat("outputGamma", params.outputGamma);
    setFloat("ntscLumaBandwidthMHz", params.ntscLumaBandwidthMHz);
    setFloat("ntscChromaBandwidthIMHz", params.ntscChromaBandwidthIMHz);
    setFloat("ntscChromaBandwidthQMHz", params.ntscChromaBandwidthQMHz);
    setFloat("ntscChromaGain", params.ntscChromaGain);
    setFloat("ntscChromaDelayNs", params.ntscChromaDelayNs);
    setFloat("ntscLumaPeaking", params.ntscLumaPeaking);
    setFloat("ntscNoise", params.ntscNoise);
    setFloat("ntscHum", params.ntscHum);
    setVec3("videoGain", params.videoGain);
    setVec3("videoCutoff", params.videoCutoff);
    setVec3("gunGamma", params.gunGamma);
    setFloat("beamMinWidth", params.beamMinWidth);
    setFloat("beamMaxWidth", params.beamMaxWidth);
    setFloat("beamShape", params.beamShape);
    setFloat("beamIntensityWeight", params.beamIntensityWeight);
    setFloat("beamScanlineStrength", params.beamScanlineStrength);
    setFloat("beamHorizontalSigma", params.beamHorizontalSigma);
    setFloat("focusEdgeSoftness", params.focusEdgeSoftness);
    setFloat("astigmatism", params.astigmatism);
    setFloat("misconvergence", params.misconvergence);
    setFloat("horizontalJitter", params.horizontalJitter);
    setFloat("maskStrength", params.maskStrength);
    setFloat("maskTriadsAcross", params.maskTriadsAcross);
    setFloat("maskType", params.maskType);
    setVec3("phosphorFastDecay", params.phosphorFastDecay);
    setVec3("phosphorSlowDecay", params.phosphorSlowDecay);
    setFloat("phosphorSlowWeight", params.phosphorSlowWeight);
    setFloat("phosphorSpread", params.phosphorSpread);
    setFloat("observerIntegration", params.observerIntegration);
    setFloat("bloomThreshold", params.bloomThreshold);
    setFloat("bloomIntensity", params.bloomIntensity);
    setFloat("wideBloomIntensity", params.wideBloomIntensity);
    setFloat("bloomRadius", params.bloomRadius);
    setFloat("halation", params.halation);
    setFloat("curvatureX", params.curvatureX);
    setFloat("curvatureY", params.curvatureY);
    setFloat("pincushion", params.pincushion);
    setFloat("highVoltageBloom", params.highVoltageBloom);
    setFloat("overscan", params.overscan);
    setFloat("cornerRadius", params.cornerRadius);
    setFloat("vignette", params.vignette);
    setFloat("glassTransmission", params.glassTransmission);
    setVec3("glassTint", params.glassTint);
    setFloat("reflection", params.reflection);
    setFloat("blackLevel", params.blackLevel);
    setFloat("brightness", params.brightness);
    setFloat("saturation", params.saturation);
    setFloat("flicker", params.flicker);
    setFloat("noise", params.noise);
}

void Shader_m::bindTemporalTextures(Shader shader, const std::string& name) {
    if (name != "crt") return;

    Texture2D previousFrame = prevSceneRT_.texture;
    if (prevSceneRT_.id) {
        int loc = GetShaderLocation(shader, "prevTexture");
        if (loc >= 0) SetShaderValueTexture(shader, loc, previousFrame);
    }

}

Texture2D Shader_m::updatePhosphorState(Texture2D source) {
    if (!has("phosphor_state") || !has("crt_phosphor_combine") ||
        !phosphorRT_[0].id || !phosphorRT_[1].id ||
        !phosphorSlowRT_[0].id || !phosphorSlowRT_[1].id) {
        return source;
    }

    Shader shader = get("phosphor_state");
    auto updateLayer = [&](RenderTexture2D states[2], Vector3 decay) {
        RenderTexture2D& prev = states[phosphorIndex_];
        RenderTexture2D& dst = states[phosphorIndex_ ^ 1];

        BeginTextureMode(dst);
            ClearBackground(BLACK);
            BeginShaderMode(shader);
                uploadPassUniforms(shader, "phosphor_state", source);
                int loc = GetShaderLocation(shader, "prevTexture");
                if (loc >= 0) SetShaderValueTexture(shader, loc, prev.texture);
                loc = GetShaderLocation(shader, "stateDecay");
                if (loc >= 0) {
                    float value[3] = {decay.x, decay.y, decay.z};
                    SetShaderValue(shader, loc, value, SHADER_UNIFORM_VEC3);
                }
                DrawTextureRec(source, {0,0,(float)source.width, -(float)source.height}, {0,0}, WHITE);
            EndShaderMode();
        EndTextureMode();
    };

    const CRTParams& params = crtParams();
    updateLayer(phosphorRT_, params.phosphorFastDecay);
    updateLayer(phosphorSlowRT_, params.phosphorSlowDecay);

    phosphorIndex_ ^= 1;
    return runFullscreenPass(
        "crt_phosphor_combine",
        phosphorRT_[phosphorIndex_].texture,
        ping_[0],
        "slowTexture",
        phosphorSlowRT_[phosphorIndex_].texture
    );
}

Texture2D Shader_m::updateObserverState(Texture2D source) {
    if (!has("crt_observer_response") ||
        !observerRT_[0].id || !observerRT_[1].id) {
        return source;
    }

    RenderTexture2D& previous = observerRT_[observerIndex_];
    RenderTexture2D& target = observerRT_[observerIndex_ ^ 1];
    runFullscreenPass(
        "crt_observer_response",
        source,
        target,
        "prevTexture",
        previous.texture
    );
    observerIndex_ ^= 1;
    return observerRT_[observerIndex_].texture;
}

Texture2D Shader_m::runFullscreenPass(
    const std::string& name,
    Texture2D source,
    RenderTexture2D& target,
    const char* extraUniform,
    Texture2D extraTexture,
    const char* secondExtraUniform,
    Texture2D secondExtraTexture
) {
    if (!has(name) || !source.id || !target.id) return source;

    Shader shader = get(name);
    BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginShaderMode(shader);
            uploadPassUniforms(shader, name, source);
            if (extraUniform && extraTexture.id) {
                int loc = GetShaderLocation(shader, extraUniform);
                if (loc >= 0) {
                    SetShaderValueTexture(shader, loc, extraTexture);
                }
            }
            if (secondExtraUniform && secondExtraTexture.id) {
                int loc = GetShaderLocation(shader, secondExtraUniform);
                if (loc >= 0) {
                    SetShaderValueTexture(shader, loc, secondExtraTexture);
                }
            }
            DrawTexturePro(
                source,
                {0, 0, (float)source.width, -(float)source.height},
                {0, 0, (float)target.texture.width, (float)target.texture.height},
                {0, 0},
                0.0f,
                WHITE
            );
        EndShaderMode();
    EndTextureMode();
    return target.texture;
}

Texture2D Shader_m::applyCRTPipeline(Texture2D source) {
    const char* required[] = {
        "crt_ntsc_encode",
        "crt_ntsc_decode",
        "crt_scan_vertical",
        "crt_emission",
        "phosphor_state",
        "crt_phosphor_combine",
        "crt_observer_response",
        "crt_brightpass",
        "crt_bloom_downsample",
        "crt_bloom_h",
        "crt_bloom_v",
        "crt"
    };
    for (const char* name : required) {
        if (!has(name)) {
            TraceLog(LOG_WARNING, "CRT: required stage '%s' is unavailable", name);
            return source;
        }
    }

    Texture2D composite = runFullscreenPass(
        "crt_ntsc_encode",
        source,
        ping_[0],
        "prevTexture",
        prevSceneRT_.texture
    );
    Texture2D signal = runFullscreenPass(
        "crt_ntsc_decode",
        composite,
        ping_[1]
    );
    Texture2D vertical = runFullscreenPass(
        "crt_scan_vertical",
        signal,
        ping_[0]
    );
    Texture2D emission = runFullscreenPass(
        "crt_emission",
        vertical,
        ping_[1]
    );
    Texture2D phosphor = updatePhosphorState(emission);
    Texture2D observed = updateObserverState(phosphor);
    Texture2D bright = runFullscreenPass(
        "crt_brightpass",
        observed,
        bloomRT_[0]
    );
    Texture2D bloomHorizontal = runFullscreenPass(
        "crt_bloom_h",
        bright,
        bloomRT_[1]
    );
    Texture2D bloom = runFullscreenPass(
        "crt_bloom_v",
        bloomHorizontal,
        bloomRT_[0]
    );
    Texture2D wideSeed = runFullscreenPass(
        "crt_bloom_downsample",
        bloom,
        bloomWideRT_[0]
    );
    Texture2D wideHorizontal = runFullscreenPass(
        "crt_bloom_h",
        wideSeed,
        bloomWideRT_[1]
    );
    Texture2D wideBloom = runFullscreenPass(
        "crt_bloom_v",
        wideHorizontal,
        bloomWideRT_[0]
    );
    return runFullscreenPass(
        "crt",
        observed,
        ping_[1],
        "bloomTexture",
        bloom,
        "wideBloomTexture",
        wideBloom
    );
}

static void drawFullscreenTexture(Texture2D tex) {
    DrawTextureRec(tex, {0,0,(float)tex.width, -(float)tex.height}, {0,0}, WHITE);
}

static Rectangle getLetterboxRect(int srcW, int srcH, int targetW, int targetH) {
    if (srcW <= 0 || srcH <= 0 || targetW <= 0 || targetH <= 0) return {0,0,0,0};

    int scaleX = targetW / srcW;
    int scaleY = targetH / srcH;
    int scale = std::max(1, std::min(scaleX, scaleY));

    int dw = srcW * scale;
    int dh = srcH * scale;
    int dx = (targetW - dw) / 2;
    int dy = (targetH - dh) / 2;

    return {(float)dx, (float)dy, (float)dw, (float)dh};
}

static Rectangle getCRTDisplayRect(int targetW, int targetH) {
    if (targetW <= 0 || targetH <= 0) return {0, 0, 0, 0};

    constexpr float tubeAspect = 4.0f / 3.0f;
    float width = static_cast<float>(targetW);
    float height = width / tubeAspect;
    if (height > static_cast<float>(targetH)) {
        height = static_cast<float>(targetH);
        width = height * tubeAspect;
    }
    return {
        (static_cast<float>(targetW) - width) * 0.5f,
        (static_cast<float>(targetH) - height) * 0.5f,
        width,
        height
    };
}

static void drawTextureLetterboxed(Texture2D tex, int targetW, int targetH) {
    Rectangle src{0, 0, (float)tex.width, -(float)tex.height}; // flip vertically
    Rectangle dst = getLetterboxRect(tex.width, tex.height, targetW, targetH);
    DrawTexturePro(tex, src, dst, {0,0}, 0.0f, WHITE);
}

static void setFloatUniform(Shader shader, const char* name, float value) {
    int loc = GetShaderLocation(shader, name);
    if (loc >= 0) SetShaderValue(shader, loc, &value, SHADER_UNIFORM_FLOAT);
}

static void setColorUniform(Shader shader, const char* name, Color color) {
    float value[3] = {
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f
    };
    int loc = GetShaderLocation(shader, name);
    if (loc >= 0) SetShaderValue(shader, loc, value, SHADER_UNIFORM_VEC3);
}

static void uploadWaterParams(Shader shader) {
    Shader_m::WaterParams& params = Shader_m::waterParams();
    setFloatUniform(shader, "stillReflectionShiftAmplitude", params.stillReflectionShiftAmplitude);
    setFloatUniform(shader, "stillRippleSlowScale", params.stillRippleSlowScale);
    setFloatUniform(shader, "stillRippleFastScale", params.stillRippleFastScale);
    setFloatUniform(shader, "stillRippleSlowSpeed", params.stillRippleSlowSpeed);
    setFloatUniform(shader, "stillRippleFastSpeed", params.stillRippleFastSpeed);
    setFloatUniform(shader, "stillRippleSlowWeight", params.stillRippleSlowWeight);
    setFloatUniform(shader, "stillRippleFastWeight", params.stillRippleFastWeight);
    setFloatUniform(shader, "stillReflectionLineOffset", params.stillReflectionLineOffset);

    setFloatUniform(shader, "waterfallShiftAmplitude", params.waterfallShiftAmplitude);
    setFloatUniform(shader, "waterfallSegmentHeight", params.waterfallSegmentHeight);
    setFloatUniform(shader, "waterfallFlowSpeed", params.waterfallFlowSpeed);
    setFloatUniform(shader, "waterfallLineSpacing", params.waterfallLineSpacing);
    setFloatUniform(shader, "waterfallLineWidth", params.waterfallLineWidth);
    setFloatUniform(shader, "waterfallLineLength", params.waterfallLineLength);
    setFloatUniform(shader, "waterfallLinePeriod", params.waterfallLinePeriod);
    setFloatUniform(shader, "waterfallLineIntensity", params.waterfallLineIntensity);
    setFloatUniform(shader, "waterfallLineRandomSeed", params.waterfallLineRandomSeed);
}

static void uploadFogParams(Shader shader) {
    Shader_m::FogParams& params = Shader_m::fogParams();
    setColorUniform(shader, "fogColor", params.color);
    setFloatUniform(shader, "fogOpacity", params.opacity);
    setFloatUniform(shader, "fogScale", params.scale);
    setFloatUniform(shader, "fogSpeedX", params.speedX);
    setFloatUniform(shader, "fogSpeedY", params.speedY);
    setFloatUniform(shader, "fogContrast", params.contrast);
    setFloatUniform(shader, "fogSoftness", params.softness);
}

static Rectangle nativeRectToTargetScaled(Rectangle r, Texture2D targetTex) {
    Rectangle dst = getLetterboxRect(NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT, targetTex.width, targetTex.height);
    float scaleX = dst.width / (float)NATIVE_RES_WIDTH;
    float scaleY = dst.height / (float)NATIVE_RES_HEIGHT;
    return {
        dst.x + r.x * scaleX,
        dst.y + r.y * scaleY,
        r.width * scaleX,
        r.height * scaleY
    };
}

static bool clampToBounds(Rectangle &r, int W, int H) {
    float x2 = r.x + r.width;
    float y2 = r.y + r.height;
    if (r.x < 0) r.x = 0;
    if (r.y < 0) r.y = 0;
    if (x2 > W) x2 = (float)W;
    if (y2 > H) y2 = (float)H;
    r.width = x2 - r.x;
    r.height = y2 - r.y;
    return r.width > 0 && r.height > 0;
}

Texture2D Shader_m::applyPasses(Texture2D base, const std::vector<Pass>& passes, RenderTexture2D targets[2], int& targetIndex) {
    Texture2D current = base;
    for (const Pass& pass : passes) {
        RenderTexture2D &dst = targets[targetIndex ^ 1];
        BeginTextureMode(dst);
            switch(pass.type) {
                case Pass::Fullscreen: {
                    // Activate shader for the whole current render target.
                    if (has(pass.shader)) {
                        Shader sh = get(pass.shader);
                        BeginShaderMode(sh);
                        uploadPassUniforms(sh, pass.shader, current);
                        bindTemporalTextures(sh, pass.shader);
                    }
                    // Straight full-screen quad through the shader
                    DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                    if (has(pass.shader)) EndShaderMode();
                } break;
                case Pass::ScreenArea: {
                    // 1) Draw base image (unmodified) so regions outside the area remain untouched
                    DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                    // 2) Activate shader only for target area and redraw that portion
                    if (has(pass.shader)) {
                        Shader sh = get(pass.shader);
                        BeginShaderMode(sh);
                        uploadPassUniforms(sh, pass.shader, current);
                        bindTemporalTextures(sh, pass.shader);
                        Rectangle r = nativeRectToTargetScaled(pass.rect, current);
                        if (r.width > 0 && r.height > 0 && clampToBounds(r, current.width, current.height)) {
                            BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
                                DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                            EndScissorMode();
                        }
                        EndShaderMode();
                    }
                } break;
                case Pass::WorldArea: {
                    // Base image first (unshaded)
                    DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                    if (has(pass.shader)) {
                        // Convert world rect to screen-space before scissor
                        Camera2D cam = Raycam_m::getCam();
                        Vector2 tl = GetWorldToScreen2D({pass.rect.x, pass.rect.y}, cam);
                        Vector2 br = GetWorldToScreen2D({pass.rect.x+pass.rect.width, pass.rect.y+pass.rect.height}, cam);
                        if (br.x < tl.x) std::swap(br.x, tl.x);
                        if (br.y < tl.y) std::swap(br.y, tl.y);
                        Rectangle sr = nativeRectToTargetScaled({tl.x, tl.y, br.x - tl.x, br.y - tl.y}, current);
                        if (sr.width > 0 && sr.height > 0 && clampToBounds(sr, current.width, current.height)) {
                            Shader sh = get(pass.shader);
                            BeginShaderMode(sh);
                            uploadPassUniforms(sh, pass.shader, current);
                            bindTemporalTextures(sh, pass.shader);
                            BeginScissorMode((int)sr.x, (int)sr.y, (int)sr.width, (int)sr.height);
                                DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                            EndScissorMode();
                            EndShaderMode();
                        }
                    }
                } break;
                case Pass::WaterArea: {
                    DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                    if (has(pass.shader)) {
                        Camera2D cam = Raycam_m::getCam();
                        Vector2 tl = GetWorldToScreen2D({pass.rect.x, pass.rect.y}, cam);
                        Vector2 br = GetWorldToScreen2D({pass.rect.x+pass.rect.width, pass.rect.y+pass.rect.height}, cam);
                        if (br.x < tl.x) std::swap(br.x, tl.x);
                        if (br.y < tl.y) std::swap(br.y, tl.y);
                        Rectangle sr = nativeRectToTargetScaled({tl.x, tl.y, br.x - tl.x, br.y - tl.y}, current);
                        if (sr.width > 0 && sr.height > 0 && clampToBounds(sr, current.width, current.height)) {
                            Shader sh = get(pass.shader);
                            BeginShaderMode(sh);
                            uploadPassUniforms(sh, pass.shader, current);
                            bindTemporalTextures(sh, pass.shader);

                            float waterRect[4] = { pass.rect.x, pass.rect.y, pass.rect.width, pass.rect.height };
                            float cameraTarget[2] = { cam.target.x, cam.target.y };
                            float cameraOffset[2] = { cam.offset.x, cam.offset.y };
                            float cameraZoom = cam.zoom;
                            float waterKind = static_cast<float>(pass.mode);
                            int loc = GetShaderLocation(sh, "waterRect");
                            if (loc >= 0) SetShaderValue(sh, loc, waterRect, SHADER_UNIFORM_VEC4);
                            loc = GetShaderLocation(sh, "cameraTarget");
                            if (loc >= 0) SetShaderValue(sh, loc, cameraTarget, SHADER_UNIFORM_VEC2);
                            loc = GetShaderLocation(sh, "cameraOffset");
                            if (loc >= 0) SetShaderValue(sh, loc, cameraOffset, SHADER_UNIFORM_VEC2);
                            loc = GetShaderLocation(sh, "cameraZoom");
                            if (loc >= 0) SetShaderValue(sh, loc, &cameraZoom, SHADER_UNIFORM_FLOAT);
                            loc = GetShaderLocation(sh, "waterKind");
                            if (loc >= 0) SetShaderValue(sh, loc, &waterKind, SHADER_UNIFORM_FLOAT);
                            uploadWaterParams(sh);

                            DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                            EndShaderMode();
                        }
                    }
                } break;
                case Pass::FogArea: {
                    DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                    if (has(pass.shader)) {
                        Camera2D cam = Raycam_m::getCam();
                        Vector2 tl = GetWorldToScreen2D({pass.rect.x, pass.rect.y}, cam);
                        Vector2 br = GetWorldToScreen2D({pass.rect.x+pass.rect.width, pass.rect.y+pass.rect.height}, cam);
                        if (br.x < tl.x) std::swap(br.x, tl.x);
                        if (br.y < tl.y) std::swap(br.y, tl.y);
                        Rectangle sr = nativeRectToTargetScaled({tl.x, tl.y, br.x - tl.x, br.y - tl.y}, current);
                        if (sr.width > 0 && sr.height > 0 && clampToBounds(sr, current.width, current.height)) {
                            Shader sh = get(pass.shader);
                            BeginShaderMode(sh);
                            uploadPassUniforms(sh, pass.shader, current);
                            bindTemporalTextures(sh, pass.shader);

                            float fogRect[4] = { pass.rect.x, pass.rect.y, pass.rect.width, pass.rect.height };
                            float cameraTarget[2] = { cam.target.x, cam.target.y };
                            float cameraOffset[2] = { cam.offset.x, cam.offset.y };
                            float cameraZoom = cam.zoom;
                            int loc = GetShaderLocation(sh, "fogRect");
                            if (loc >= 0) SetShaderValue(sh, loc, fogRect, SHADER_UNIFORM_VEC4);
                            loc = GetShaderLocation(sh, "cameraTarget");
                            if (loc >= 0) SetShaderValue(sh, loc, cameraTarget, SHADER_UNIFORM_VEC2);
                            loc = GetShaderLocation(sh, "cameraOffset");
                            if (loc >= 0) SetShaderValue(sh, loc, cameraOffset, SHADER_UNIFORM_VEC2);
                            loc = GetShaderLocation(sh, "cameraZoom");
                            if (loc >= 0) SetShaderValue(sh, loc, &cameraZoom, SHADER_UNIFORM_FLOAT);
                            uploadFogParams(sh);

                            BeginScissorMode((int)sr.x, (int)sr.y, (int)sr.width, (int)sr.height);
                                DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                            EndScissorMode();
                            EndShaderMode();
                        }
                    }
                } break;
                default: {
                    // Unknown pass type: safe copy
                    DrawTextureRec(current, {0,0,(float)current.width, -(float)current.height}, {0,0}, WHITE);
                } break;
            }
        EndTextureMode();
        current = dst.texture;
        targetIndex ^= 1;
    }
    return current;
}

void Shader_m::present() {
    ensureTargets();

    std::vector<Pass> nativeRegularPasses;
    std::vector<Pass> nativeWaterPasses;
    std::vector<Pass> nativePasses;
    std::vector<Pass> displayPasses;
    nativeRegularPasses.reserve(queue_.size());
    nativeWaterPasses.reserve(queue_.size());
    nativePasses.reserve(queue_.size());
    displayPasses.reserve(queue_.size());
    for (const Pass& pass : queue_) {
        if (pass.shader == "crt") {
            displayPasses.push_back(pass);
        } else if (pass.type == Pass::WaterArea) {
            // Water refraction must run after every other native pass so it can
            // reflect the fully post-processed scene right before CRT.
            nativeWaterPasses.push_back(pass);
        } else {
            nativeRegularPasses.push_back(pass);
        }
    }
    nativePasses.insert(nativePasses.end(), nativeRegularPasses.begin(), nativeRegularPasses.end());
    nativePasses.insert(nativePasses.end(), nativeWaterPasses.begin(), nativeWaterPasses.end());

    Texture2D nativeTex = nativePasses.empty()
        ? sceneRT_.texture
        : applyPasses(sceneRT_.texture, nativePasses, nativePing_, nativePingIndex_);

    BeginTextureMode(postRT_);
        ClearBackground(BLACK);
        if (displayPasses.empty()) {
            drawTextureLetterboxed(nativeTex, postRT_.texture.width, postRT_.texture.height);
        } else {
            Rectangle sourceRect{0, 0, (float)nativeTex.width, -(float)nativeTex.height};
            Rectangle tubeRect = getCRTDisplayRect(postRT_.texture.width, postRT_.texture.height);
            DrawTexturePro(nativeTex, sourceRect, tubeRect, {0, 0}, 0.0f, WHITE);
        }
    EndTextureMode();

    Texture2D outTex = displayPasses.empty()
        ? postRT_.texture
        : applyCRTPipeline(postRT_.texture);
    // Post-process output is already in final window resolution.
    drawFullscreenTexture(outTex);
    // Keep the previous raw video frame, never the already simulated tube.
    if (prevSceneRT_.id) {
        BeginTextureMode(prevSceneRT_);
            drawFullscreenTexture(postRT_.texture);
        EndTextureMode();
        videoHistoryValid_ = true;
    }
    queue_.clear();
}

void Shader_m::routine() {
    ensureTargets();
    if (detectChanges()) reload();
}

void Shader_m::snapshot() {
    fileTimes_.clear();
    std::error_code ec;
    if (!std::filesystem::exists(dir_, ec)) return;
    for (auto &p : std::filesystem::directory_iterator(dir_, ec)) {
        if (!p.is_regular_file()) continue;
        std::string ext = p.path().extension().string();
        for (auto &c: ext) c=(char)tolower(c);
        if (ext==".fs" || ext==".vs") fileTimes_[p.path().string()] = std::filesystem::last_write_time(p.path(), ec);
    }
}

bool Shader_m::detectChanges() {
    std::error_code ec;
    if (!std::filesystem::exists(dir_, ec)) return false;
    bool changed = false;
    std::unordered_map<std::string, std::filesystem::file_time_type> cur;
    for (auto &p : std::filesystem::directory_iterator(dir_, ec)) {
        if (!p.is_regular_file()) continue;
        std::string ext = p.path().extension().string();
        for (auto &c: ext) c=(char)tolower(c);
        if (ext==".fs" || ext==".vs") cur[p.path().string()] = std::filesystem::last_write_time(p.path(), ec);
    }
    if (cur.size() != fileTimes_.size()) changed = true;
    else {
        for (auto &kv : cur) {
            auto it = fileTimes_.find(kv.first);
            if (it==fileTimes_.end() || it->second != kv.second) { changed = true; break; }
        }
    }
    if (changed) fileTimes_ = std::move(cur);
    return changed;
}
