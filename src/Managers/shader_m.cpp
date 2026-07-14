#include "shader_m.h"
#include "raycam_m.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <rlgl.h>
#include <sstream>
#include "definitions.h"
#include "light_m.h"

using json = nlohmann::json;

std::unordered_map<std::string, Shader> Shader_m::shaders_;
std::filesystem::path Shader_m::dir_;
RenderTexture2D Shader_m::sceneRT_{};
RenderTexture2D Shader_m::postRT_{};
RenderTexture2D Shader_m::heldVideoRT_{};
RenderTexture2D Shader_m::pendingVideoRT_{};
RenderTexture2D Shader_m::signalRT_[2] = {};
RenderTexture2D Shader_m::sourceYiqRT_{};
RenderTexture2D Shader_m::burstRT_{};
RenderTexture2D Shader_m::receiverBurstRT_[2] = {};
RenderTexture2D Shader_m::receiverVideoRT_[2] = {};
RenderTexture2D Shader_m::receiverVerticalRT_[2] = {};
RenderTexture2D Shader_m::phosphorRT_[2] = {};
RenderTexture2D Shader_m::phosphorMediumRT_[2] = {};
RenderTexture2D Shader_m::phosphorSlowRT_[2] = {};
RenderTexture2D Shader_m::previousEmissionRT_{};
RenderTexture2D Shader_m::currentEmissionRT_{};
RenderTexture2D Shader_m::observerRT_[2] = {};
RenderTexture2D Shader_m::aplRT_[2] = {};
RenderTexture2D Shader_m::previousDriveRT_{};
RenderTexture2D Shader_m::currentDriveRT_{};
RenderTexture2D Shader_m::maskThermalRT_[2] = {};
RenderTexture2D Shader_m::nativePing_[2] = {};
RenderTexture2D Shader_m::ping_[2] = {};
RenderTexture2D Shader_m::bloomRT_[2] = {};
RenderTexture2D Shader_m::bloomWideRT_[2] = {};
int Shader_m::nativePingIndex_ = 0;
int Shader_m::pingIndex_ = 0;
int Shader_m::phosphorIndex_ = 0;
int Shader_m::observerIndex_ = 0;
int Shader_m::aplIndex_ = 0;
int Shader_m::maskThermalIndex_ = 0;
int Shader_m::receiverBurstIndex_ = 0;
int Shader_m::receiverVideoIndex_ = 0;
int Shader_m::receiverVerticalIndex_ = 0;
long long Shader_m::heldVideoFrame_ = -1;
bool Shader_m::pendingVideoValid_ = false;
bool Shader_m::emissionHistoryValid_ = false;
bool Shader_m::driveHistoryValid_ = false;
int Shader_m::lastW_ = 0;
int Shader_m::lastH_ = 0;
int Shader_m::lastScreenW_ = 0;
int Shader_m::lastScreenH_ = 0;
std::vector<Shader_m::Pass> Shader_m::queue_;
std::vector<std::filesystem::path> Shader_m::screenshotQueue_;
std::filesystem::path Shader_m::lastScreenshotPath_;
bool Shader_m::lastScreenshotSucceeded_ = false;
std::unordered_map<std::string, std::filesystem::file_time_type> Shader_m::fileTimes_;

namespace {
constexpr const char* CRT_PARAMS_PATH = "crt_params.json";
constexpr const char* WATER_PARAMS_PATH = "water_params.json";
constexpr const char* FOG_PARAMS_PATH = "fog_params.json";
bool crtParamsLoaded = false;
bool waterParamsLoaded = false;
bool fogParamsLoaded = false;
bool crtPipelineClockActive = false;
float crtPipelineTime = 0.0f;
constexpr int CRT_TOTAL_LINES = 262;
constexpr int CRT_ACTIVE_LINES = 240;
constexpr int CRT_ACTIVE_START_LINE = 20;
constexpr int CRT_CONTENT_LINES = NATIVE_RES_HEIGHT;
constexpr int CRT_CONTENT_START_LINE = 44;
constexpr double CRT_LINE_RATE_HZ = 15734.264;
constexpr double CRT_FRAME_RATE_HZ = CRT_LINE_RATE_HZ / CRT_TOTAL_LINES;
constexpr int CRT_SIGNAL_SAMPLES = 910;
constexpr int CRT_RASTER_WIDTH = 3840;
constexpr int CRT_RASTER_HEIGHT = 2160;
constexpr int CRT_NATIVE_PRESENTATION_SCALE = 11;
constexpr int CRT_TUBE_WIDTH = NATIVE_RES_WIDTH * CRT_NATIVE_PRESENTATION_SCALE;
constexpr int CRT_TUBE_HEIGHT = NATIVE_RES_HEIGHT * CRT_NATIVE_PRESENTATION_SCALE;
constexpr int CRT_TUBE_X = (CRT_RASTER_WIDTH - CRT_TUBE_WIDTH) / 2;
constexpr int CRT_TUBE_Y = (CRT_RASTER_HEIGHT - CRT_TUBE_HEIGHT) / 2;
static_assert(CRT_CONTENT_START_LINE >= CRT_ACTIVE_START_LINE);
static_assert(CRT_CONTENT_START_LINE + CRT_CONTENT_LINES <=
    CRT_ACTIVE_START_LINE + CRT_ACTIVE_LINES);
static_assert(CRT_TUBE_WIDTH == 3520 && CRT_TUBE_HEIGHT == 2112);
static_assert(CRT_TUBE_X == 160 && CRT_TUBE_Y == 24);
float crtPipelineFrameTime = static_cast<float>(1.0 / CRT_FRAME_RATE_HZ);
float crtFramePhase = 0.0f;
bool crtFrameAdvanced = false;
int crtFramesAdvanced = 0;
double crtLastPipelineTime = -1.0;
bool sceneTimeFrozen = false;
float frozenSceneTime = 0.0f;
bool crtLastFrameApplied = false;

const char* crtTestPatternName(int pattern) {
    switch (pattern) {
        case 0: return "game";
        case 1: return "bars_pluge";
        case 2: return "convergence";
        case 3: return "multiburst";
        case 4: return "half_screen";
        case 5: return "zone_plate";
        case 6: return "grayscale_ramp";
        default: return "unknown";
    }
}

std::string sanitizeScreenshotLabel(std::string label) {
    for (char& character : label) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '-' && character != '_') {
            character = '_';
        }
    }
    if (label.empty()) label = "game";
    return label;
}

std::filesystem::path timestampedScreenshotPath(const std::string& label) {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream name;
    name << std::put_time(&local, "%Y%m%d-%H%M%S") << '-'
         << std::setfill('0') << std::setw(3) << milliseconds.count() << '-'
         << sanitizeScreenshotLabel(label) << ".png";
    return std::filesystem::path("screenshots") / name.str();
}

float halfToFloat(std::uint16_t half) {
    const std::uint32_t sign =
        static_cast<std::uint32_t>(half & 0x8000u) << 16u;
    std::uint32_t exponent = (half >> 10u) & 0x1fu;
    std::uint32_t mantissa = half & 0x03ffu;
    std::uint32_t bits = 0;

    if (exponent == 0u) {
        if (mantissa == 0u) {
            bits = sign;
        } else {
            // Normalize a half-float subnormal before rebiasing it to float32.
            std::uint32_t shift = 0u;
            while ((mantissa & 0x0400u) == 0u) {
                mantissa <<= 1u;
                ++shift;
            }
            mantissa &= 0x03ffu;
            const std::uint32_t floatExponent = 113u - shift;
            bits = sign | (floatExponent << 23u) | (mantissa << 13u);
        }
    } else if (exponent == 0x1fu) {
        bits = sign | 0x7f800000u | (mantissa << 13u);
    } else {
        exponent += 112u;
        bits = sign | (exponent << 23u) | (mantissa << 13u);
    }
    return std::bit_cast<float>(bits);
}

json sampleStateTexture(Texture2D texture) {
    if (!texture.id || texture.width != 1 || texture.height != 1) return nullptr;
    void* pixels = rlReadTexturePixels(texture.id, texture.width,
        texture.height, texture.format);
    if (!pixels) return nullptr;

    json result = nullptr;
    if (texture.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16A16) {
        const auto* half = static_cast<const std::uint16_t*>(pixels);
        result = {halfToFloat(half[0]), halfToFloat(half[1]),
            halfToFloat(half[2]), halfToFloat(half[3])};
    } else if (texture.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32) {
        const auto* value = static_cast<const float*>(pixels);
        result = {value[0], value[1], value[2], value[3]};
    } else if (texture.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        const auto* value = static_cast<const std::uint8_t*>(pixels);
        result = {value[0] / 255.0f, value[1] / 255.0f,
            value[2] / 255.0f, value[3] / 255.0f};
    }
    RL_FREE(pixels);
    return result;
}

float readFloat(const json& j, const char* key, float fallback) {
    if (!j.contains(key) || !j[key].is_number()) return fallback;
    return j[key].get<float>();
}

