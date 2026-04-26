#include "shader_m.h"
#include "raycam_m.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "definitions.h"
#include "rlgl.h"

using json = nlohmann::json;

std::unordered_map<std::string, Shader> Shader_m::shaders_;
std::filesystem::path Shader_m::dir_;
RenderTexture2D Shader_m::sceneRT_{};
RenderTexture2D Shader_m::postRT_{};
RenderTexture2D Shader_m::prevSceneRT_{};
RenderTexture2D Shader_m::ping_[2] = {};
RenderTexture2D Shader_m::crtComposite_[2] = {};
RenderTexture2D Shader_m::crtFullRT_{};
RenderTexture2D Shader_m::crtDownsampleRT_{};
RenderTexture2D Shader_m::crtUpsampleRT_{};
RenderTexture2D Shader_m::trueSignalRT_{};
RenderTexture2D Shader_m::trueBeamRT_{};
RenderTexture2D Shader_m::trueEmissionRT_{};
RenderTexture2D Shader_m::truePhosphor_[2] = {};
Texture2D Shader_m::crtMaskTexture_{};
Texture2D Shader_m::crtArtifactsTexture_{};
Model Shader_m::crtScreenModel_{};
Model Shader_m::crtFrameModel_{};
bool Shader_m::crtScreenModelLoaded_ = false;
bool Shader_m::crtFrameModelLoaded_ = false;
int Shader_m::pingIndex_ = 0;
int Shader_m::crtCompositeIndex_ = 0;
bool Shader_m::crtEvenFrame_ = true;
int Shader_m::lastW_ = 0;
int Shader_m::lastH_ = 0;
int Shader_m::lastScreenW_ = 0;
int Shader_m::lastScreenH_ = 0;
std::vector<Shader_m::Pass> Shader_m::queue_;
std::unordered_map<std::string, std::filesystem::file_time_type> Shader_m::fileTimes_;

namespace {
constexpr const char* CRT_PARAMS_PATH = "crt_params.json";
bool crtParamsLoaded = false;

float readFloat(const json& j, const char* key, float fallback) {
    if (!j.contains(key) || !j[key].is_number()) return fallback;
    return j[key].get<float>();
}

Vector3 readVector3(const json& j, const char* key, Vector3 fallback) {
    if (!j.contains(key) || !j[key].is_object()) return fallback;
    const json& v = j[key];
    return {
        readFloat(v, "x", fallback.x),
        readFloat(v, "y", fallback.y),
        readFloat(v, "z", fallback.z),
    };
}

template<typename T>
bool readPod(std::ifstream& file, T& value) {
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    return file.good();
}

bool readBytes(std::ifstream& file, void* dst, std::size_t size) {
    file.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(size));
    return file.good();
}

void freeMeshArrays(Mesh& mesh) {
    if (mesh.vertices) MemFree(mesh.vertices);
    if (mesh.texcoords) MemFree(mesh.texcoords);
    if (mesh.texcoords2) MemFree(mesh.texcoords2);
    if (mesh.normals) MemFree(mesh.normals);
    if (mesh.colors) MemFree(mesh.colors);
    if (mesh.indices) MemFree(mesh.indices);
    mesh = {};
}

float readStreamFloat(const std::vector<unsigned char>& data, std::size_t offset) {
    float value = 0.0f;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

Model loadM3DModel(const std::filesystem::path& path, bool& loaded) {
    loaded = false;
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        TraceLog(LOG_WARNING, "CRTSim: missing M3D model %s", path.string().c_str());
        return {};
    }

    std::uint32_t magic = 0;
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
    std::uint8_t alignment[2] = {};
    std::uint32_t streamCount = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
    bool unused = false;
    std::uint32_t indexSize = 0;

    if (!readPod(file, magic) || !readPod(file, major) || !readPod(file, minor) ||
        !readBytes(file, alignment, sizeof(alignment)) || !readPod(file, streamCount) ||
        !readPod(file, vertexCount) || !readPod(file, indexCount) || !readPod(file, unused) ||
        !readPod(file, indexSize)) {
        TraceLog(LOG_WARNING, "CRTSim: invalid M3D header %s", path.string().c_str());
        return {};
    }

    constexpr std::uint32_t M3D_MAGIC = 0x64336d2e;
    if (magic != M3D_MAGIC || indexSize != sizeof(unsigned short) || vertexCount == 0 ||
        indexCount == 0 || indexCount % 3 != 0) {
        TraceLog(LOG_WARNING, "CRTSim: unsupported M3D model %s", path.string().c_str());
        return {};
    }

    Mesh mesh = {};
    mesh.vertexCount = static_cast<int>(vertexCount);
    mesh.triangleCount = static_cast<int>(indexCount / 3);
    mesh.indices = static_cast<unsigned short*>(MemAlloc(indexCount * sizeof(unsigned short)));
    if (!mesh.indices || !readBytes(file, mesh.indices, indexCount * sizeof(unsigned short))) {
        freeMeshArrays(mesh);
        TraceLog(LOG_WARNING, "CRTSim: invalid M3D indices %s", path.string().c_str());
        return {};
    }

    for (std::uint32_t stream = 0; stream < streamCount; ++stream) {
        std::uint32_t usage = 0;
        std::uint32_t stride = 0;
        if (!readPod(file, usage) || !readPod(file, stride) || stride == 0) {
            freeMeshArrays(mesh);
            TraceLog(LOG_WARNING, "CRTSim: invalid M3D stream %s", path.string().c_str());
            return {};
        }

        const std::size_t streamSize = static_cast<std::size_t>(vertexCount) * stride;
        std::vector<unsigned char> data(streamSize);
        if (!readBytes(file, data.data(), streamSize)) {
            freeMeshArrays(mesh);
            TraceLog(LOG_WARNING, "CRTSim: truncated M3D stream %s", path.string().c_str());
            return {};
        }

        switch (usage) {
            case 0: { // Position
                if (stride < sizeof(float) * 3) break;
                mesh.vertices = static_cast<float*>(MemAlloc(vertexCount * 3 * sizeof(float)));
                for (std::uint32_t i = 0; i < vertexCount; ++i) {
                    const std::size_t base = static_cast<std::size_t>(i) * stride;
                    mesh.vertices[i * 3 + 0] = readStreamFloat(data, base + sizeof(float) * 0);
                    mesh.vertices[i * 3 + 1] = readStreamFloat(data, base + sizeof(float) * 1);
                    mesh.vertices[i * 3 + 2] = readStreamFloat(data, base + sizeof(float) * 2);
                }
            } break;
            case 1: { // Normal
                if (stride < sizeof(float) * 3) break;
                mesh.normals = static_cast<float*>(MemAlloc(vertexCount * 3 * sizeof(float)));
                for (std::uint32_t i = 0; i < vertexCount; ++i) {
                    const std::size_t base = static_cast<std::size_t>(i) * stride;
                    mesh.normals[i * 3 + 0] = readStreamFloat(data, base + sizeof(float) * 0);
                    mesh.normals[i * 3 + 1] = readStreamFloat(data, base + sizeof(float) * 1);
                    mesh.normals[i * 3 + 2] = readStreamFloat(data, base + sizeof(float) * 2);
                }
            } break;
            case 4: { // D3DCOLOR AARRGGBB
                if (stride < sizeof(std::uint32_t)) break;
                mesh.colors = static_cast<unsigned char*>(MemAlloc(vertexCount * 4 * sizeof(unsigned char)));
                for (std::uint32_t i = 0; i < vertexCount; ++i) {
                    std::uint32_t color = 0;
                    std::memcpy(&color, data.data() + i * stride, sizeof(color));
                    mesh.colors[i * 4 + 0] = static_cast<unsigned char>((color >> 16) & 0xff);
                    mesh.colors[i * 4 + 1] = static_cast<unsigned char>((color >> 8) & 0xff);
                    mesh.colors[i * 4 + 2] = static_cast<unsigned char>(color & 0xff);
                    mesh.colors[i * 4 + 3] = static_cast<unsigned char>((color >> 24) & 0xff);
                }
            } break;
            case 5: { // Texcoord0
                if (stride < sizeof(float) * 2) break;
                mesh.texcoords = static_cast<float*>(MemAlloc(vertexCount * 2 * sizeof(float)));
                for (std::uint32_t i = 0; i < vertexCount; ++i) {
                    const std::size_t base = static_cast<std::size_t>(i) * stride;
                    mesh.texcoords[i * 2 + 0] = readStreamFloat(data, base + sizeof(float) * 0);
                    mesh.texcoords[i * 2 + 1] = readStreamFloat(data, base + sizeof(float) * 1);
                }
            } break;
            case 6: { // Texcoord1, used by frame as reflection blend
                if (stride < sizeof(float)) break;
                mesh.texcoords2 = static_cast<float*>(MemAlloc(vertexCount * 2 * sizeof(float)));
                for (std::uint32_t i = 0; i < vertexCount; ++i) {
                    const std::size_t base = static_cast<std::size_t>(i) * stride;
                    mesh.texcoords2[i * 2 + 0] = readStreamFloat(data, base);
                    mesh.texcoords2[i * 2 + 1] = 0.0f;
                }
            } break;
            default:
                break;
        }
    }

    if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.colors) {
        freeMeshArrays(mesh);
        TraceLog(LOG_WARNING, "CRTSim: incomplete M3D model %s", path.string().c_str());
        return {};
    }

    UploadMesh(&mesh, false);
    Model model = LoadModelFromMesh(mesh);
    loaded = model.meshCount > 0;
    if (!loaded) {
        freeMeshArrays(mesh);
    }
    return model;
}
} // namespace

Shader_m::CRTParams& Shader_m::crtParams() {
    static CRTParams params;
    return params;
}

void Shader_m::resetCRTParams() {
    crtParams() = CRTParams{};
}

void Shader_m::loadCRTSimPreset() {
    CRTParams& params = crtParams();
    params = CRTParams{};
    params.sharpness = 0.8f;
    params.persistence = 0.7f;
    params.persistenceR = 0.7f;
    params.persistenceG = 0.525f;
    params.persistenceB = 0.42f;
    params.bleed = 0.5f;
    params.ntscArtifacts = 0.5f;
    params.ntscAlternation = 1.0f;
    params.overscan = 1.0f;
    params.pixelRatio = 8.0f / 7.0f;
    params.dimming = 0.5f;
    params.saturation = 1.35f;
    params.reflectionScalar = 0.3f;
    // CRTSim's Tuning_Barrel is -0.115; our shaders expose the positive magnitude.
    params.curvature = 0.115f;
    params.maskBrightness = 0.45f;
    params.maskOpacity = 1.0f;
    params.bloomDownsampleSpread = 0.025f;
    params.bloomUpsampleSpread = 0.025f;
    params.bloomSpread = 0.025f;
    params.bloomIntensity = 0.25f;
    params.bloomPower = 2.0f;
    params.geometry3D = 1.0f;
    params.frameEnabled = 1.0f;
    params.diffuseBrightness = 0.5f;
    params.specBrightness = 0.35f;
    params.specPower = 50.0f;
    params.fresnelBrightness = 1.0f;
    params.lightPos = {-10.0f, -5.0f, 10.0f};
    params.frameColor = {15, 15, 15, 255};
}

void Shader_m::loadTrueCRTPreset() {
    CRTParams& params = crtParams();
    params = CRTParams{};
    params.trueCRT = 1.0f;
    params.curvature = 0.115f;
    params.vignette = 0.18f;
    params.edgeSoftness = 0.035f;
    params.brightness = 0.68f;
    params.overscan = 1.0f;
    params.pixelRatio = 1.0f;
    params.saturation = 1.0f;
    params.geometry3D = 0.0f;
    params.frameEnabled = 0.0f;
    params.reflectionScalar = 0.28f;
    params.diffuseBrightness = 0.5f;
    params.specBrightness = 0.35f;
    params.specPower = 50.0f;
    params.fresnelBrightness = 0.85f;
    params.lightPos = {-10.0f, -5.0f, 10.0f};
    params.frameColor = {15, 15, 15, 255};

    params.signalMode = 0.0f;
    params.lumaBandwidth = 0.96f;
    params.chromaBandwidth = 0.92f;
    params.chromaDelay = 0.0f;
    params.ntscPhase = 0.0f;
    params.dotCrawl = 0.0f;

    params.beamWidthX = 0.75f;
    params.beamWidthY = 0.95f;
    params.beamFocus = 1.0f;
    params.beamBloom = 0.08f;
    params.beamScanlineStrength = 0.68f;
    params.edgeDefocus = 0.06f;
    params.convergenceR = {0.0f, 0.0f, 0.0f};
    params.convergenceG = {0.0f, 0.0f, 0.0f};
    params.convergenceB = {0.0f, 0.0f, 0.0f};

    params.phosphorLayout = 2.0f;
    params.phosphorPitchPx = 3.0f;
    params.phosphorRoundness = 0.35f;
    params.phosphorGap = 0.42f;
    params.phosphorGain = 1.45f;
    params.blackMatrix = 0.5f;
    params.phosphorBleed = 0.08f;

    params.afterglowR = 0.06f;
    params.afterglowG = 0.09f;
    params.afterglowB = 0.12f;
    params.afterglowThreshold = 0.015f;

    params.glassDiffusion = 0.035f;
    params.halation = 0.0f;
    params.tubeGlow = 0.04f;
    params.whitePoint = 0.82f;
    params.inputGamma = 2.4f;
    params.outputGamma = 2.4f;
    params.bloomIntensity = 0.035f;
    params.bloomDownsampleSpread = 0.012f;
    params.bloomUpsampleSpread = 0.018f;
    params.bloomSpread = params.bloomDownsampleSpread;
    params.bloomPower = 1.25f;
}