std::string readString(const json& j, const char* key, std::string fallback) {
    if (!j.contains(key) || !j[key].is_string()) return fallback;
    return j[key].get<std::string>();
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
            {"presentationLineCombStrength", params.ntscLineCombStrength},
            {"tube", "Philips A34JLN60X"},
            {"tubeSizeInches", 13.0},
            {"mask", "slot"},
            {"deflectionDegrees", 90.0},
            // The service supplement gives a safety procedure, not a nominal
            // operating EHT for the measured unit. Do not turn an estimate
            // into apparently calibrated metadata.
            {"anodeVoltageKv", params.anodeVoltageKv > 0.0f
                ? json(params.anodeVoltageKv) : json(nullptr)},
            {"anodeVoltageStatus", params.anodeVoltageStatus},
            {"horizontalRateHz", 15734.264},
            {"videoMode", "native 240p progressive"},
            {"sourceTimingProfile", "rayEngine-320x192-262p"},
            {"sourceTimingScope", "fixed engine source; 240p is not one universal timing standard"},
            {"totalLines", CRT_TOTAL_LINES},
            {"activeStartLine", CRT_ACTIVE_START_LINE},
            {"activeLines", CRT_ACTIVE_LINES},
            {"contentStartLine", CRT_CONTENT_START_LINE},
            {"contentLines", CRT_CONTENT_LINES},
            {"contentPixelsPerLine", NATIVE_RES_WIDTH},
            {"sourceFramebufferLines", NATIVE_RES_HEIGHT},
            {"presentationAspect", static_cast<double>(NATIVE_RES_WIDTH) /
                NATIVE_RES_HEIGHT},
            {"overscan", false},
            {"sourcePixelClockMHz", NATIVE_RES_WIDTH / 52.655},
            {"signalSamplesPerLine", CRT_SIGNAL_SAMPLES},
            {"rasterWidth", CRT_RASTER_WIDTH},
            {"rasterHeight", CRT_RASTER_HEIGHT},
            {"tubeRect", {
                {"x", CRT_TUBE_X},
                {"y", CRT_TUBE_Y},
                {"width", CRT_TUBE_WIDTH},
                {"height", CRT_TUBE_HEIGHT}
            }},
            {"nativePresentationScale", CRT_NATIVE_PRESENTATION_SCALE},
            {"rasterFormat", "RGBA16F"},
            {"framebufferLatch", "vertical blank"},
            {"setupIRE", 7.5},
            {"frameRateHz", CRT_FRAME_RATE_HZ},
            {"subcarrierMHz", 3.579545},
            {"calibrationStatus", params.calibrationStatus},
            {"measuredSerial", params.measuredSerial},
            {"note", "electrical standard values are documented; tube-specific coefficients require measurements"}
        }},
        {"outputGamma", params.outputGamma},
        {"tubePeakNits", params.tubePeakNits},
        {"hostPeakNits", params.hostPeakNits},
        {"referenceWhiteRadiance", params.referenceWhiteRadiance},
        {"ntscSourceLumaBandwidthMHz", params.ntscSourceLumaBandwidthMHz},
        {"ntscSourceIBandwidthMHz", params.ntscSourceIBandwidthMHz},
        {"ntscSourceQBandwidthMHz", params.ntscSourceQBandwidthMHz},
        {"ntscLumaBandwidthMHz", params.ntscLumaBandwidthMHz},
        {"ntscChromaBandwidthIMHz", params.ntscChromaBandwidthIMHz},
        {"ntscChromaBandwidthQMHz", params.ntscChromaBandwidthQMHz},
        {"ntscChromaGain", params.ntscChromaGain},
        {"ntscChromaDelayNs", params.ntscChromaDelayNs},
        {"ntscLineCombStrength", params.ntscLineCombStrength},
        {"ntscLumaPeaking", params.ntscLumaPeaking},
        {"ntscDifferentialGain", params.ntscDifferentialGain},
        {"ntscDifferentialPhaseDeg", params.ntscDifferentialPhaseDeg},
        {"ntscNoise", params.ntscNoise},
        {"ntscHum", params.ntscHum},
        {"ntscAgcResponse", params.ntscAgcResponse},
        {"ntscClampResponse", params.ntscClampResponse},
        {"ntscBurstPllBandwidthHz", params.ntscBurstPllBandwidthHz},
        {"ntscHorizontalPllBandwidthHz", params.ntscHorizontalPllBandwidthHz},
        {"ntscVerticalPllBandwidthHz", params.ntscVerticalPllBandwidthHz},
        {"ntscAccResponse", params.ntscAccResponse},
        {"ntscColorKillerThreshold", params.ntscColorKillerThreshold},
        {"videoGain", vector3ToJson(params.videoGain)},
        {"videoCutoff", vector3ToJson(params.videoCutoff)},
        {"gunGamma", vector3ToJson(params.gunGamma)},
        {"beamMinWidth", params.beamMinWidth},
        {"beamMaxWidth", params.beamMaxWidth},
        {"beamShape", params.beamShape},
        {"beamIntensityWeight", params.beamIntensityWeight},
        {"beamScanlineStrength", params.beamScanlineStrength},
        {"beamHorizontalSigma", params.beamHorizontalSigma},
        {"beamCurrentLimit", params.beamCurrentLimit},
        {"beamCurrentCompression", params.beamCurrentCompression},
        {"videoOutputBandwidthMHz", params.videoOutputBandwidthMHz},
        {"cathodeDriveHeadroom", params.cathodeDriveHeadroom},
        {"spaceChargeCompression", params.spaceChargeCompression},
        {"spotBloom", params.spotBloom},
        {"dynamicFocus", params.dynamicFocus},
        {"focusEdgeSoftness", params.focusEdgeSoftness},
        {"astigmatism", params.astigmatism},
        {"misconvergence", params.misconvergence},
        {"horizontalJitter", params.horizontalJitter},
        {"maskStrength", params.maskStrength},
        {"maskTriadsAcross", params.maskTriadsAcross},
        {"maskType", params.maskType},
        {"maskHeating", params.maskHeating},
        {"maskThermalTau", params.maskThermalTau},
        {"maskThermalDiffusion", params.maskThermalDiffusion},
        {"maskDoming", params.maskDoming},
        {"maskCrosstalk", params.maskCrosstalk},
        {"phosphorFastDecay", vector3ToJson(params.phosphorFastDecay)},
        {"phosphorMediumDecay", vector3ToJson(params.phosphorMediumDecay)},
        {"phosphorSlowDecay", vector3ToJson(params.phosphorSlowDecay)},
        {"phosphorMediumWeight", params.phosphorMediumWeight},
        {"phosphorSlowWeight", params.phosphorSlowWeight},
        {"phosphorSpread", params.phosphorSpread},
        {"phosphorSaturation", params.phosphorSaturation},
        {"observerIntegration", params.observerIntegration},
        {"bloomThreshold", params.bloomThreshold},
        {"bloomIntensity", params.bloomIntensity},
        {"wideBloomIntensity", params.wideBloomIntensity},
        {"bloomRadius", params.bloomRadius},
        {"bloomRadiusRGB", vector3ToJson(params.bloomRadiusRGB)},
        {"halation", params.halation},
        {"curvatureX", params.curvatureX},
        {"curvatureY", params.curvatureY},
        {"pincushion", params.pincushion},
        {"highVoltageBloom", params.highVoltageBloom},
        {"highVoltageResponse", params.highVoltageResponse},
        {"highVoltageSag", params.highVoltageSag},
        {"highVoltageRipple", params.highVoltageRipple},
        {"bPlusResponse", params.bPlusResponse},
        {"bPlusSag", params.bPlusSag},
        {"bPlusRipple", params.bPlusRipple},
        {"cornerRadius", params.cornerRadius},
        {"vignette", params.vignette},
        {"glassTransmission", params.glassTransmission},
        {"glassTint", vector3ToJson(params.glassTint)},
        {"glassDispersion", params.glassDispersion},
        {"glassRefractiveIndex", params.glassRefractiveIndex},
        {"glassThicknessMm", params.glassThicknessMm},
        {"glassAbsorption", vector3ToJson(params.glassAbsorption)},
        {"internalReflection", params.internalReflection},
        {"ambientIlluminance", params.ambientIlluminance},
        {"faceplateCurvatureX", params.faceplateCurvatureX},
        {"faceplateCurvatureY", params.faceplateCurvatureY},
        {"tubeColorMatrixR", vector3ToJson(params.tubeColorMatrixR)},
        {"tubeColorMatrixG", vector3ToJson(params.tubeColorMatrixG)},
        {"tubeColorMatrixB", vector3ToJson(params.tubeColorMatrixB)},
        {"reflection", params.reflection},
        {"blackLevel", params.blackLevel},
        {"brightness", params.brightness},
        {"saturation", params.saturation},
        {"flicker", params.flicker},
        {"noise", params.noise},
        {"testPattern", params.testPattern},
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
        if (j.contains("profile") && j["profile"].is_object()) {
            params.calibrationStatus = readString(
                j["profile"], "calibrationStatus", params.calibrationStatus);
            params.measuredSerial = readString(
                j["profile"], "measuredSerial", params.measuredSerial);
            params.anodeVoltageStatus = readString(
                j["profile"], "anodeVoltageStatus",
                params.anodeVoltageStatus);
            params.anodeVoltageKv = readFloat(
                j["profile"], "anodeVoltageKv", params.anodeVoltageKv);
        }
        params.outputGamma = readFloat(j, "outputGamma", params.outputGamma);
        params.tubePeakNits = readFloat(j, "tubePeakNits", params.tubePeakNits);
        params.hostPeakNits = readFloat(j, "hostPeakNits", params.hostPeakNits);
        params.referenceWhiteRadiance = readFloat(
            j, "referenceWhiteRadiance", params.referenceWhiteRadiance);
        params.ntscSourceLumaBandwidthMHz = readFloat(j, "ntscSourceLumaBandwidthMHz", params.ntscSourceLumaBandwidthMHz);
        params.ntscSourceIBandwidthMHz = readFloat(j, "ntscSourceIBandwidthMHz", params.ntscSourceIBandwidthMHz);
        params.ntscSourceQBandwidthMHz = readFloat(j, "ntscSourceQBandwidthMHz", params.ntscSourceQBandwidthMHz);
        params.ntscLumaBandwidthMHz = readFloat(j, "ntscLumaBandwidthMHz", params.ntscLumaBandwidthMHz);
        params.ntscChromaBandwidthIMHz = readFloat(j, "ntscChromaBandwidthIMHz", params.ntscChromaBandwidthIMHz);
        params.ntscChromaBandwidthQMHz = readFloat(j, "ntscChromaBandwidthQMHz", params.ntscChromaBandwidthQMHz);
        params.ntscChromaGain = readFloat(j, "ntscChromaGain", params.ntscChromaGain);
        params.ntscChromaDelayNs = readFloat(j, "ntscChromaDelayNs", params.ntscChromaDelayNs);
        params.ntscLineCombStrength = readFloat(
            j, "ntscLineCombStrength", params.ntscLineCombStrength);
        params.ntscLumaPeaking = readFloat(j, "ntscLumaPeaking", params.ntscLumaPeaking);
        params.ntscDifferentialGain = readFloat(j, "ntscDifferentialGain", params.ntscDifferentialGain);
        params.ntscDifferentialPhaseDeg = readFloat(j, "ntscDifferentialPhaseDeg", params.ntscDifferentialPhaseDeg);
        params.ntscNoise = readFloat(j, "ntscNoise", params.ntscNoise);
        params.ntscHum = readFloat(j, "ntscHum", params.ntscHum);
        params.ntscAgcResponse = readFloat(j, "ntscAgcResponse", params.ntscAgcResponse);
        params.ntscClampResponse = readFloat(j, "ntscClampResponse", params.ntscClampResponse);
        params.ntscBurstPllBandwidthHz = readFloat(j, "ntscBurstPllBandwidthHz", params.ntscBurstPllBandwidthHz);
        params.ntscHorizontalPllBandwidthHz = readFloat(j, "ntscHorizontalPllBandwidthHz", params.ntscHorizontalPllBandwidthHz);
        params.ntscVerticalPllBandwidthHz = readFloat(j, "ntscVerticalPllBandwidthHz", params.ntscVerticalPllBandwidthHz);
        params.ntscAccResponse = readFloat(j, "ntscAccResponse", params.ntscAccResponse);
        params.ntscColorKillerThreshold = readFloat(j, "ntscColorKillerThreshold", params.ntscColorKillerThreshold);
        params.videoGain = readVector3(j, "videoGain", params.videoGain);
        params.videoCutoff = readVector3(j, "videoCutoff", params.videoCutoff);
        params.gunGamma = readVector3(j, "gunGamma", params.gunGamma);
        params.beamMinWidth = readFloat(j, "beamMinWidth", params.beamMinWidth);
        params.beamMaxWidth = readFloat(j, "beamMaxWidth", params.beamMaxWidth);
        params.beamShape = readFloat(j, "beamShape", params.beamShape);
        params.beamIntensityWeight = readFloat(j, "beamIntensityWeight", params.beamIntensityWeight);
        params.beamScanlineStrength = readFloat(j, "beamScanlineStrength", params.beamScanlineStrength);
        params.beamHorizontalSigma = readFloat(j, "beamHorizontalSigma", params.beamHorizontalSigma);
        params.beamCurrentLimit = readFloat(j, "beamCurrentLimit", params.beamCurrentLimit);
        params.beamCurrentCompression = readFloat(j, "beamCurrentCompression", params.beamCurrentCompression);
        params.videoOutputBandwidthMHz = readFloat(j, "videoOutputBandwidthMHz", params.videoOutputBandwidthMHz);
        params.cathodeDriveHeadroom = readFloat(j, "cathodeDriveHeadroom", params.cathodeDriveHeadroom);
        params.spaceChargeCompression = readFloat(j, "spaceChargeCompression", params.spaceChargeCompression);
        params.spotBloom = readFloat(j, "spotBloom", params.spotBloom);
        params.dynamicFocus = readFloat(j, "dynamicFocus", params.dynamicFocus);
        params.focusEdgeSoftness = readFloat(j, "focusEdgeSoftness", params.focusEdgeSoftness);
        params.astigmatism = readFloat(j, "astigmatism", params.astigmatism);
        params.misconvergence = readFloat(j, "misconvergence", params.misconvergence);
        params.horizontalJitter = readFloat(j, "horizontalJitter", params.horizontalJitter);
        params.maskStrength = readFloat(j, "maskStrength", params.maskStrength);
        params.maskTriadsAcross = readFloat(j, "maskTriadsAcross", params.maskTriadsAcross);
        params.maskType = readFloat(j, "maskType", params.maskType);
        params.maskHeating = readFloat(j, "maskHeating", params.maskHeating);
        params.maskThermalTau = readFloat(j, "maskThermalTau", params.maskThermalTau);
        params.maskThermalDiffusion = readFloat(j, "maskThermalDiffusion", params.maskThermalDiffusion);
        params.maskDoming = readFloat(j, "maskDoming", params.maskDoming);
        params.maskCrosstalk = readFloat(j, "maskCrosstalk", params.maskCrosstalk);
        params.phosphorFastDecay = readVector3(j, "phosphorFastDecay", params.phosphorFastDecay);
        params.phosphorMediumDecay = readVector3(j, "phosphorMediumDecay", params.phosphorMediumDecay);
        params.phosphorSlowDecay = readVector3(j, "phosphorSlowDecay", params.phosphorSlowDecay);
        params.phosphorMediumWeight = readFloat(j, "phosphorMediumWeight", params.phosphorMediumWeight);
        params.phosphorSlowWeight = readFloat(j, "phosphorSlowWeight", params.phosphorSlowWeight);
        params.phosphorSpread = readFloat(j, "phosphorSpread", params.phosphorSpread);
        params.phosphorSaturation = readFloat(j, "phosphorSaturation", params.phosphorSaturation);
        params.observerIntegration = readFloat(j, "observerIntegration", params.observerIntegration);
        params.bloomThreshold = readFloat(j, "bloomThreshold", params.bloomThreshold);
        params.bloomIntensity = readFloat(j, "bloomIntensity", params.bloomIntensity);
        params.wideBloomIntensity = readFloat(j, "wideBloomIntensity", params.wideBloomIntensity);
        params.bloomRadius = readFloat(j, "bloomRadius", params.bloomRadius);
        params.bloomRadiusRGB = readVector3(j, "bloomRadiusRGB", params.bloomRadiusRGB);
        params.halation = readFloat(j, "halation", params.halation);
        params.curvatureX = readFloat(j, "curvatureX", params.curvatureX);
        params.curvatureY = readFloat(j, "curvatureY", params.curvatureY);
        params.pincushion = readFloat(j, "pincushion", params.pincushion);
        params.highVoltageBloom = readFloat(j, "highVoltageBloom", params.highVoltageBloom);
        params.highVoltageResponse = readFloat(j, "highVoltageResponse", params.highVoltageResponse);
        params.highVoltageSag = readFloat(j, "highVoltageSag", params.highVoltageSag);
        params.highVoltageRipple = readFloat(j, "highVoltageRipple", params.highVoltageRipple);
        params.bPlusResponse = readFloat(j, "bPlusResponse", params.bPlusResponse);
        params.bPlusSag = readFloat(j, "bPlusSag", params.bPlusSag);
        params.bPlusRipple = readFloat(j, "bPlusRipple", params.bPlusRipple);
        params.cornerRadius = readFloat(j, "cornerRadius", params.cornerRadius);
        params.vignette = readFloat(j, "vignette", params.vignette);
        params.glassTransmission = readFloat(j, "glassTransmission", params.glassTransmission);
        params.glassTint = readVector3(j, "glassTint", params.glassTint);
        params.glassDispersion = readFloat(j, "glassDispersion", params.glassDispersion);
        params.glassRefractiveIndex = readFloat(j, "glassRefractiveIndex", params.glassRefractiveIndex);
        params.glassThicknessMm = readFloat(j, "glassThicknessMm", params.glassThicknessMm);
        params.glassAbsorption = readVector3(j, "glassAbsorption", params.glassAbsorption);
        params.internalReflection = readFloat(j, "internalReflection", params.internalReflection);
        params.ambientIlluminance = readFloat(j, "ambientIlluminance", params.ambientIlluminance);
        params.faceplateCurvatureX = readFloat(j, "faceplateCurvatureX", params.faceplateCurvatureX);
        params.faceplateCurvatureY = readFloat(j, "faceplateCurvatureY", params.faceplateCurvatureY);
        params.tubeColorMatrixR = readVector3(j, "tubeColorMatrixR", params.tubeColorMatrixR);
        params.tubeColorMatrixG = readVector3(j, "tubeColorMatrixG", params.tubeColorMatrixG);
        params.tubeColorMatrixB = readVector3(j, "tubeColorMatrixB", params.tubeColorMatrixB);
        params.reflection = readFloat(j, "reflection", params.reflection);
        params.blackLevel = readFloat(j, "blackLevel", params.blackLevel);
        params.brightness = readFloat(j, "brightness", params.brightness);
        params.saturation = readFloat(j, "saturation", params.saturation);
        params.flicker = readFloat(j, "flicker", params.flicker);
        params.noise = readFloat(j, "noise", params.noise);
        params.testPattern = readFloat(j, "testPattern", params.testPattern);
        return true;
    } catch (...) {
        return false;
    }
}

void Shader_m::requestScreenshot(const std::string& label) {
    requestScreenshotTo(timestampedScreenshotPath(label));
}

void Shader_m::requestScreenshotTo(const std::filesystem::path& requestedPath) {
    std::filesystem::path path = requestedPath;
    if (path.extension() != ".png") path.replace_extension(".png");
    screenshotQueue_.push_back(std::move(path));
}

const std::filesystem::path& Shader_m::lastScreenshotPath() {
    return lastScreenshotPath_;
}

bool Shader_m::lastScreenshotSucceeded() {
    return lastScreenshotSucceeded_;
}

void Shader_m::setSceneTimeFrozen(bool frozen) {
    if (frozen && !sceneTimeFrozen) {
        frozenSceneTime = static_cast<float>(GetTime());
    }
    sceneTimeFrozen = frozen;
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

static void clearRenderTexture(RenderTexture2D target, Color color = BLACK) {
    if (!target.id) return;
    BeginTextureMode(target);
        ClearBackground(color);
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
    // CRT image-bearing stages always run on the fixed UHD reference raster.
    // Window resizing only changes the final presentation copy.
    lastScreenW_ = CRT_RASTER_WIDTH;
    lastScreenH_ = CRT_RASTER_HEIGHT;
    sceneRT_ = loadPointRenderTexture(lastW_, lastH_);
    nativePing_[0] = loadPointRenderTexture(lastW_, lastH_);
    nativePing_[1] = loadPointRenderTexture(lastW_, lastH_);
    postRT_ = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    heldVideoRT_ = loadPointRenderTexture(lastW_, lastH_);
    pendingVideoRT_ = loadPointRenderTexture(lastW_, lastH_);
    signalRT_[0] = loadHDRRenderTexture(CRT_SIGNAL_SAMPLES, CRT_TOTAL_LINES);
    signalRT_[1] = loadHDRRenderTexture(CRT_SIGNAL_SAMPLES, CRT_TOTAL_LINES);
    sourceYiqRT_ = loadHDRRenderTexture(CRT_SIGNAL_SAMPLES, CRT_TOTAL_LINES);
    burstRT_ = loadHDRRenderTexture(1, 1);
    receiverBurstRT_[0] = loadHDRRenderTexture(1, 1);
    receiverBurstRT_[1] = loadHDRRenderTexture(1, 1);
    receiverVideoRT_[0] = loadHDRRenderTexture(1, 1);
    receiverVideoRT_[1] = loadHDRRenderTexture(1, 1);
    receiverVerticalRT_[0] = loadHDRRenderTexture(1, 1);
    receiverVerticalRT_[1] = loadHDRRenderTexture(1, 1);
    phosphorRT_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorRT_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorMediumRT_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorMediumRT_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorSlowRT_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    phosphorSlowRT_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    previousEmissionRT_ = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    currentEmissionRT_ = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    if (crtParams().observerIntegration > 0.0f) {
        observerRT_[0] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
        observerRT_[1] = loadHDRRenderTexture(lastScreenW_, lastScreenH_);
    }
    aplRT_[0] = loadHDRRenderTexture(1, 1);
    aplRT_[1] = loadHDRRenderTexture(1, 1);
    previousDriveRT_ = loadHDRRenderTexture(16, 12);
    currentDriveRT_ = loadHDRRenderTexture(16, 12);
    maskThermalRT_[0] = loadHDRRenderTexture(64, 48);
    maskThermalRT_[1] = loadHDRRenderTexture(64, 48);
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
    aplIndex_ = 0;
    maskThermalIndex_ = 0;
    receiverBurstIndex_ = 0;
    receiverVideoIndex_ = 0;
    receiverVerticalIndex_ = 0;
    heldVideoFrame_ = -1;
    pendingVideoValid_ = false;
    emissionHistoryValid_ = false;
    driveHistoryValid_ = false;
    crtLastPipelineTime = -1.0;
    clearRenderTexture(heldVideoRT_);
    clearRenderTexture(pendingVideoRT_);
    clearRenderTexture(signalRT_[0]);
    clearRenderTexture(signalRT_[1]);
    clearRenderTexture(sourceYiqRT_);
    clearRenderTexture(burstRT_);
    // Encoded neutral receiver state: phase/frequency error 0, AGC gain 1,
    // clamp 0 and ACC gain 1. A black clear would encode zero AGC gain.
    clearRenderTexture(receiverBurstRT_[0], {128, 128, 0, 128});
    clearRenderTexture(receiverBurstRT_[1], {128, 128, 0, 128});
    clearRenderTexture(receiverVideoRT_[0], {77, 128, 0, 128});
    clearRenderTexture(receiverVideoRT_[1], {77, 128, 0, 128});
    clearRenderTexture(receiverVerticalRT_[0], {128, 128, 0, 0});
    clearRenderTexture(receiverVerticalRT_[1], {128, 128, 0, 0});
    clearRenderTexture(phosphorRT_[0]);
    clearRenderTexture(phosphorRT_[1]);
    clearRenderTexture(phosphorMediumRT_[0]);
    clearRenderTexture(phosphorMediumRT_[1]);
    clearRenderTexture(phosphorSlowRT_[0]);
    clearRenderTexture(phosphorSlowRT_[1]);
    clearRenderTexture(previousEmissionRT_);
    clearRenderTexture(currentEmissionRT_);
    if (observerRT_[0].id) clearRenderTexture(observerRT_[0]);
    if (observerRT_[1].id) clearRenderTexture(observerRT_[1]);
    // Power state: load, normalized EHT, normalized B+, heater temperature.
    clearRenderTexture(aplRT_[0], {0, 255, 255, 255});
    clearRenderTexture(aplRT_[1], {0, 255, 255, 255});
    clearRenderTexture(previousDriveRT_);
    clearRenderTexture(currentDriveRT_);
    clearRenderTexture(maskThermalRT_[0]);
    clearRenderTexture(maskThermalRT_[1]);
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
    crtLastPipelineTime = -1.0;
    crtPipelineClockActive = false;
    if (sceneRT_.id) { UnloadRenderTexture(sceneRT_); sceneRT_.id = 0; }
    if (postRT_.id) { UnloadRenderTexture(postRT_); postRT_.id = 0; }
    if (heldVideoRT_.id) { UnloadRenderTexture(heldVideoRT_); heldVideoRT_.id = 0; }
    if (pendingVideoRT_.id) {
        UnloadRenderTexture(pendingVideoRT_);
        pendingVideoRT_.id = 0;
    }
    for (auto &r : signalRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    if (sourceYiqRT_.id) { UnloadRenderTexture(sourceYiqRT_); sourceYiqRT_.id = 0; }
    if (burstRT_.id) { UnloadRenderTexture(burstRT_); burstRT_.id = 0; }
    for (auto &r : receiverBurstRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : receiverVideoRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : receiverVerticalRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : phosphorRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : phosphorMediumRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : phosphorSlowRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    if (previousEmissionRT_.id) {
        UnloadRenderTexture(previousEmissionRT_);
        previousEmissionRT_.id = 0;
    }
    if (currentEmissionRT_.id) {
        UnloadRenderTexture(currentEmissionRT_);
        currentEmissionRT_.id = 0;
    }
    for (auto &r : observerRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : aplRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    if (previousDriveRT_.id) {
        UnloadRenderTexture(previousDriveRT_);
        previousDriveRT_.id = 0;
    }
    if (currentDriveRT_.id) {
        UnloadRenderTexture(currentDriveRT_);
        currentDriveRT_.id = 0;
    }
    for (auto &r : maskThermalRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : nativePing_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : ping_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : bloomRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : bloomWideRT_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &kv : shaders_) if (kv.second.id) UnloadShader(kv.second);
    shaders_.clear();
    queue_.clear();
    screenshotQueue_.clear();
}

void Shader_m::reload() { load(dir_); }

bool Shader_m::has(const std::string& name) { return shaders_.count(name)!=0; }

Shader Shader_m::get(const std::string& name) { auto it = shaders_.find(name); return it==shaders_.end()? Shader{0} : it->second; }

void Shader_m::ensureTargets() {
    int w = NATIVE_RES_WIDTH;
    int h = NATIVE_RES_HEIGHT;
    int sw = CRT_RASTER_WIDTH;
    int sh = CRT_RASTER_HEIGHT;

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
        heldVideoRT_.id && pendingVideoRT_.id &&
        phosphorRT_[0].id && phosphorRT_[1].id &&
        signalRT_[0].id && signalRT_[1].id &&
        sourceYiqRT_.id &&
        burstRT_.id &&
        receiverBurstRT_[0].id && receiverBurstRT_[1].id &&
        receiverVideoRT_[0].id && receiverVideoRT_[1].id &&
        receiverVerticalRT_[0].id && receiverVerticalRT_[1].id &&
        phosphorSlowRT_[0].id && phosphorSlowRT_[1].id &&
        phosphorMediumRT_[0].id && phosphorMediumRT_[1].id &&
        previousEmissionRT_.id && currentEmissionRT_.id &&
        (crtParams().observerIntegration <= 0.0f ||
            (observerRT_[0].id && observerRT_[1].id)) &&
        aplRT_[0].id && aplRT_[1].id &&
        previousDriveRT_.id && currentDriveRT_.id &&
        maskThermalRT_[0].id && maskThermalRT_[1].id &&
        ping_[0].id && ping_[1].id && bloomRT_[0].id && bloomRT_[1].id &&
        bloomWideRT_[0].id && bloomWideRT_[1].id) {
        return;
    }

    if (postRT_.id) UnloadRenderTexture(postRT_);
    if (heldVideoRT_.id) UnloadRenderTexture(heldVideoRT_);
    if (pendingVideoRT_.id) UnloadRenderTexture(pendingVideoRT_);
    for (auto &r: signalRT_) if (r.id) UnloadRenderTexture(r);
    if (sourceYiqRT_.id) UnloadRenderTexture(sourceYiqRT_);
    if (burstRT_.id) UnloadRenderTexture(burstRT_);
    for (auto &r: receiverBurstRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: receiverVideoRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: receiverVerticalRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: phosphorRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: phosphorMediumRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: phosphorSlowRT_) if (r.id) UnloadRenderTexture(r);
    if (previousEmissionRT_.id) UnloadRenderTexture(previousEmissionRT_);
    if (currentEmissionRT_.id) UnloadRenderTexture(currentEmissionRT_);
    for (auto &r: observerRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: aplRT_) if (r.id) UnloadRenderTexture(r);
    if (previousDriveRT_.id) UnloadRenderTexture(previousDriveRT_);
    if (currentDriveRT_.id) UnloadRenderTexture(currentDriveRT_);
    for (auto &r: maskThermalRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: ping_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: bloomRT_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: bloomWideRT_) if (r.id) UnloadRenderTexture(r);
    postRT_ = loadPointRenderTexture(sw,sh);
    heldVideoRT_ = loadPointRenderTexture(w,h);
    pendingVideoRT_ = loadPointRenderTexture(w,h);
    signalRT_[0] = loadHDRRenderTexture(CRT_SIGNAL_SAMPLES, CRT_TOTAL_LINES);
    signalRT_[1] = loadHDRRenderTexture(CRT_SIGNAL_SAMPLES, CRT_TOTAL_LINES);
    sourceYiqRT_ = loadHDRRenderTexture(CRT_SIGNAL_SAMPLES, CRT_TOTAL_LINES);
    burstRT_ = loadHDRRenderTexture(1, 1);
    receiverBurstRT_[0] = loadHDRRenderTexture(1, 1);
    receiverBurstRT_[1] = loadHDRRenderTexture(1, 1);
    receiverVideoRT_[0] = loadHDRRenderTexture(1, 1);
    receiverVideoRT_[1] = loadHDRRenderTexture(1, 1);
    receiverVerticalRT_[0] = loadHDRRenderTexture(1, 1);
    receiverVerticalRT_[1] = loadHDRRenderTexture(1, 1);
    phosphorRT_[0] = loadHDRRenderTexture(sw,sh);
    phosphorRT_[1] = loadHDRRenderTexture(sw,sh);
    phosphorMediumRT_[0] = loadHDRRenderTexture(sw,sh);
    phosphorMediumRT_[1] = loadHDRRenderTexture(sw,sh);
    phosphorSlowRT_[0] = loadHDRRenderTexture(sw,sh);
    phosphorSlowRT_[1] = loadHDRRenderTexture(sw,sh);
    previousEmissionRT_ = loadHDRRenderTexture(sw,sh);
    currentEmissionRT_ = loadHDRRenderTexture(sw,sh);
    if (crtParams().observerIntegration > 0.0f) {
        observerRT_[0] = loadHDRRenderTexture(sw,sh);
        observerRT_[1] = loadHDRRenderTexture(sw,sh);
    } else {
        observerRT_[0] = {};
        observerRT_[1] = {};
    }
    aplRT_[0] = loadHDRRenderTexture(1, 1);
    aplRT_[1] = loadHDRRenderTexture(1, 1);
    previousDriveRT_ = loadHDRRenderTexture(16,12);
    currentDriveRT_ = loadHDRRenderTexture(16,12);
    maskThermalRT_[0] = loadHDRRenderTexture(64, 48);
    maskThermalRT_[1] = loadHDRRenderTexture(64, 48);
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
    aplIndex_ = 0;
    maskThermalIndex_ = 0;
    receiverBurstIndex_ = 0;
    receiverVideoIndex_ = 0;
    receiverVerticalIndex_ = 0;
    heldVideoFrame_ = -1;
    pendingVideoValid_ = false;
    emissionHistoryValid_ = false;
    driveHistoryValid_ = false;
    crtLastPipelineTime = -1.0;
    clearRenderTexture(heldVideoRT_);
    clearRenderTexture(pendingVideoRT_);
    clearRenderTexture(signalRT_[0]);
    clearRenderTexture(signalRT_[1]);
    clearRenderTexture(sourceYiqRT_);
    clearRenderTexture(burstRT_);
    clearRenderTexture(receiverBurstRT_[0], {128, 128, 0, 128});
    clearRenderTexture(receiverBurstRT_[1], {128, 128, 0, 128});
    clearRenderTexture(receiverVideoRT_[0], {77, 128, 0, 128});
    clearRenderTexture(receiverVideoRT_[1], {77, 128, 0, 128});
    clearRenderTexture(receiverVerticalRT_[0], {128, 128, 0, 0});
    clearRenderTexture(receiverVerticalRT_[1], {128, 128, 0, 0});
    clearRenderTexture(phosphorRT_[0]);
    clearRenderTexture(phosphorRT_[1]);
    clearRenderTexture(phosphorMediumRT_[0]);
    clearRenderTexture(phosphorMediumRT_[1]);
    clearRenderTexture(phosphorSlowRT_[0]);
    clearRenderTexture(phosphorSlowRT_[1]);
    clearRenderTexture(previousEmissionRT_);
    clearRenderTexture(currentEmissionRT_);
    if (observerRT_[0].id) clearRenderTexture(observerRT_[0]);
    if (observerRT_[1].id) clearRenderTexture(observerRT_[1]);
    clearRenderTexture(aplRT_[0], {0, 255, 255, 255});
    clearRenderTexture(aplRT_[1], {0, 255, 255, 255});
    clearRenderTexture(previousDriveRT_);
    clearRenderTexture(currentDriveRT_);
    clearRenderTexture(maskThermalRT_[0]);
    clearRenderTexture(maskThermalRT_[1]);
    clearRenderTexture(bloomRT_[0]);
    clearRenderTexture(bloomRT_[1]);
    clearRenderTexture(bloomWideRT_[0]);
    clearRenderTexture(bloomWideRT_[1]);
    lastScreenW_ = sw; lastScreenH_ = sh;
}

void Shader_m::resetCRTHistory() {
    ensureTargets();
    phosphorIndex_ = 0;
    observerIndex_ = 0;
    aplIndex_ = 0;
    maskThermalIndex_ = 0;
    receiverBurstIndex_ = 0;
    receiverVideoIndex_ = 0;
    receiverVerticalIndex_ = 0;
    heldVideoFrame_ = -1;
    pendingVideoValid_ = false;
    emissionHistoryValid_ = false;
    driveHistoryValid_ = false;
    crtLastPipelineTime = -1.0;
    crtPipelineClockActive = false;
    crtFrameAdvanced = false;
    crtFramesAdvanced = 0;

    clearRenderTexture(heldVideoRT_);
    clearRenderTexture(pendingVideoRT_);
    clearRenderTexture(signalRT_[0]);
    clearRenderTexture(signalRT_[1]);
    clearRenderTexture(sourceYiqRT_);
    clearRenderTexture(burstRT_);
    clearRenderTexture(receiverBurstRT_[0], {128, 128, 0, 128});
    clearRenderTexture(receiverBurstRT_[1], {128, 128, 0, 128});
    clearRenderTexture(receiverVideoRT_[0], {77, 128, 0, 128});
    clearRenderTexture(receiverVideoRT_[1], {77, 128, 0, 128});
    clearRenderTexture(receiverVerticalRT_[0], {128, 128, 0, 0});
    clearRenderTexture(receiverVerticalRT_[1], {128, 128, 0, 0});
    for (auto& target : phosphorRT_) clearRenderTexture(target);
    for (auto& target : phosphorMediumRT_) clearRenderTexture(target);
    for (auto& target : phosphorSlowRT_) clearRenderTexture(target);
    clearRenderTexture(previousEmissionRT_);
    clearRenderTexture(currentEmissionRT_);
    for (auto& target : observerRT_) clearRenderTexture(target);
    clearRenderTexture(aplRT_[0], {0, 255, 255, 255});
    clearRenderTexture(aplRT_[1], {0, 255, 255, 255});
    clearRenderTexture(previousDriveRT_);
    clearRenderTexture(currentDriveRT_);
    for (auto& target : maskThermalRT_) clearRenderTexture(target);
    for (auto& target : ping_) clearRenderTexture(target);
    for (auto& target : bloomRT_) clearRenderTexture(target);
    for (auto& target : bloomWideRT_) clearRenderTexture(target);
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
    bool crtStage = name == "crt" || name == "phosphor_state" ||
                    name.rfind("crt_", 0) == 0;
    float time = crtStage && crtPipelineClockActive
        ? crtPipelineTime
        : (sceneTimeFrozen ? frozenSceneTime : static_cast<float>(GetTime()));
    float frameTime = crtStage && crtPipelineClockActive
        ? crtPipelineFrameTime
        : std::clamp((float)GetFrameTime(), 0.0f, 0.25f);
    float resolution[2] = { (float)source.width, (float)source.height };
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

    // Fixed timing for the engine's native 262-line progressive source and
    // electrical facts for the North-American CT-1358 receiver.
    setFloat("ntscSubcarrierMHz", 3.579545f);
    setFloat("ntscLineRateHz", static_cast<float>(CRT_LINE_RATE_HZ));
    setFloat("ntscFrameRateHz", static_cast<float>(CRT_FRAME_RATE_HZ));
    setFloat("ntscTotalLines", static_cast<float>(CRT_TOTAL_LINES));
    setFloat("ntscActiveLines", static_cast<float>(CRT_ACTIVE_LINES));
    setFloat("ntscActiveStartLine", static_cast<float>(CRT_ACTIVE_START_LINE));
    setFloat("ntscContentLines", static_cast<float>(CRT_CONTENT_LINES));
    setFloat("ntscContentStartLine",
        static_cast<float>(CRT_CONTENT_START_LINE));
    setFloat("ntscActiveVideoUs", 52.655f);
    setFloat("ntscSetupLevel", 0.075f);
    setFloat("crtFramePhase", crtFramePhase);
    setFloat("crtFramesAdvanced", static_cast<float>(crtFramesAdvanced));
    setFloat("outputGamma", params.outputGamma);
    setFloat("tubePeakNits", params.tubePeakNits);
    setFloat("hostPeakNits", params.hostPeakNits);
    setFloat("referenceWhiteRadiance", params.referenceWhiteRadiance);
    setFloat("ntscSourceLumaBandwidthMHz", params.ntscSourceLumaBandwidthMHz);
    setFloat("ntscSourceIBandwidthMHz", params.ntscSourceIBandwidthMHz);
    setFloat("ntscSourceQBandwidthMHz", params.ntscSourceQBandwidthMHz);
    setFloat("ntscLumaBandwidthMHz", params.ntscLumaBandwidthMHz);
    setFloat("ntscChromaBandwidthIMHz", params.ntscChromaBandwidthIMHz);
    setFloat("ntscChromaBandwidthQMHz", params.ntscChromaBandwidthQMHz);
    setFloat("ntscChromaGain", params.ntscChromaGain);
    setFloat("ntscChromaDelayNs", params.ntscChromaDelayNs);
    setFloat("ntscLineCombStrength", params.ntscLineCombStrength);
    setFloat("ntscLumaPeaking", params.ntscLumaPeaking);
    setFloat("ntscDifferentialGain", params.ntscDifferentialGain);
    setFloat("ntscDifferentialPhaseDeg", params.ntscDifferentialPhaseDeg);
    setFloat("ntscNoise", params.ntscNoise);
    setFloat("ntscHum", params.ntscHum);
    setFloat("ntscAgcResponse", params.ntscAgcResponse);
    setFloat("ntscClampResponse", params.ntscClampResponse);
    setFloat("ntscBurstPllBandwidthHz", params.ntscBurstPllBandwidthHz);
    setFloat("ntscHorizontalPllBandwidthHz", params.ntscHorizontalPllBandwidthHz);
    setFloat("ntscVerticalPllBandwidthHz", params.ntscVerticalPllBandwidthHz);
    setFloat("ntscAccResponse", params.ntscAccResponse);
    setFloat("ntscColorKillerThreshold", params.ntscColorKillerThreshold);
    setVec3("videoGain", params.videoGain);
    setVec3("videoCutoff", params.videoCutoff);
    setVec3("gunGamma", params.gunGamma);
    setFloat("beamMinWidth", params.beamMinWidth);
    setFloat("beamMaxWidth", params.beamMaxWidth);
    setFloat("beamShape", params.beamShape);
    setFloat("beamIntensityWeight", params.beamIntensityWeight);
    setFloat("beamScanlineStrength", params.beamScanlineStrength);
    setFloat("beamHorizontalSigma", params.beamHorizontalSigma);
    setFloat("beamCurrentLimit", params.beamCurrentLimit);
    setFloat("beamCurrentCompression", params.beamCurrentCompression);
    setFloat("videoOutputBandwidthMHz", params.videoOutputBandwidthMHz);
    setFloat("cathodeDriveHeadroom", params.cathodeDriveHeadroom);
    setFloat("spaceChargeCompression", params.spaceChargeCompression);
    setFloat("spotBloom", params.spotBloom);
    setFloat("dynamicFocus", params.dynamicFocus);
    setFloat("focusEdgeSoftness", params.focusEdgeSoftness);
    setFloat("astigmatism", params.astigmatism);
    setFloat("misconvergence", params.misconvergence);
    setFloat("horizontalJitter", params.horizontalJitter);
    setFloat("maskStrength", params.maskStrength);
    setFloat("maskTriadsAcross", params.maskTriadsAcross);
    setFloat("maskType", params.maskType);
    setFloat("maskHeating", params.maskHeating);
    setFloat("maskThermalTau", params.maskThermalTau);
    setFloat("maskThermalDiffusion", params.maskThermalDiffusion);
    setFloat("maskDoming", params.maskDoming);
    setFloat("maskCrosstalk", params.maskCrosstalk);
    setVec3("phosphorFastDecay", params.phosphorFastDecay);
    setVec3("phosphorMediumDecay", params.phosphorMediumDecay);
    setVec3("phosphorSlowDecay", params.phosphorSlowDecay);
    setFloat("phosphorMediumWeight", params.phosphorMediumWeight);
    setFloat("phosphorSlowWeight", params.phosphorSlowWeight);
    setFloat("phosphorSpread", params.phosphorSpread);
    setFloat("phosphorSaturation", params.phosphorSaturation);
    setFloat("observerIntegration", params.observerIntegration);
    setFloat("bloomThreshold", params.bloomThreshold);
    setFloat("bloomIntensity", params.bloomIntensity);
    setFloat("wideBloomIntensity", params.wideBloomIntensity);
    setFloat("bloomRadius", params.bloomRadius);
    setVec3("bloomRadiusRGB", params.bloomRadiusRGB);
    setFloat("halation", params.halation);
    setFloat("curvatureX", params.curvatureX);
    setFloat("curvatureY", params.curvatureY);
    setFloat("pincushion", params.pincushion);
    setFloat("highVoltageBloom", params.highVoltageBloom);
    setFloat("highVoltageResponse", params.highVoltageResponse);
    setFloat("highVoltageSag", params.highVoltageSag);
    setFloat("highVoltageRipple", params.highVoltageRipple);
    setFloat("bPlusResponse", params.bPlusResponse);
    setFloat("bPlusSag", params.bPlusSag);
    setFloat("bPlusRipple", params.bPlusRipple);
    setFloat("cornerRadius", params.cornerRadius);
    setFloat("vignette", params.vignette);
    setFloat("glassTransmission", params.glassTransmission);
    setVec3("glassTint", params.glassTint);
    setFloat("glassDispersion", params.glassDispersion);
    setFloat("glassRefractiveIndex", params.glassRefractiveIndex);
    setFloat("glassThicknessMm", params.glassThicknessMm);
    setVec3("glassAbsorption", params.glassAbsorption);
    setFloat("internalReflection", params.internalReflection);
    setFloat("ambientIlluminance", params.ambientIlluminance);
    setFloat("faceplateCurvatureX", params.faceplateCurvatureX);
    setFloat("faceplateCurvatureY", params.faceplateCurvatureY);
    setVec3("tubeColorMatrixR", params.tubeColorMatrixR);
    setVec3("tubeColorMatrixG", params.tubeColorMatrixG);
    setVec3("tubeColorMatrixB", params.tubeColorMatrixB);
    setFloat("reflection", params.reflection);
    setFloat("blackLevel", params.blackLevel);
    setFloat("brightness", params.brightness);
    setFloat("saturation", params.saturation);
    setFloat("flicker", params.flicker);
    setFloat("noise", params.noise);
    setFloat("testPattern", params.testPattern);
}

void Shader_m::bindTemporalTextures(Shader shader, const std::string& name) {
    (void)shader;
    (void)name;
}

Texture2D Shader_m::updatePhosphorState(Texture2D source) {
    if (!has("phosphor_state") || !has("crt_phosphor_combine") ||
        !phosphorRT_[0].id || !phosphorRT_[1].id ||
        !phosphorMediumRT_[0].id || !phosphorMediumRT_[1].id ||
        !phosphorSlowRT_[0].id || !phosphorSlowRT_[1].id ||
        !previousEmissionRT_.id || !currentEmissionRT_.id) {
        return source;
    }

    Shader shader = get("phosphor_state");
    auto copyEmission = [](Texture2D input, RenderTexture2D& target) {
        BeginTextureMode(target);
            ClearBackground(BLACK);
            DrawTextureRec(input,
                {0, 0, (float)input.width, -(float)input.height},
                {0, 0}, WHITE);
        EndTextureMode();
    };
    if (!emissionHistoryValid_) {
        copyEmission(source, previousEmissionRT_);
        copyEmission(source, currentEmissionRT_);
        emissionHistoryValid_ = true;
    } else if (crtFrameAdvanced) {
        copyEmission(currentEmissionRT_.texture, previousEmissionRT_);
    }
    auto updateLayer = [&](RenderTexture2D states[2], Vector3 decay) {
        RenderTexture2D& prev = states[phosphorIndex_];
        RenderTexture2D& dst = states[phosphorIndex_ ^ 1];

        BeginTextureMode(dst);
                ClearBackground(BLACK);
                BeginShaderMode(shader);
                    rlDrawRenderBatchActive();
                    rlDisableColorBlend();
                    uploadPassUniforms(shader, "phosphor_state", source);
                    int loc = GetShaderLocation(shader, "prevTexture");
                    if (loc >= 0) SetShaderValueTexture(shader, loc, prev.texture);
                    loc = GetShaderLocation(shader, "previousEmissionTexture");
                    if (loc >= 0) SetShaderValueTexture(
                        shader, loc, previousEmissionRT_.texture);
                    loc = GetShaderLocation(shader, "stateDecay");
                    if (loc >= 0) {
                        float value[3] = {decay.x, decay.y, decay.z};
                        SetShaderValue(shader, loc, value, SHADER_UNIFORM_VEC3);
                    }
                    DrawTextureRec(source,
                        {0,0,(float)source.width, -(float)source.height},
                        {0,0}, WHITE);
                    rlDrawRenderBatchActive();
                    rlEnableColorBlend();
                EndShaderMode();
        EndTextureMode();
    };

    const CRTParams& params = crtParams();
    updateLayer(phosphorRT_, params.phosphorFastDecay);
    updateLayer(phosphorMediumRT_, params.phosphorMediumDecay);
    updateLayer(phosphorSlowRT_, params.phosphorSlowDecay);

    // Current emission is refreshed throughout this progressive frame, while
    // previousEmissionRT_ remains frozen until the next vertical boundary.
    copyEmission(source, currentEmissionRT_);

    phosphorIndex_ ^= 1;
    return runFullscreenPass(
        "crt_phosphor_combine",
        phosphorRT_[phosphorIndex_].texture,
        ping_[0],
        "slowTexture",
        phosphorSlowRT_[phosphorIndex_].texture,
        "mediumTexture",
        phosphorMediumRT_[phosphorIndex_].texture,
        "currentEmissionTexture",
        currentEmissionRT_.texture,
        "previousEmissionTexture",
        previousEmissionRT_.texture
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

Texture2D Shader_m::updateAPLState(Texture2D source) {
    if (!has("crt_apl_state") || !aplRT_[0].id || !aplRT_[1].id ||
        !previousDriveRT_.id || !currentDriveRT_.id) {
        return {};
    }

    auto copyDrive = [](Texture2D input, RenderTexture2D& target) {
        BeginTextureMode(target);
            ClearBackground(BLACK);
            DrawTexturePro(input,
                {0, 0, (float)input.width, -(float)input.height},
                {0, 0, (float)target.texture.width,
                    (float)target.texture.height},
                {0, 0}, 0.0f, WHITE);
        EndTextureMode();
    };
    if (!driveHistoryValid_) {
        copyDrive(source, previousDriveRT_);
        copyDrive(source, currentDriveRT_);
        driveHistoryValid_ = true;
    } else if (crtFrameAdvanced) {
        copyDrive(currentDriveRT_.texture, previousDriveRT_);
    }

    RenderTexture2D& previous = aplRT_[aplIndex_];
    RenderTexture2D& target = aplRT_[aplIndex_ ^ 1];
    runFullscreenPass(
        "crt_apl_state",
        source,
        target,
        "prevTexture",
        previous.texture,
        "previousDriveTexture",
        previousDriveRT_.texture
    );
    copyDrive(source, currentDriveRT_);
    aplIndex_ ^= 1;
    return aplRT_[aplIndex_].texture;
}

Texture2D Shader_m::updateMaskThermalState(Texture2D source) {
    if (!has("crt_mask_thermal_state") ||
        !maskThermalRT_[0].id || !maskThermalRT_[1].id) {
        return {};
    }

    RenderTexture2D& previous = maskThermalRT_[maskThermalIndex_];
    RenderTexture2D& target = maskThermalRT_[maskThermalIndex_ ^ 1];
    runFullscreenPass(
        "crt_mask_thermal_state",
        source,
        target,
        "prevTexture",
        previous.texture
    );
    maskThermalIndex_ ^= 1;
    return maskThermalRT_[maskThermalIndex_].texture;
}

Texture2D Shader_m::updateReceiverBurstState(Texture2D source) {
    if (!has("crt_ntsc_burst_state") ||
        !receiverBurstRT_[0].id || !receiverBurstRT_[1].id) {
        return source;
    }

    RenderTexture2D& previous = receiverBurstRT_[receiverBurstIndex_];
    RenderTexture2D& target = receiverBurstRT_[receiverBurstIndex_ ^ 1];
    runFullscreenPass(
        "crt_ntsc_burst_state",
        source,
        target,
        "prevTexture",
        previous.texture
    );
    receiverBurstIndex_ ^= 1;
    return receiverBurstRT_[receiverBurstIndex_].texture;
}

Texture2D Shader_m::updateReceiverVideoState(Texture2D source) {
    if (!has("crt_ntsc_video_state") ||
        !receiverVideoRT_[0].id || !receiverVideoRT_[1].id) {
        return {};
    }

    RenderTexture2D& previous = receiverVideoRT_[receiverVideoIndex_];
    RenderTexture2D& target = receiverVideoRT_[receiverVideoIndex_ ^ 1];
    runFullscreenPass(
        "crt_ntsc_video_state",
        source,
        target,
        "prevTexture",
        previous.texture
    );
    receiverVideoIndex_ ^= 1;
    return receiverVideoRT_[receiverVideoIndex_].texture;
}

Texture2D Shader_m::updateReceiverVerticalState(Texture2D source) {
    if (!has("crt_ntsc_vertical_state") ||
        !receiverVerticalRT_[0].id || !receiverVerticalRT_[1].id) {
        return {};
    }

    RenderTexture2D& previous = receiverVerticalRT_[receiverVerticalIndex_];
    RenderTexture2D& target = receiverVerticalRT_[receiverVerticalIndex_ ^ 1];
    runFullscreenPass(
        "crt_ntsc_vertical_state",
        source,
        target,
        "prevTexture",
        previous.texture
    );
    receiverVerticalIndex_ ^= 1;
    return receiverVerticalRT_[receiverVerticalIndex_].texture;
}

Texture2D Shader_m::runFullscreenPass(
    const std::string& name,
    Texture2D source,
    RenderTexture2D& target,
    const char* extraUniform,
    Texture2D extraTexture,
    const char* secondExtraUniform,
    Texture2D secondExtraTexture,
    const char* thirdExtraUniform,
    Texture2D thirdExtraTexture,
    const char* fourthExtraUniform,
    Texture2D fourthExtraTexture
) {
    if (!has(name) || !source.id || !target.id) return source;

    Shader shader = get(name);
    BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginShaderMode(shader);
            // CRT render targets are numerical buffers, not composited UI.
            // Flush before sampler bindings: rlDrawRenderBatchActive() clears
            // raylib's additional texture units as part of its batch reset.
            rlDrawRenderBatchActive();
            rlDisableColorBlend();
            uploadPassUniforms(shader, name, source);
            float targetResolution[2] = {
                (float)target.texture.width,
                (float)target.texture.height
            };
            Rectangle targetDisplay = getCRTDisplayRect(
                target.texture.width,
                target.texture.height
            );
            float targetDisplayRect[4] = {
                targetDisplay.x,
                targetDisplay.y,
                targetDisplay.width,
                targetDisplay.height
            };
            int targetLoc = GetShaderLocation(shader, "targetResolution");
            if (targetLoc >= 0) {
                SetShaderValue(shader, targetLoc, targetResolution, SHADER_UNIFORM_VEC2);
            }
            targetLoc = GetShaderLocation(shader, "targetDisplayRect");
            if (targetLoc >= 0) {
                SetShaderValue(shader, targetLoc, targetDisplayRect, SHADER_UNIFORM_VEC4);
            }
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
            if (thirdExtraUniform && thirdExtraTexture.id) {
                int loc = GetShaderLocation(shader, thirdExtraUniform);
                if (loc >= 0) {
                    SetShaderValueTexture(shader, loc, thirdExtraTexture);
                }
            }
            if (fourthExtraUniform && fourthExtraTexture.id) {
                int loc = GetShaderLocation(shader, fourthExtraUniform);
                if (loc >= 0) {
                    SetShaderValueTexture(shader, loc, fourthExtraTexture);
                }
            }
            // Several state stages store real data in alpha (ACC gain, AFC
            // phase, heater state). The normal SRC_ALPHA blend would feed
            // that data back into RGB and corrupt every persistent loop.
            DrawTexturePro(
                source,
                {0, 0, (float)source.width, -(float)source.height},
                {0, 0, (float)target.texture.width, (float)target.texture.height},
                {0, 0},
                0.0f,
                WHITE
            );
            rlDrawRenderBatchActive();
            rlEnableColorBlend();
        EndShaderMode();
    EndTextureMode();
    return target.texture;
}

Texture2D Shader_m::applyCRTPipeline(Texture2D source) {
    crtLastFrameApplied = false;
    // Every pass observes the same native progressive frame and raster phase.
    // The frame clock is derived from exactly 262 complete horizontal lines.
    constexpr double frameRate = CRT_FRAME_RATE_HZ;
    const double preciseTime = GetTime();
    const double preciseFrame = preciseTime * frameRate;
    const long long preciseFrameIndex = static_cast<long long>(
        std::floor(preciseFrame));
    // Keep the shader clock small enough that a 32-bit float still resolves
    // microsecond raster time after the game has been running for hours.
    crtPipelineTime = static_cast<float>(std::fmod(preciseTime, 256.0));
    // Temporal stages must see wall-clock time, not a value truncated at
    // 250 ms.  Otherwise pausing, moving the window or a debugger stop leaves
    // stale phosphor, AGC and supply state on screen for several frames.
    double elapsed = crtLastPipelineTime >= 0.0 &&
            preciseTime >= crtLastPipelineTime
        ? preciseTime - crtLastPipelineTime
        : static_cast<double>(GetFrameTime());
    crtLastPipelineTime = preciseTime;
    // One hour is already many orders of magnitude beyond the slowest modeled
    // time constant.  The finite ceiling also keeps event indices exactly
    // representable in the GLSL float path after an abnormally long suspend.
    crtPipelineFrameTime = static_cast<float>(std::clamp(
        elapsed, 0.000001, 3600.0));
    crtFramePhase = static_cast<float>(
        preciseFrame - std::floor(preciseFrame));
    crtPipelineClockActive = true;
    const char* required[] = {
        "crt_ntsc_prefilter",
        "crt_ntsc_encode",
        "crt_ntsc_bandpass",
        "crt_ntsc_burst",
        "crt_ntsc_burst_state",
        "crt_ntsc_video_state",
        "crt_ntsc_vertical_state",
        "crt_ntsc_decode",
        "crt_drive",
        "crt_deflection",
        "crt_scan_vertical",
        "crt_mask_thermal_state",
        "crt_emission",
        "phosphor_state",
        "crt_phosphor_combine",
        "crt_tube_color",
        "crt_apl_state",
        "crt_brightpass",
        "crt_bloom_downsample",
        "crt_bloom_h",
        "crt_bloom_v",
        "crt"
    };
    for (const char* name : required) {
        if (!has(name)) {
            TraceLog(LOG_WARNING, "CRT: required stage '%s' is unavailable", name);
            crtPipelineClockActive = false;
            return source;
        }
    }

    auto isHDR = [](const RenderTexture2D& target) {
        return target.id && target.texture.format ==
            PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    };
    const bool faithfulPrecision =
        isHDR(signalRT_[0]) && isHDR(signalRT_[1]) && isHDR(sourceYiqRT_) &&
        isHDR(burstRT_) && isHDR(receiverBurstRT_[0]) &&
        isHDR(receiverBurstRT_[1]) && isHDR(receiverVideoRT_[0]) &&
        isHDR(receiverVideoRT_[1]) && isHDR(receiverVerticalRT_[0]) &&
        isHDR(receiverVerticalRT_[1]) && isHDR(phosphorRT_[0]) &&
        isHDR(phosphorRT_[1]) && isHDR(phosphorMediumRT_[0]) &&
        isHDR(phosphorMediumRT_[1]) && isHDR(phosphorSlowRT_[0]) &&
        isHDR(phosphorSlowRT_[1]) && isHDR(previousEmissionRT_) &&
        isHDR(currentEmissionRT_) && isHDR(aplRT_[0]) && isHDR(aplRT_[1]) &&
        isHDR(previousDriveRT_) && isHDR(currentDriveRT_) &&
        isHDR(maskThermalRT_[0]) && isHDR(maskThermalRT_[1]) &&
        isHDR(ping_[0]) && isHDR(ping_[1]) && isHDR(bloomRT_[0]) &&
        isHDR(bloomRT_[1]) && isHDR(bloomWideRT_[0]) &&
        isHDR(bloomWideRT_[1]) &&
        (crtParams().observerIntegration <= 0.0f ||
            (isHDR(observerRT_[0]) && isHDR(observerRT_[1])));
    if (!faithfulPrecision) {
        static bool warnedAboutPrecision = false;
        if (!warnedAboutPrecision) {
            TraceLog(LOG_ERROR,
                "CRT: RGBA16F UHD allocation failed; faithful CRT disabled");
            warnedAboutPrecision = true;
        }
        crtPipelineClockActive = false;
        return source;
    }

    long long frameIndex = preciseFrameIndex;
    auto copyVideoFrame = [](Texture2D input, RenderTexture2D& target) {
        if (!input.id || !target.id) return;
        BeginTextureMode(target);
            ClearBackground(BLACK);
            DrawTextureRec(input,
                {0, 0, (float)input.width, -(float)input.height},
                {0, 0}, WHITE);
        EndTextureMode();
    };
    crtFrameAdvanced = heldVideoFrame_ != frameIndex;
    crtFramesAdvanced = heldVideoFrame_ >= 0
        ? static_cast<int>(std::clamp<long long>(
            frameIndex - heldVideoFrame_, 0, 1000000))
        : 0;
    if (!pendingVideoValid_) {
        copyVideoFrame(source, pendingVideoRT_);
        pendingVideoValid_ = true;
    }
    if (heldVideoFrame_ != frameIndex && heldVideoRT_.id) {
        // Commit only the last complete framebuffer that existed before this
        // progressive vertical boundary. The newly rendered host image is
        // queued for the next boundary, which prevents mid-raster tearing.
        copyVideoFrame(pendingVideoRT_.texture, heldVideoRT_);
        heldVideoFrame_ = frameIndex;
    }
    copyVideoFrame(source, pendingVideoRT_);
    Texture2D videoSource = heldVideoRT_.id ? heldVideoRT_.texture : source;
    if (crtParams().testPattern >= 0.5f && has("crt_test_pattern")) {
        RenderTexture2D& patternTarget =
            nativePing_[0].texture.id == source.id
            ? nativePing_[1] : nativePing_[0];
        videoSource = runFullscreenPass(
            "crt_test_pattern",
            source,
            patternTarget
        );
    }
    Texture2D filteredYiq = runFullscreenPass(
        "crt_ntsc_prefilter",
        videoSource,
        sourceYiqRT_
    );
    Texture2D composite = runFullscreenPass(
        "crt_ntsc_encode",
        filteredYiq,
        signalRT_[0]
    );
    Texture2D separatedComposite = runFullscreenPass(
        "crt_ntsc_bandpass",
        composite,
        signalRT_[1]
    );
    Texture2D videoState = updateReceiverVideoState(separatedComposite);
    Texture2D verticalState = updateReceiverVerticalState(separatedComposite);
    Texture2D burstMeasurement = runFullscreenPass(
        "crt_ntsc_burst",
        separatedComposite,
        burstRT_
    );
    Texture2D burstPhase = updateReceiverBurstState(burstMeasurement);
    Texture2D signal = runFullscreenPass(
        "crt_ntsc_decode",
        separatedComposite,
        ping_[1],
        "burstPhaseTexture",
        burstPhase,
        "videoStateTexture",
        videoState,
        "verticalStateTexture",
        verticalState
    );
    Texture2D drive = runFullscreenPass(
        "crt_drive",
        signal,
        ping_[0],
        "aplTexture",
        aplRT_[aplIndex_].texture
    );
    Texture2D apl = updateAPLState(drive);
    Texture2D deflected = runFullscreenPass(
        "crt_deflection",
        drive,
        ping_[1],
        "aplTexture",
        apl
    );
    Texture2D vertical = runFullscreenPass(
        "crt_scan_vertical",
        deflected,
        ping_[0],
        "aplTexture",
        apl
    );
    Texture2D maskThermal = updateMaskThermalState(vertical);
    Texture2D emission = runFullscreenPass(
        "crt_emission",
        vertical,
        ping_[1],
        "maskThermalTexture",
        maskThermal
    );
    Texture2D phosphor = updatePhosphorState(emission);
    Texture2D tubeColor = runFullscreenPass(
        "crt_tube_color",
        phosphor,
        ping_[1]
    );
    Texture2D observed = crtParams().observerIntegration > 0.0f
        ? updateObserverState(tubeColor)
        : tubeColor;
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
    Texture2D result = runFullscreenPass(
        "crt",
        observed,
        ping_[0],
        "bloomTexture",
        bloom,
        "wideBloomTexture",
        wideBloom,
        "aplTexture",
        apl
    );
    crtLastFrameApplied = result.width == CRT_RASTER_WIDTH &&
        result.height == CRT_RASTER_HEIGHT;
    crtPipelineClockActive = false;
    return result;
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

    // The native framebuffer itself contains no presentation padding. Every
    // CRT image plane derives its rectangle from the same UHD reference so
    // quarter-resolution bloom and the final raster stay pixel-aligned.
    if (targetW == NATIVE_RES_WIDTH && targetH == NATIVE_RES_HEIGHT) {
        return {0.0f, 0.0f, static_cast<float>(targetW),
            static_cast<float>(targetH)};
    }
    const float scaleX = static_cast<float>(targetW) / CRT_RASTER_WIDTH;
    const float scaleY = static_cast<float>(targetH) / CRT_RASTER_HEIGHT;
    return {
        CRT_TUBE_X * scaleX,
        CRT_TUBE_Y * scaleY,
        CRT_TUBE_WIDTH * scaleX,
        CRT_TUBE_HEIGHT * scaleY
    };
}

static Rectangle getRasterPresentationRect(int targetW, int targetH) {
    if (targetW <= 0 || targetH <= 0) return {0, 0, 0, 0};
    const float scale = std::min(
        static_cast<float>(targetW) / CRT_RASTER_WIDTH,
        static_cast<float>(targetH) / CRT_RASTER_HEIGHT);
    const float width = CRT_RASTER_WIDTH * scale;
    const float height = CRT_RASTER_HEIGHT * scale;
    return {
        (static_cast<float>(targetW) - width) * 0.5f,
        (static_cast<float>(targetH) - height) * 0.5f,
        width,
        height
    };
}

static void drawReferenceRaster(Texture2D texture) {
    if (GetScreenWidth() == CRT_RASTER_WIDTH &&
        GetScreenHeight() == CRT_RASTER_HEIGHT &&
        texture.width == CRT_RASTER_WIDTH &&
        texture.height == CRT_RASTER_HEIGHT) {
        // Exact texel-for-pixel presentation: no reconstruction kernel is
        // invoked after the simulated glass stage.
        DrawTextureRec(texture,
            {0, 0, static_cast<float>(texture.width),
                -static_cast<float>(texture.height)},
            {0, 0}, WHITE);
        return;
    }
    const Rectangle destination = getRasterPresentationRect(
        GetScreenWidth(), GetScreenHeight());
    DrawTexturePro(
        texture,
        {0, 0, static_cast<float>(texture.width),
            -static_cast<float>(texture.height)},
        destination,
        {0, 0},
        0.0f,
        WHITE
    );
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

void Shader_m::captureRequestedScreenshots(bool crtApplied,
                                           Texture2D referenceOutput) {
    if (screenshotQueue_.empty()) return;

    rlDrawRenderBatchActive();

    const int pattern = std::clamp(
        static_cast<int>(std::lround(crtParams().testPattern)), 0, 6);
    const auto capturedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const std::filesystem::path& path : screenshotQueue_) {
        std::error_code error;
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path(), error);
        }
        lastScreenshotPath_ = path;
        lastScreenshotSucceeded_ = false;
        if (error) {
            TraceLog(LOG_WARNING, "Screenshot: cannot create '%s' (%s)",
                path.parent_path().string().c_str(), error.message().c_str());
            continue;
        }

        // Capture the fixed reference raster itself. This guarantees UHD
        // analysis images even when a window manager scales or constrains the
        // visible preview window.
        Image framebuffer = LoadImageFromTexture(referenceOutput);
        if (framebuffer.data) {
            ImageFlipVertical(&framebuffer);
            ImageFormat(&framebuffer,
                PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        }
        const bool exported = framebuffer.data &&
            ExportImage(framebuffer, path.string().c_str());
        if (framebuffer.data) UnloadImage(framebuffer);
        lastScreenshotSucceeded_ = exported &&
            std::filesystem::is_regular_file(path, error);
        if (!lastScreenshotSucceeded_ || error) {
            TraceLog(LOG_WARNING, "Screenshot: failed to write '%s'",
                path.string().c_str());
            continue;
        }

        json metadata = {
            {"schema", "rayengine.screenshot.v1"},
            {"image", path.filename().string()},
            {"capturedAtUnixMs", capturedAt},
            {"width", referenceOutput.width},
            {"height", referenceOutput.height},
            {"previewWidth", GetScreenWidth()},
            {"previewHeight", GetScreenHeight()},
            {"capturePoint", "fixed reference raster before FPS and ImGui"},
            {"crtEnabled", crtApplied},
            {"testPattern", pattern},
            {"testPatternName", crtTestPatternName(pattern)},
            {"renderLayout", {
                {"source", {{"width", NATIVE_RES_WIDTH},
                    {"height", NATIVE_RES_HEIGHT}}},
                {"signal", {{"width", CRT_SIGNAL_SAMPLES},
                    {"height", CRT_TOTAL_LINES}}},
                {"raster", {{"width", CRT_RASTER_WIDTH},
                    {"height", CRT_RASTER_HEIGHT}, {"format", "RGBA16F"}}},
                {"tubeRect", {{"x", CRT_TUBE_X}, {"y", CRT_TUBE_Y},
                    {"width", CRT_TUBE_WIDTH}, {"height", CRT_TUBE_HEIGHT}}},
                {"nativePresentationScale", CRT_NATIVE_PRESENTATION_SCALE},
                {"contentStartLine", CRT_CONTENT_START_LINE},
                {"contentLines", CRT_CONTENT_LINES},
                {"finalFilterAtReferenceSize", "none_1_to_1"}
            }},
            {"crtProfile", {
                {"manufacturer", "Hitachi"},
                {"model", "CT-1358"},
                {"calibrationStatus", crtParams().calibrationStatus},
                {"measuredSerial", crtParams().measuredSerial},
                {"timing", "rayEngine-320x192-262p"},
                {"frameRateHz", CRT_FRAME_RATE_HZ}
            }},
            {"parameterSnapshot", "crt_params.json"}
        };
        if (crtApplied) {
            metadata["receiverState"] = {
                {"burstMeasurement", sampleStateTexture(burstRT_.texture)},
                {"burstPll", sampleStateTexture(
                    receiverBurstRT_[receiverBurstIndex_].texture)},
                {"videoAgcAfc", sampleStateTexture(
                    receiverVideoRT_[receiverVideoIndex_].texture)},
                {"verticalPll", sampleStateTexture(
                    receiverVerticalRT_[receiverVerticalIndex_].texture)},
                {"powerApl", sampleStateTexture(aplRT_[aplIndex_].texture)}
            };
        }
        std::filesystem::path metadataPath = path;
        metadataPath.replace_extension(".json");
        std::ofstream metadataFile(metadataPath, std::ios::trunc);
        if (metadataFile.good()) metadataFile << metadata.dump(2);
        TraceLog(LOG_INFO, "Screenshot: saved '%s'", path.string().c_str());
    }
    screenshotQueue_.clear();
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

    Texture2D outTex{};
    if (displayPasses.empty()) {
        BeginTextureMode(postRT_);
            ClearBackground(BLACK);
            Rectangle sourceRect{0, 0, (float)nativeTex.width,
                -(float)nativeTex.height};
            Rectangle tubeRect = getCRTDisplayRect(
                postRT_.texture.width, postRT_.texture.height);
            DrawTexturePro(nativeTex, sourceRect, tubeRect,
                {0, 0}, 0.0f, WHITE);
        EndTextureMode();
        outTex = postRT_.texture;
    } else {
        // The vertical latch remains native 320x192. Composite generation is
        // the first resampling stage; decoded cathode drive is then written
        // directly into the fixed UHD raster.
        outTex = applyCRTPipeline(nativeTex);
        if (outTex.width != CRT_RASTER_WIDTH ||
            outTex.height != CRT_RASTER_HEIGHT) {
            static bool warnedAboutRasterFallback = false;
            if (!warnedAboutRasterFallback) {
                TraceLog(LOG_ERROR,
                    "CRT: UHD raster unavailable; presenting native image instead");
                warnedAboutRasterFallback = true;
            }
            BeginTextureMode(postRT_);
                ClearBackground(BLACK);
                Rectangle sourceRect{0, 0, (float)nativeTex.width,
                    -(float)nativeTex.height};
                Rectangle tubeRect = getCRTDisplayRect(
                    postRT_.texture.width, postRT_.texture.height);
                DrawTexturePro(nativeTex, sourceRect, tubeRect,
                    {0, 0}, 0.0f, WHITE);
            EndTextureMode();
            outTex = postRT_.texture;
        }
    }
    drawReferenceRaster(outTex);
    // Capture the actual presented framebuffer here: the CRT is included when
    // active, while FPS and ImGui overlays have not been drawn yet.
    captureRequestedScreenshots(
        !displayPasses.empty() && crtLastFrameApplied, outTex);
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