bool Shader_m::saveCRTParams() {
    const CRTParams& params = crtParams();
    json j = {
        {"curvature", params.curvature},
        {"vignette", params.vignette},
        {"edgeSoftness", params.edgeSoftness},
        {"glow", params.glow},
        {"dotMask", params.dotMask},
        {"dotBlur", params.dotBlur},
        {"bleed", params.bleed},
        {"dotGridSize", params.dotGridSize},
        {"hexGrid", params.hexGrid},
        {"alternateLineShift", params.alternateLineShift},
        {"scanline", params.scanline},
        {"chromaticAberration", params.chromaticAberration},
        {"brightness", params.brightness},
        {"sharpness", params.sharpness},
        {"persistence", params.persistence},
        {"persistenceR", params.persistenceR},
        {"persistenceG", params.persistenceG},
        {"persistenceB", params.persistenceB},
        {"ntscArtifacts", params.ntscArtifacts},
        {"ntscAlternation", params.ntscAlternation},
        {"overscan", params.overscan},
        {"pixelRatio", params.pixelRatio},
        {"dimming", params.dimming},
        {"saturation", params.saturation},
        {"maskBrightness", params.maskBrightness},
        {"maskOpacity", params.maskOpacity},
        {"maskScale", params.maskScale},
        {"bloomIntensity", params.bloomIntensity},
        {"bloomDownsampleSpread", params.bloomDownsampleSpread},
        {"bloomUpsampleSpread", params.bloomUpsampleSpread},
        {"bloomSpread", params.bloomSpread},
        {"bloomPower", params.bloomPower},
        {"geometry3D", params.geometry3D},
        {"frameEnabled", params.frameEnabled},
        {"reflectionScalar", params.reflectionScalar},
        {"diffuseBrightness", params.diffuseBrightness},
        {"specBrightness", params.specBrightness},
        {"specPower", params.specPower},
        {"fresnelBrightness", params.fresnelBrightness},
        {"lightPos", {{"x", params.lightPos.x}, {"y", params.lightPos.y}, {"z", params.lightPos.z}}},
        {"frameColor", {{"r", params.frameColor.r}, {"g", params.frameColor.g}, {"b", params.frameColor.b}, {"a", params.frameColor.a}}},
        {"trueCRT", params.trueCRT},
        {"trueCRTDebugView", params.trueCRTDebugView},
        {"signalMode", params.signalMode},
        {"lumaBandwidth", params.lumaBandwidth},
        {"chromaBandwidth", params.chromaBandwidth},
        {"chromaDelay", params.chromaDelay},
        {"ntscPhase", params.ntscPhase},
        {"dotCrawl", params.dotCrawl},
        {"beamWidthX", params.beamWidthX},
        {"beamWidthY", params.beamWidthY},
        {"beamFocus", params.beamFocus},
        {"beamBloom", params.beamBloom},
        {"beamScanlineStrength", params.beamScanlineStrength},
        {"edgeDefocus", params.edgeDefocus},
        {"convergenceR", {{"x", params.convergenceR.x}, {"y", params.convergenceR.y}, {"z", params.convergenceR.z}}},
        {"convergenceG", {{"x", params.convergenceG.x}, {"y", params.convergenceG.y}, {"z", params.convergenceG.z}}},
        {"convergenceB", {{"x", params.convergenceB.x}, {"y", params.convergenceB.y}, {"z", params.convergenceB.z}}},
        {"phosphorLayout", params.phosphorLayout},
        {"phosphorPitchPx", params.phosphorPitchPx},
        {"phosphorRoundness", params.phosphorRoundness},
        {"phosphorGap", params.phosphorGap},
        {"phosphorGain", params.phosphorGain},
        {"blackMatrix", params.blackMatrix},
        {"phosphorBleed", params.phosphorBleed},
        {"afterglowR", params.afterglowR},
        {"afterglowG", params.afterglowG},
        {"afterglowB", params.afterglowB},
        {"afterglowThreshold", params.afterglowThreshold},
        {"glassDiffusion", params.glassDiffusion},
        {"halation", params.halation},
        {"tubeGlow", params.tubeGlow},
        {"whitePoint", params.whitePoint},
        {"inputGamma", params.inputGamma},
        {"outputGamma", params.outputGamma},
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
        params.curvature = readFloat(j, "curvature", params.curvature);
        params.vignette = readFloat(j, "vignette", params.vignette);
        params.edgeSoftness = readFloat(j, "edgeSoftness", params.edgeSoftness);
        params.glow = readFloat(j, "glow", params.glow);
        params.dotMask = readFloat(j, "dotMask", params.dotMask);
        params.dotBlur = readFloat(j, "dotBlur", params.dotBlur);
        params.bleed = readFloat(j, "bleed", params.bleed);
        params.dotGridSize = readFloat(j, "dotGridSize", params.dotGridSize);
        params.hexGrid = readFloat(j, "hexGrid", params.hexGrid);
        params.alternateLineShift = readFloat(j, "alternateLineShift", params.alternateLineShift);
        params.scanline = readFloat(j, "scanline", params.scanline);
        params.chromaticAberration = readFloat(j, "chromaticAberration", params.chromaticAberration);
        params.brightness = readFloat(j, "brightness", params.brightness);
        params.sharpness = readFloat(j, "sharpness", params.sharpness);
        params.persistence = readFloat(j, "persistence", params.persistence);
        params.persistenceR = readFloat(j, "persistenceR", params.persistence);
        params.persistenceG = readFloat(j, "persistenceG", params.persistence);
        params.persistenceB = readFloat(j, "persistenceB", params.persistence);
        params.ntscArtifacts = readFloat(j, "ntscArtifacts", params.ntscArtifacts);
        params.ntscAlternation = readFloat(j, "ntscAlternation", params.ntscAlternation);
        params.overscan = readFloat(j, "overscan", params.overscan);
        params.pixelRatio = readFloat(j, "pixelRatio", params.pixelRatio);
        params.dimming = readFloat(j, "dimming", params.dimming);
        params.saturation = readFloat(j, "saturation", params.saturation);
        params.maskBrightness = readFloat(j, "maskBrightness", params.maskBrightness);
        params.maskOpacity = readFloat(j, "maskOpacity", params.maskOpacity);
        params.maskScale = readFloat(j, "maskScale", params.maskScale);
        params.bloomIntensity = readFloat(j, "bloomIntensity", params.bloomIntensity);
        const float legacyBloomSpread = readFloat(j, "bloomSpread", params.bloomSpread);
        params.bloomSpread = legacyBloomSpread;
        params.bloomDownsampleSpread = readFloat(j, "bloomDownsampleSpread", legacyBloomSpread);
        params.bloomUpsampleSpread = readFloat(j, "bloomUpsampleSpread", legacyBloomSpread);
        params.bloomPower = readFloat(j, "bloomPower", params.bloomPower);
        params.geometry3D = readFloat(j, "geometry3D", params.geometry3D);
        params.frameEnabled = readFloat(j, "frameEnabled", params.frameEnabled);
        params.reflectionScalar = readFloat(j, "reflectionScalar", params.reflectionScalar);
        params.diffuseBrightness = readFloat(j, "diffuseBrightness", params.diffuseBrightness);
        params.specBrightness = readFloat(j, "specBrightness", params.specBrightness);
        params.specPower = readFloat(j, "specPower", params.specPower);
        params.fresnelBrightness = readFloat(j, "fresnelBrightness", params.fresnelBrightness);
        if (j.contains("lightPos") && j["lightPos"].is_object()) {
            const json& light = j["lightPos"];
            params.lightPos.x = readFloat(light, "x", params.lightPos.x);
            params.lightPos.y = readFloat(light, "y", params.lightPos.y);
            params.lightPos.z = readFloat(light, "z", params.lightPos.z);
        }
        if (j.contains("frameColor") && j["frameColor"].is_object()) {
            const json& color = j["frameColor"];
            params.frameColor.r = static_cast<unsigned char>(std::clamp(readFloat(color, "r", params.frameColor.r), 0.0f, 255.0f));
            params.frameColor.g = static_cast<unsigned char>(std::clamp(readFloat(color, "g", params.frameColor.g), 0.0f, 255.0f));
            params.frameColor.b = static_cast<unsigned char>(std::clamp(readFloat(color, "b", params.frameColor.b), 0.0f, 255.0f));
            params.frameColor.a = static_cast<unsigned char>(std::clamp(readFloat(color, "a", params.frameColor.a), 0.0f, 255.0f));
        }
        params.trueCRT = readFloat(j, "trueCRT", params.trueCRT);
        params.trueCRTDebugView = readFloat(j, "trueCRTDebugView", params.trueCRTDebugView);
        params.signalMode = readFloat(j, "signalMode", params.signalMode);
        params.lumaBandwidth = readFloat(j, "lumaBandwidth", params.lumaBandwidth);
        params.chromaBandwidth = readFloat(j, "chromaBandwidth", params.chromaBandwidth);
        params.chromaDelay = readFloat(j, "chromaDelay", params.chromaDelay);
        params.ntscPhase = readFloat(j, "ntscPhase", params.ntscPhase);
        params.dotCrawl = readFloat(j, "dotCrawl", params.dotCrawl);
        params.beamWidthX = readFloat(j, "beamWidthX", params.beamWidthX);
        params.beamWidthY = readFloat(j, "beamWidthY", params.beamWidthY);
        params.beamFocus = readFloat(j, "beamFocus", params.beamFocus);
        params.beamBloom = readFloat(j, "beamBloom", params.beamBloom);
        params.beamScanlineStrength = readFloat(j, "beamScanlineStrength", params.beamScanlineStrength);
        params.edgeDefocus = readFloat(j, "edgeDefocus", params.edgeDefocus);
        params.convergenceR = readVector3(j, "convergenceR", params.convergenceR);
        params.convergenceG = readVector3(j, "convergenceG", params.convergenceG);
        params.convergenceB = readVector3(j, "convergenceB", params.convergenceB);
        params.phosphorLayout = readFloat(j, "phosphorLayout", params.phosphorLayout);
        params.phosphorPitchPx = readFloat(j, "phosphorPitchPx", params.phosphorPitchPx);
        params.phosphorRoundness = readFloat(j, "phosphorRoundness", params.phosphorRoundness);
        params.phosphorGap = readFloat(j, "phosphorGap", params.phosphorGap);
        params.phosphorGain = readFloat(j, "phosphorGain", params.phosphorGain);
        params.blackMatrix = readFloat(j, "blackMatrix", params.blackMatrix);
        params.phosphorBleed = readFloat(j, "phosphorBleed", params.phosphorBleed);
        params.afterglowR = readFloat(j, "afterglowR", params.afterglowR);
        params.afterglowG = readFloat(j, "afterglowG", params.afterglowG);
        params.afterglowB = readFloat(j, "afterglowB", params.afterglowB);
        params.afterglowThreshold = readFloat(j, "afterglowThreshold", params.afterglowThreshold);
        params.glassDiffusion = readFloat(j, "glassDiffusion", params.glassDiffusion);
        params.halation = readFloat(j, "halation", params.halation);
        params.tubeGlow = readFloat(j, "tubeGlow", params.tubeGlow);
        params.whitePoint = readFloat(j, "whitePoint", params.whitePoint);
        params.inputGamma = readFloat(j, "inputGamma", params.inputGamma);
        params.outputGamma = readFloat(j, "outputGamma", params.outputGamma);
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

static RenderTexture2D loadFilteredRenderTexture(int width, int height, int filter) {
    RenderTexture2D target = LoadRenderTexture(width, height);
    if (target.id) {
        SetTextureFilter(target.texture, filter);
    }
    return target;
}

static RenderTexture2D loadFloatRenderTexture(int width, int height, int filter) {
    RenderTexture2D target = {};
    target.id = rlLoadFramebuffer();
    if (target.id > 0) {
        rlEnableFramebuffer(target.id);
        target.texture.id = rlLoadTexture(nullptr, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
        target.texture.mipmaps = 1;
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;
        target.depth.mipmaps = 1;
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
        if (rlFramebufferComplete(target.id)) {
            rlDisableFramebuffer();
            SetTextureFilter(target.texture, filter);
            return target;
        }
        rlDisableFramebuffer();
        if (target.texture.id) rlUnloadTexture(target.texture.id);
        rlUnloadFramebuffer(target.id);
    }

    TraceLog(LOG_WARNING, "TrueCRT: RGBA16F render texture unavailable, falling back to RGBA8");
    return loadFilteredRenderTexture(width, height, filter);
}

static void clearRenderTexture(RenderTexture2D target) {
    if (!target.id) return;
    BeginTextureMode(target);
        ClearBackground(BLACK);
    EndTextureMode();
}

static Texture2D loadCrtSimTexture(const std::filesystem::path& path, int filter) {
    Texture2D tex = LoadTexture(path.string().c_str());
    if (tex.id) {
        SetTextureFilter(tex, filter);
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    }
    return tex;
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
    unload();
    lastW_ = NATIVE_RES_WIDTH;
    lastH_ = NATIVE_RES_HEIGHT;
    lastScreenW_ = std::max(GetScreenWidth(), 1);
    lastScreenH_ = std::max(GetScreenHeight(), 1);
    sceneRT_ = loadPointRenderTexture(lastW_, lastH_);
    postRT_ = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    prevSceneRT_ = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    ping_[0] = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    ping_[1] = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    crtComposite_[0] = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    crtComposite_[1] = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    crtFullRT_ = loadPointRenderTexture(lastScreenW_, lastScreenH_);
    crtDownsampleRT_ = loadFilteredRenderTexture(std::max(lastScreenW_ / 16, 1), std::max(lastScreenH_ / 16, 1), TEXTURE_FILTER_BILINEAR);
    crtUpsampleRT_ = loadFilteredRenderTexture(lastScreenW_, lastScreenH_, TEXTURE_FILTER_BILINEAR);
    trueSignalRT_ = loadFloatRenderTexture(lastScreenW_, lastScreenH_, TEXTURE_FILTER_BILINEAR);
    trueBeamRT_ = loadFloatRenderTexture(lastScreenW_, lastScreenH_, TEXTURE_FILTER_BILINEAR);
    trueEmissionRT_ = loadFloatRenderTexture(lastScreenW_, lastScreenH_, TEXTURE_FILTER_BILINEAR);
    truePhosphor_[0] = loadFloatRenderTexture(lastScreenW_, lastScreenH_, TEXTURE_FILTER_BILINEAR);
    truePhosphor_[1] = loadFloatRenderTexture(lastScreenW_, lastScreenH_, TEXTURE_FILTER_BILINEAR);
    crtCompositeIndex_ = 0;
    crtEvenFrame_ = true;
    clearRenderTexture(crtComposite_[0]);
    clearRenderTexture(crtComposite_[1]);
    clearRenderTexture(truePhosphor_[0]);
    clearRenderTexture(truePhosphor_[1]);
    const std::filesystem::path crtSimBin = dir_ / "CRTSim" / "CRTSim" / "bin";
    std::filesystem::path maskPath = dir_ / "crtsim_mask.bmp";
    std::filesystem::path artifactsPath = dir_ / "crtsim_artifacts.bmp";
    if (!std::filesystem::exists(maskPath)) maskPath = crtSimBin / "mask.bmp";
    if (!std::filesystem::exists(artifactsPath)) artifactsPath = crtSimBin / "artifacts.bmp";
    crtMaskTexture_ = loadCrtSimTexture(maskPath, TEXTURE_FILTER_BILINEAR);
    crtArtifactsTexture_ = loadCrtSimTexture(artifactsPath, TEXTURE_FILTER_POINT);
    for (auto &pr : collect()) {
        std::string vsPath = pr.second.vs.empty() ? std::string{} : pr.second.vs.string();
        std::string fsPath = pr.second.fs.empty() ? std::string{} : pr.second.fs.string();
        const char* vs = vsPath.empty() ? nullptr : vsPath.c_str();
        const char* fs = fsPath.empty() ? nullptr : fsPath.c_str();
        Shader sh = LoadShader(vs, fs);
        if (sh.id) shaders_[pr.first] = sh;
    }
    std::filesystem::path screenModelPath = dir_ / "crtsim_screen.m3d";
    std::filesystem::path frameModelPath = dir_ / "crtsim_frame.m3d";
    if (!std::filesystem::exists(screenModelPath)) screenModelPath = crtSimBin / "screen.m3d";
    if (!std::filesystem::exists(frameModelPath)) frameModelPath = crtSimBin / "frame.m3d";
    crtScreenModel_ = loadM3DModel(screenModelPath, crtScreenModelLoaded_);
    crtFrameModel_ = loadM3DModel(frameModelPath, crtFrameModelLoaded_);
    snapshot();
}

void Shader_m::unload() {
    if (sceneRT_.id) { UnloadRenderTexture(sceneRT_); sceneRT_.id = 0; }
    if (postRT_.id) { UnloadRenderTexture(postRT_); postRT_.id = 0; }
    if (prevSceneRT_.id) { UnloadRenderTexture(prevSceneRT_); prevSceneRT_.id = 0; }
    for (auto &r : ping_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &r : crtComposite_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    if (crtFullRT_.id) { UnloadRenderTexture(crtFullRT_); crtFullRT_.id = 0; }
    if (crtDownsampleRT_.id) { UnloadRenderTexture(crtDownsampleRT_); crtDownsampleRT_.id = 0; }
    if (crtUpsampleRT_.id) { UnloadRenderTexture(crtUpsampleRT_); crtUpsampleRT_.id = 0; }
    if (trueSignalRT_.id) { UnloadRenderTexture(trueSignalRT_); trueSignalRT_.id = 0; }
    if (trueBeamRT_.id) { UnloadRenderTexture(trueBeamRT_); trueBeamRT_.id = 0; }
    if (trueEmissionRT_.id) { UnloadRenderTexture(trueEmissionRT_); trueEmissionRT_.id = 0; }
    for (auto &r : truePhosphor_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    if (crtMaskTexture_.id) { UnloadTexture(crtMaskTexture_); crtMaskTexture_.id = 0; }
    if (crtArtifactsTexture_.id) { UnloadTexture(crtArtifactsTexture_); crtArtifactsTexture_.id = 0; }
    if (crtScreenModelLoaded_) { UnloadModel(crtScreenModel_); crtScreenModel_ = {}; crtScreenModelLoaded_ = false; }
    if (crtFrameModelLoaded_) { UnloadModel(crtFrameModel_); crtFrameModel_ = {}; crtFrameModelLoaded_ = false; }
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

    if (w != lastW_ || h != lastH_) {
        if (sceneRT_.id) UnloadRenderTexture(sceneRT_);
        sceneRT_ = loadPointRenderTexture(w,h);
        lastW_ = w; lastH_ = h;
    }

    if (sw == lastScreenW_ && sh == lastScreenH_ && postRT_.id && prevSceneRT_.id && ping_[0].id && ping_[1].id &&
        crtComposite_[0].id && crtComposite_[1].id && crtFullRT_.id && crtDownsampleRT_.id && crtUpsampleRT_.id &&
        trueSignalRT_.id && trueBeamRT_.id && trueEmissionRT_.id && truePhosphor_[0].id && truePhosphor_[1].id) return;

    if (postRT_.id) UnloadRenderTexture(postRT_);
    if (prevSceneRT_.id) UnloadRenderTexture(prevSceneRT_);
    for (auto &r: ping_) if (r.id) UnloadRenderTexture(r);
    for (auto &r: crtComposite_) if (r.id) UnloadRenderTexture(r);
    if (crtFullRT_.id) UnloadRenderTexture(crtFullRT_);
    if (crtDownsampleRT_.id) UnloadRenderTexture(crtDownsampleRT_);
    if (crtUpsampleRT_.id) UnloadRenderTexture(crtUpsampleRT_);
    if (trueSignalRT_.id) UnloadRenderTexture(trueSignalRT_);
    if (trueBeamRT_.id) UnloadRenderTexture(trueBeamRT_);
    if (trueEmissionRT_.id) UnloadRenderTexture(trueEmissionRT_);
    for (auto &r: truePhosphor_) if (r.id) UnloadRenderTexture(r);
    postRT_ = loadPointRenderTexture(sw,sh);
    prevSceneRT_ = loadPointRenderTexture(sw,sh);
    ping_[0] = loadPointRenderTexture(sw,sh);
    ping_[1] = loadPointRenderTexture(sw,sh);
    crtComposite_[0] = loadPointRenderTexture(sw,sh);
    crtComposite_[1] = loadPointRenderTexture(sw,sh);
    crtFullRT_ = loadPointRenderTexture(sw,sh);
    crtDownsampleRT_ = loadFilteredRenderTexture(std::max(sw / 16, 1), std::max(sh / 16, 1), TEXTURE_FILTER_BILINEAR);
    crtUpsampleRT_ = loadFilteredRenderTexture(sw,sh, TEXTURE_FILTER_BILINEAR);
    trueSignalRT_ = loadFloatRenderTexture(sw, sh, TEXTURE_FILTER_BILINEAR);
    trueBeamRT_ = loadFloatRenderTexture(sw, sh, TEXTURE_FILTER_BILINEAR);
    trueEmissionRT_ = loadFloatRenderTexture(sw, sh, TEXTURE_FILTER_BILINEAR);
    truePhosphor_[0] = loadFloatRenderTexture(sw, sh, TEXTURE_FILTER_BILINEAR);
    truePhosphor_[1] = loadFloatRenderTexture(sw, sh, TEXTURE_FILTER_BILINEAR);
    crtCompositeIndex_ = 0;
    crtEvenFrame_ = true;
    clearRenderTexture(crtComposite_[0]);
    clearRenderTexture(crtComposite_[1]);
    clearRenderTexture(truePhosphor_[0]);
    clearRenderTexture(truePhosphor_[1]);
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

void Shader_m::swapPing() { pingIndex_ ^= 1; }

static Rectangle getLetterboxRect(int srcW, int srcH, int targetW, int targetH);

void Shader_m::uploadPassUniforms(Shader shader, const std::string& name, Texture2D source) {
    float time = (float)GetTime();
    float resolution[2] = { (float)source.width, (float)source.height };
    Rectangle display = getLetterboxRect(NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT, source.width, source.height);
    float nativeResolution[2] = { (float)NATIVE_RES_WIDTH, (float)NATIVE_RES_HEIGHT };
    float displayRect[4] = { display.x, display.y, display.width, display.height };

    int loc = GetShaderLocation(shader, "time");
    if (loc >= 0) SetShaderValue(shader, loc, &time, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "resolution");
    if (loc >= 0) SetShaderValue(shader, loc, resolution, SHADER_UNIFORM_VEC2);
    loc = GetShaderLocation(shader, "nativeResolution");
    if (loc >= 0) SetShaderValue(shader, loc, nativeResolution, SHADER_UNIFORM_VEC2);
    loc = GetShaderLocation(shader, "displayRect");
    if (loc >= 0) SetShaderValue(shader, loc, displayRect, SHADER_UNIFORM_VEC4);

    if (name != "crt" && name.rfind("crtsim_", 0) != 0 && name.rfind("truecrt_", 0) != 0) return;

    CRTParams& params = crtParams();
    loc = GetShaderLocation(shader, "curvature");
    if (loc >= 0) SetShaderValue(shader, loc, &params.curvature, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "vignette");
    if (loc >= 0) SetShaderValue(shader, loc, &params.vignette, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "edgeSoftness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.edgeSoftness, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "glow");
    if (loc >= 0) SetShaderValue(shader, loc, &params.glow, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "dotMask");
    if (loc >= 0) SetShaderValue(shader, loc, &params.dotMask, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "dotBlur");
    if (loc >= 0) SetShaderValue(shader, loc, &params.dotBlur, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "bleed");
    if (loc >= 0) SetShaderValue(shader, loc, &params.bleed, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "dotGridSize");
    if (loc >= 0) SetShaderValue(shader, loc, &params.dotGridSize, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "hexGrid");
    if (loc >= 0) SetShaderValue(shader, loc, &params.hexGrid, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "alternateLineShift");
    if (loc >= 0) SetShaderValue(shader, loc, &params.alternateLineShift, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "scanline");
    if (loc >= 0) SetShaderValue(shader, loc, &params.scanline, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "chromaticAberration");
    if (loc >= 0) SetShaderValue(shader, loc, &params.chromaticAberration, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "brightness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.brightness, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "sharpness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.sharpness, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "persistence");
    if (loc >= 0) SetShaderValue(shader, loc, &params.persistence, SHADER_UNIFORM_FLOAT);
    float persistenceRgb[3] = {
        params.persistenceR,
        params.persistenceG,
        params.persistenceB,
    };
    loc = GetShaderLocation(shader, "persistenceRgb");
    if (loc >= 0) SetShaderValue(shader, loc, persistenceRgb, SHADER_UNIFORM_VEC3);
    loc = GetShaderLocation(shader, "ntscArtifacts");
    if (loc >= 0) SetShaderValue(shader, loc, &params.ntscArtifacts, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "ntscAlternation");
    if (loc >= 0) SetShaderValue(shader, loc, &params.ntscAlternation, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "overscan");
    if (loc >= 0) SetShaderValue(shader, loc, &params.overscan, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "pixelRatio");
    if (loc >= 0) SetShaderValue(shader, loc, &params.pixelRatio, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "dimming");
    if (loc >= 0) SetShaderValue(shader, loc, &params.dimming, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "saturation");
    if (loc >= 0) SetShaderValue(shader, loc, &params.saturation, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "maskBrightness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.maskBrightness, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "maskOpacity");
    if (loc >= 0) SetShaderValue(shader, loc, &params.maskOpacity, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "maskScale");
    if (loc >= 0) SetShaderValue(shader, loc, &params.maskScale, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "bloomIntensity");
    if (loc >= 0) SetShaderValue(shader, loc, &params.bloomIntensity, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "bloomDownsampleSpread");
    if (loc >= 0) SetShaderValue(shader, loc, &params.bloomDownsampleSpread, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "bloomUpsampleSpread");
    if (loc >= 0) SetShaderValue(shader, loc, &params.bloomUpsampleSpread, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "bloomSpread");
    if (loc >= 0) SetShaderValue(shader, loc, &params.bloomSpread, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "bloomPower");
    if (loc >= 0) SetShaderValue(shader, loc, &params.bloomPower, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "geometry3D");
    if (loc >= 0) SetShaderValue(shader, loc, &params.geometry3D, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "frameEnabled");
    if (loc >= 0) SetShaderValue(shader, loc, &params.frameEnabled, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "reflectionScalar");
    if (loc >= 0) SetShaderValue(shader, loc, &params.reflectionScalar, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "diffuseBrightness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.diffuseBrightness, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "specBrightness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.specBrightness, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "specPower");
    if (loc >= 0) SetShaderValue(shader, loc, &params.specPower, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "fresnelBrightness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.fresnelBrightness, SHADER_UNIFORM_FLOAT);
    float lightPos[3] = {params.lightPos.x, params.lightPos.y, params.lightPos.z};
    loc = GetShaderLocation(shader, "lightPos");
    if (loc >= 0) SetShaderValue(shader, loc, lightPos, SHADER_UNIFORM_VEC3);
    float frameColor[4] = {
        params.frameColor.r / 255.0f,
        params.frameColor.g / 255.0f,
        params.frameColor.b / 255.0f,
        params.frameColor.a / 255.0f,
    };
    loc = GetShaderLocation(shader, "frameColor");
    if (loc >= 0) SetShaderValue(shader, loc, frameColor, SHADER_UNIFORM_VEC4);
    loc = GetShaderLocation(shader, "trueCRT");
    if (loc >= 0) SetShaderValue(shader, loc, &params.trueCRT, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "trueCRTDebugView");
    if (loc >= 0) SetShaderValue(shader, loc, &params.trueCRTDebugView, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "signalMode");
    if (loc >= 0) SetShaderValue(shader, loc, &params.signalMode, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "lumaBandwidth");
    if (loc >= 0) SetShaderValue(shader, loc, &params.lumaBandwidth, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "chromaBandwidth");
    if (loc >= 0) SetShaderValue(shader, loc, &params.chromaBandwidth, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "chromaDelay");
    if (loc >= 0) SetShaderValue(shader, loc, &params.chromaDelay, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "ntscPhase");
    if (loc >= 0) SetShaderValue(shader, loc, &params.ntscPhase, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "dotCrawl");
    if (loc >= 0) SetShaderValue(shader, loc, &params.dotCrawl, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "beamWidthX");
    if (loc >= 0) SetShaderValue(shader, loc, &params.beamWidthX, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "beamWidthY");
    if (loc >= 0) SetShaderValue(shader, loc, &params.beamWidthY, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "beamFocus");
    if (loc >= 0) SetShaderValue(shader, loc, &params.beamFocus, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "beamBloom");
    if (loc >= 0) SetShaderValue(shader, loc, &params.beamBloom, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "beamScanlineStrength");
    if (loc >= 0) SetShaderValue(shader, loc, &params.beamScanlineStrength, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "edgeDefocus");
    if (loc >= 0) SetShaderValue(shader, loc, &params.edgeDefocus, SHADER_UNIFORM_FLOAT);
    float convergenceR[3] = {params.convergenceR.x, params.convergenceR.y, params.convergenceR.z};
    float convergenceG[3] = {params.convergenceG.x, params.convergenceG.y, params.convergenceG.z};
    float convergenceB[3] = {params.convergenceB.x, params.convergenceB.y, params.convergenceB.z};
    loc = GetShaderLocation(shader, "convergenceR");
    if (loc >= 0) SetShaderValue(shader, loc, convergenceR, SHADER_UNIFORM_VEC3);
    loc = GetShaderLocation(shader, "convergenceG");
    if (loc >= 0) SetShaderValue(shader, loc, convergenceG, SHADER_UNIFORM_VEC3);
    loc = GetShaderLocation(shader, "convergenceB");
    if (loc >= 0) SetShaderValue(shader, loc, convergenceB, SHADER_UNIFORM_VEC3);
    loc = GetShaderLocation(shader, "phosphorLayout");
    if (loc >= 0) SetShaderValue(shader, loc, &params.phosphorLayout, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "phosphorPitchPx");
    if (loc >= 0) SetShaderValue(shader, loc, &params.phosphorPitchPx, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "phosphorRoundness");
    if (loc >= 0) SetShaderValue(shader, loc, &params.phosphorRoundness, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "phosphorGap");
    if (loc >= 0) SetShaderValue(shader, loc, &params.phosphorGap, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "phosphorGain");
    if (loc >= 0) SetShaderValue(shader, loc, &params.phosphorGain, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "blackMatrix");
    if (loc >= 0) SetShaderValue(shader, loc, &params.blackMatrix, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "phosphorBleed");
    if (loc >= 0) SetShaderValue(shader, loc, &params.phosphorBleed, SHADER_UNIFORM_FLOAT);
    float afterglowRgb[3] = {params.afterglowR, params.afterglowG, params.afterglowB};
    loc = GetShaderLocation(shader, "afterglowRgb");
    if (loc >= 0) SetShaderValue(shader, loc, afterglowRgb, SHADER_UNIFORM_VEC3);
    loc = GetShaderLocation(shader, "afterglowThreshold");
    if (loc >= 0) SetShaderValue(shader, loc, &params.afterglowThreshold, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "glassDiffusion");
    if (loc >= 0) SetShaderValue(shader, loc, &params.glassDiffusion, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "halation");
    if (loc >= 0) SetShaderValue(shader, loc, &params.halation, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "tubeGlow");
    if (loc >= 0) SetShaderValue(shader, loc, &params.tubeGlow, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "whitePoint");
    if (loc >= 0) SetShaderValue(shader, loc, &params.whitePoint, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "inputGamma");
    if (loc >= 0) SetShaderValue(shader, loc, &params.inputGamma, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "outputGamma");
    if (loc >= 0) SetShaderValue(shader, loc, &params.outputGamma, SHADER_UNIFORM_FLOAT);
    float deltaTime = GetFrameTime();
    loc = GetShaderLocation(shader, "deltaTime");
    if (loc >= 0) SetShaderValue(shader, loc, &deltaTime, SHADER_UNIFORM_FLOAT);
    if (crtMaskTexture_.id) {
        loc = GetShaderLocation(shader, "shadowMaskTexture");
        if (loc >= 0) SetShaderValueTexture(shader, loc, crtMaskTexture_);
    }
    if (crtArtifactsTexture_.id) {
        loc = GetShaderLocation(shader, "ntscArtifactTexture");
        if (loc >= 0) SetShaderValueTexture(shader, loc, crtArtifactsTexture_);
    }
}

static void drawFullscreenTexture(Texture2D tex) {
    DrawTextureRec(tex, {0,0,(float)tex.width, -(float)tex.height}, {0,0}, WHITE);
}

static void drawTextureToTarget(Texture2D tex, RenderTexture2D target) {
    Rectangle src{0, 0, (float)tex.width, -(float)tex.height};
    Rectangle dst{0, 0, (float)target.texture.width, (float)target.texture.height};
    DrawTexturePro(tex, src, dst, {0, 0}, 0.0f, WHITE);
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

static void drawTextureLetterboxed(Texture2D tex, int targetW, int targetH) {
    Rectangle src{0, 0, (float)tex.width, -(float)tex.height}; // flip vertically
    Rectangle dst = getLetterboxRect(tex.width, tex.height, targetW, targetH);
    DrawTexturePro(tex, src, dst, {0,0}, 0.0f, WHITE);
}

static Rectangle nativeRectToPostScaled(Rectangle r, Texture2D postTex) {
    Rectangle dst = getLetterboxRect(NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT, postTex.width, postTex.height);
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

bool Shader_m::renderCRTSim3D(Texture2D composite) {
    CRTParams& params = crtParams();
    if (params.geometry3D < 0.5f || !crtScreenModelLoaded_ || !has("crtsim_screen3d")) return false;

    constexpr float DEG2RAD_LOCAL = 3.14159265358979323846f / 180.0f;
    constexpr float FOV = 15.0f;
    const float halfFov = FOV * 0.5f * DEG2RAD_LOCAL;
    const float distance = std::cos(halfFov) / std::max(std::sin(halfFov), 0.0001f);
    Camera3D camera = {};
    camera.position = {-distance, 0.0f, 0.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 0.0f, 1.0f};
    camera.fovy = FOV;
    camera.projection = CAMERA_PERSPECTIVE;

    auto configureShader = [&](Shader shader, const char* passName, float sampleOverscan, Vector2 maskScale) {
        uploadPassUniforms(shader, passName, composite);
        int loc = GetShaderLocation(shader, "compFrameTexture");
        if (loc >= 0) SetShaderValueTexture(shader, loc, composite);

        const float ratioViewport = 4.0f / 3.0f;
        const float ratioBuffer = composite.height > 0 ? static_cast<float>(composite.width) / static_cast<float>(composite.height) : ratioViewport;
        float uvScalar[2] = {1.0f, (ratioBuffer / ratioViewport) * std::max(params.pixelRatio, 0.001f)};
        float uvOffset[2] = {0.0f, (1.0f - uvScalar[1]) * 0.5f};
        float crtMaskScale[2] = {
            static_cast<float>(composite.width) * maskScale.x * std::max(params.maskScale, 0.001f),
            static_cast<float>(composite.height) * maskScale.y * std::max(params.maskScale, 0.001f),
        };
        float camPos[3] = {camera.position.x, camera.position.y, camera.position.z};

        loc = GetShaderLocation(shader, "sampleOverscan");
        if (loc >= 0) SetShaderValue(shader, loc, &sampleOverscan, SHADER_UNIFORM_FLOAT);
        loc = GetShaderLocation(shader, "uvScalar");
        if (loc >= 0) SetShaderValue(shader, loc, uvScalar, SHADER_UNIFORM_VEC2);
        loc = GetShaderLocation(shader, "uvOffset");
        if (loc >= 0) SetShaderValue(shader, loc, uvOffset, SHADER_UNIFORM_VEC2);
        loc = GetShaderLocation(shader, "crtMaskScale");
        if (loc >= 0) SetShaderValue(shader, loc, crtMaskScale, SHADER_UNIFORM_VEC2);
        loc = GetShaderLocation(shader, "camPos");
        if (loc >= 0) SetShaderValue(shader, loc, camPos, SHADER_UNIFORM_VEC3);
    };

    BeginTextureMode(crtFullRT_);
        ClearBackground(BLACK);
        BeginMode3D(camera);
            rlDisableBackfaceCulling();
            Shader screenShader = get("crtsim_screen3d");
            configureShader(screenShader, "crtsim_screen3d", 1.0f / std::max(params.overscan, 0.001f), {0.5f, 1.0f});
            for (int i = 0; i < crtScreenModel_.materialCount; ++i) {
                crtScreenModel_.materials[i].shader = screenShader;
            }
            DrawModel(crtScreenModel_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

            if (params.frameEnabled >= 0.5f && crtFrameModelLoaded_ && has("crtsim_frame3d")) {
                Shader frameShader = get("crtsim_frame3d");
                configureShader(frameShader, "crtsim_frame3d", std::max(params.overscan, 0.001f), {0.5f, 0.5f});
                for (int i = 0; i < crtFrameModel_.materialCount; ++i) {
                    crtFrameModel_.materials[i].shader = frameShader;
                }
                DrawModel(crtFrameModel_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
            }
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();

    return true;
}

bool Shader_m::renderTrueCRT3D(Texture2D composite) {
    CRTParams& params = crtParams();
    if (params.geometry3D < 0.5f || !crtScreenModelLoaded_ || !has("truecrt_screen3d")) return false;

    constexpr float DEG2RAD_LOCAL = 3.14159265358979323846f / 180.0f;
    constexpr float FOV = 15.0f;
    const float halfFov = FOV * 0.5f * DEG2RAD_LOCAL;
    const float distance = std::cos(halfFov) / std::max(std::sin(halfFov), 0.0001f);
    Camera3D camera = {};
    camera.position = {-distance, 0.0f, 0.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 0.0f, 1.0f};
    camera.fovy = FOV;
    camera.projection = CAMERA_PERSPECTIVE;

    auto configureShader = [&](Shader shader, const char* passName, float sampleOverscan) {
        uploadPassUniforms(shader, passName, composite);
        int loc = GetShaderLocation(shader, "compFrameTexture");
        if (loc >= 0) SetShaderValueTexture(shader, loc, composite);

        const float ratioViewport = 4.0f / 3.0f;
        const float ratioBuffer = composite.height > 0 ? static_cast<float>(composite.width) / static_cast<float>(composite.height) : ratioViewport;
        float uvScalar[2] = {1.0f, (ratioBuffer / ratioViewport) * std::max(params.pixelRatio, 0.001f)};
        float uvOffset[2] = {0.0f, (1.0f - uvScalar[1]) * 0.5f};
        float camPos[3] = {camera.position.x, camera.position.y, camera.position.z};

        loc = GetShaderLocation(shader, "sampleOverscan");
        if (loc >= 0) SetShaderValue(shader, loc, &sampleOverscan, SHADER_UNIFORM_FLOAT);
        loc = GetShaderLocation(shader, "uvScalar");
        if (loc >= 0) SetShaderValue(shader, loc, uvScalar, SHADER_UNIFORM_VEC2);
        loc = GetShaderLocation(shader, "uvOffset");
        if (loc >= 0) SetShaderValue(shader, loc, uvOffset, SHADER_UNIFORM_VEC2);
        loc = GetShaderLocation(shader, "camPos");
        if (loc >= 0) SetShaderValue(shader, loc, camPos, SHADER_UNIFORM_VEC3);
    };

    BeginTextureMode(crtFullRT_);
        ClearBackground(BLACK);
        BeginMode3D(camera);
            rlDisableBackfaceCulling();
            Shader screenShader = get("truecrt_screen3d");
            configureShader(screenShader, "truecrt_screen3d", 1.0f / std::max(params.overscan, 0.001f));
            for (int i = 0; i < crtScreenModel_.materialCount; ++i) {
                crtScreenModel_.materials[i].shader = screenShader;
            }
            DrawModel(crtScreenModel_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

            if (params.frameEnabled >= 0.5f && crtFrameModelLoaded_ && has("truecrt_frame3d")) {
                Shader frameShader = get("truecrt_frame3d");
                configureShader(frameShader, "truecrt_frame3d", std::max(params.overscan, 0.001f));
                for (int i = 0; i < crtFrameModel_.materialCount; ++i) {
                    crtFrameModel_.materials[i].shader = frameShader;
                }
                DrawModel(crtFrameModel_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
            }
            rlEnableBackfaceCulling();
        EndMode3D();
    EndTextureMode();

    return true;
}

Texture2D Shader_m::applyTrueCRT(Texture2D base) {
    if (!has("truecrt_signal") || !has("truecrt_beam") || !has("truecrt_phosphor") ||
        !has("truecrt_afterglow") || !has("truecrt_optics") || !has("crtsim_downsample") ||
        !has("crtsim_upsample") || !has("truecrt_present") || !trueSignalRT_.id ||
        !trueBeamRT_.id || !trueEmissionRT_.id || !truePhosphor_[0].id || !truePhosphor_[1].id ||
        !crtFullRT_.id || !crtDownsampleRT_.id || !crtUpsampleRT_.id) {
        return base;
    }

    {
        Shader sh = get("truecrt_signal");
        BeginTextureMode(trueSignalRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "truecrt_signal", base);
                drawTextureToTarget(base, trueSignalRT_);
            EndShaderMode();
        EndTextureMode();
    }

    {
        Shader sh = get("truecrt_beam");
        BeginTextureMode(trueBeamRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "truecrt_beam", trueSignalRT_.texture);
                drawTextureToTarget(trueSignalRT_.texture, trueBeamRT_);
            EndShaderMode();
        EndTextureMode();
    }

    {
        Shader sh = get("truecrt_phosphor");
        BeginTextureMode(trueEmissionRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "truecrt_phosphor", trueBeamRT_.texture);
                drawTextureToTarget(trueBeamRT_.texture, trueEmissionRT_);
            EndShaderMode();
        EndTextureMode();
    }

    const int sourcePhosphor = crtCompositeIndex_;
    const int targetPhosphor = crtCompositeIndex_ ^ 1;
    {
        Shader sh = get("truecrt_afterglow");
        BeginTextureMode(truePhosphor_[targetPhosphor]);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "truecrt_afterglow", trueEmissionRT_.texture);
                int loc = GetShaderLocation(sh, "prevPhosphorTexture");
                if (loc >= 0) SetShaderValueTexture(sh, loc, truePhosphor_[sourcePhosphor].texture);
                drawTextureToTarget(trueEmissionRT_.texture, truePhosphor_[targetPhosphor]);
            EndShaderMode();
        EndTextureMode();
    }

    Texture2D debugTexture = {};
    const int debugView = static_cast<int>(std::round(crtParams().trueCRTDebugView));
    if (debugView == 1) debugTexture = trueSignalRT_.texture;
    else if (debugView == 2) debugTexture = trueBeamRT_.texture;
    else if (debugView == 3) debugTexture = trueEmissionRT_.texture;
    else if (debugView == 4) debugTexture = truePhosphor_[targetPhosphor].texture;

    if (debugTexture.id) {
        RenderTexture2D& dst = ping_[pingIndex_ ^ 1];
        BeginTextureMode(dst);
            ClearBackground(BLACK);
            drawTextureToTarget(debugTexture, dst);
        EndTextureMode();
        crtCompositeIndex_ = targetPhosphor;
        swapPing();
        return dst.texture;
    }

    if (!renderTrueCRT3D(truePhosphor_[targetPhosphor].texture)) {
        Shader sh = get("truecrt_optics");
        BeginTextureMode(crtFullRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "truecrt_optics", truePhosphor_[targetPhosphor].texture);
                drawTextureToTarget(truePhosphor_[targetPhosphor].texture, crtFullRT_);
            EndShaderMode();
        EndTextureMode();
    }

    {
        Shader sh = get("crtsim_downsample");
        BeginTextureMode(crtDownsampleRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "crtsim_downsample", crtFullRT_.texture);
                drawTextureToTarget(crtFullRT_.texture, crtDownsampleRT_);
            EndShaderMode();
        EndTextureMode();
    }

    {
        Shader sh = get("crtsim_upsample");
        BeginTextureMode(crtUpsampleRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "crtsim_upsample", crtDownsampleRT_.texture);
                drawTextureToTarget(crtDownsampleRT_.texture, crtUpsampleRT_);
            EndShaderMode();
        EndTextureMode();
    }

    RenderTexture2D& dst = ping_[pingIndex_ ^ 1];
    {
        Shader sh = get("truecrt_present");
        BeginTextureMode(dst);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "truecrt_present", crtFullRT_.texture);
                int loc = GetShaderLocation(sh, "upsampleTexture");
                if (loc >= 0) SetShaderValueTexture(sh, loc, crtUpsampleRT_.texture);
                drawTextureToTarget(crtFullRT_.texture, dst);
            EndShaderMode();
        EndTextureMode();
    }

    crtCompositeIndex_ = targetPhosphor;
    crtEvenFrame_ = !crtEvenFrame_;
    swapPing();
    return dst.texture;
}

Texture2D Shader_m::applyCRTSim(Texture2D base) {
    if (!has("crtsim_composite") || !has("crtsim_screen") || !has("crtsim_downsample") ||
        !has("crtsim_upsample") || !has("crtsim_present") || !crtComposite_[0].id ||
        !crtComposite_[1].id || !crtFullRT_.id || !crtDownsampleRT_.id || !crtUpsampleRT_.id) {
        return base;
    }

    const int sourceComposite = crtCompositeIndex_;
    const int targetComposite = crtCompositeIndex_ ^ 1;

    {
        Shader sh = get("crtsim_composite");
        BeginTextureMode(crtComposite_[targetComposite]);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "crtsim_composite", base);
                int loc = GetShaderLocation(sh, "prevCompositeTexture");
                if (loc >= 0) SetShaderValueTexture(sh, loc, crtComposite_[sourceComposite].texture);
                float ntscLerp = crtEvenFrame_ ? 0.0f : std::clamp(crtParams().ntscAlternation, 0.0f, 1.0f);
                loc = GetShaderLocation(sh, "ntscLerp");
                if (loc >= 0) SetShaderValue(sh, loc, &ntscLerp, SHADER_UNIFORM_FLOAT);
                drawTextureToTarget(base, crtComposite_[targetComposite]);
            EndShaderMode();
        EndTextureMode();
    }

    if (!renderCRTSim3D(crtComposite_[targetComposite].texture)) {
        Shader sh = get("crtsim_screen");
        BeginTextureMode(crtFullRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "crtsim_screen", crtComposite_[targetComposite].texture);
                drawTextureToTarget(crtComposite_[targetComposite].texture, crtFullRT_);
            EndShaderMode();
        EndTextureMode();
    }

    {
        Shader sh = get("crtsim_downsample");
        BeginTextureMode(crtDownsampleRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "crtsim_downsample", crtFullRT_.texture);
                drawTextureToTarget(crtFullRT_.texture, crtDownsampleRT_);
            EndShaderMode();
        EndTextureMode();
    }

    {
        Shader sh = get("crtsim_upsample");
        BeginTextureMode(crtUpsampleRT_);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "crtsim_upsample", crtDownsampleRT_.texture);
                drawTextureToTarget(crtDownsampleRT_.texture, crtUpsampleRT_);
            EndShaderMode();
        EndTextureMode();
    }

    RenderTexture2D& dst = ping_[pingIndex_ ^ 1];
    {
        Shader sh = get("crtsim_present");
        BeginTextureMode(dst);
            ClearBackground(BLACK);
            BeginShaderMode(sh);
                uploadPassUniforms(sh, "crtsim_present", crtFullRT_.texture);
                int loc = GetShaderLocation(sh, "upsampleTexture");
                if (loc >= 0) SetShaderValueTexture(sh, loc, crtUpsampleRT_.texture);
                drawTextureToTarget(crtFullRT_.texture, dst);
            EndShaderMode();
        EndTextureMode();
    }

    crtCompositeIndex_ = targetComposite;
    crtEvenFrame_ = !crtEvenFrame_;
    swapPing();
    return dst.texture;
}

Texture2D Shader_m::applyQueue(Texture2D base) {
    Texture2D current = base;                      // Start with post-scaled scene
    for (auto &pass : queue_) {                    // Iterate each queued pass sequentially
        if (pass.type == Pass::Fullscreen && pass.shader == "crt") {
            if (crtParams().trueCRT >= 0.5f && has("truecrt_signal") && has("truecrt_beam") &&
                has("truecrt_phosphor") && has("truecrt_afterglow") && has("truecrt_optics") &&
                has("truecrt_present") && has("crtsim_downsample") && has("crtsim_upsample")) {
                current = applyTrueCRT(current);
                continue;
            }
            if (has("crtsim_composite") && has("crtsim_screen") && has("crtsim_downsample") &&
                has("crtsim_upsample") && has("crtsim_present")) {
                current = applyCRTSim(current);
                continue;
            }
        }
        RenderTexture2D &dst = ping_[pingIndex_^1]; // Select the destination RT (the "next" ping target)
        BeginTextureMode(dst);                     // Begin drawing into destination
            switch(pass.type) {
                case Pass::Fullscreen: {
                    // Activate shader for entire screen
                    if (has(pass.shader)) {
                        Shader sh = get(pass.shader);
                        BeginShaderMode(sh);
                        uploadPassUniforms(sh, pass.shader, current);
                        if (prevSceneRT_.id) {
                            int loc = GetShaderLocation(sh, "prevTexture");
                            if (loc >= 0) SetShaderValueTexture(sh, loc, prevSceneRT_.texture);
                        }
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
                        if (prevSceneRT_.id) {
                            int loc = GetShaderLocation(sh, "prevTexture");
                            if (loc >= 0) SetShaderValueTexture(sh, loc, prevSceneRT_.texture);
                        }
                        Rectangle r = nativeRectToPostScaled(pass.rect, current);
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
                        Rectangle sr = nativeRectToPostScaled({tl.x, tl.y, br.x - tl.x, br.y - tl.y}, current);
                        if (sr.width > 0 && sr.height > 0 && clampToBounds(sr, current.width, current.height)) {
                            Shader sh = get(pass.shader);
                            BeginShaderMode(sh);
                            uploadPassUniforms(sh, pass.shader, current);
                            if (prevSceneRT_.id) {
                                int loc = GetShaderLocation(sh, "prevTexture");
                                if (loc >= 0) SetShaderValueTexture(sh, loc, prevSceneRT_.texture);
                            }
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
        EndTextureMode();                          // Finish rendering to destination RT
        current = dst.texture;                     // Promote destination to become new source for next pass
        swapPing();                                // Flip ping index for next iteration
    }
    return current; // Final texture after all passes
}

void Shader_m::present() {
    ensureTargets();

    BeginTextureMode(postRT_);
        ClearBackground(BLACK);
        drawTextureLetterboxed(sceneRT_.texture, postRT_.texture.width, postRT_.texture.height);
    EndTextureMode();

    Texture2D outTex = queue_.empty()? postRT_.texture : applyQueue(postRT_.texture);
    // Post-process output is already in final window resolution.
    drawFullscreenTexture(outTex);
    // After presenting, keep copy as prev for persistence
    if (prevSceneRT_.id) {
        BeginTextureMode(prevSceneRT_);
            // Copy 1:1 into prev texture (same post-scaled resolution)
            drawFullscreenTexture(outTex);
        EndTextureMode();
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
