#include "shader_m.h"
#include <algorithm>
#include <unordered_map>

std::vector<Shader> Shader_m::shaders;
std::vector<std::string> Shader_m::names;
int Shader_m::currentIndex = -1;
std::filesystem::path Shader_m::shaderDir;
RenderTexture2D Shader_m::sceneTarget{};
int Shader_m::lastW = 0;
int Shader_m::lastH = 0;

std::vector<std::pair<std::string, Shader_m::Pair>> Shader_m::collectPairs() {
    std::vector<std::pair<std::string, Pair>> result;
    std::error_code ec;
    if (!std::filesystem::exists(shaderDir, ec)) return result;
    struct Accum { std::filesystem::path vs; std::filesystem::path fs; };
    std::unordered_map<std::string, Accum> map;
    for (auto& p : std::filesystem::directory_iterator(shaderDir, ec)) {
        if (!p.is_regular_file()) continue;
        auto ext = p.path().extension().string();
        for (auto &c : ext) c = (char)tolower(c);
        std::string stem = p.path().stem().string();
        if (ext == ".vs") {
            map[stem].vs = p.path();
        } else if (ext == ".fs") {
            map[stem].fs = p.path();
        }
    }
    for (auto &kv : map) {
        if (!kv.second.fs.empty() || !kv.second.vs.empty()) {
            result.push_back({kv.first, {kv.second.vs, kv.second.fs}});
        }
    }
    std::sort(result.begin(), result.end(), [](auto &a, auto &b){ return a.first < b.first; });
    return result;
}

void Shader_m::load(const std::filesystem::path& dir) {
    shaderDir = dir;
    unload();
    // (Re)create render target
    lastW = GetScreenWidth();
    lastH = GetScreenHeight();
    sceneTarget = LoadRenderTexture(lastW, lastH);
    auto pairs = collectPairs();
    for (auto &pr : pairs) {
        const char* vspath = pr.second.vs.empty() ? nullptr : pr.second.vs.string().c_str();
        const char* fspath = pr.second.fs.string().c_str();
        Shader sh = LoadShader(vspath, fspath);
        if (sh.id != 0) {
            shaders.push_back(sh);
            names.push_back(pr.first);
        }
    }
    currentIndex = shaders.empty() ? -1 : 0;
}

void Shader_m::unload() {
    if (sceneTarget.id) { UnloadRenderTexture(sceneTarget); sceneTarget.id = 0; }
    for (auto &s : shaders) if (s.id) UnloadShader(s);
    shaders.clear();
    names.clear();
    currentIndex = -1;
}

void Shader_m::reload() { load(shaderDir); }

void Shader_m::next() { if (!shaders.empty()) currentIndex = (currentIndex + 1) % (int)shaders.size(); }
void Shader_m::prev() { if (!shaders.empty()) currentIndex = (currentIndex - 1 + (int)shaders.size()) % (int)shaders.size(); }
void Shader_m::disable() { currentIndex = -1; }

bool Shader_m::active() { return currentIndex >= 0 && currentIndex < (int)shaders.size(); }
Shader Shader_m::current() { return active() ? shaders[currentIndex] : Shader{0}; }
const std::string& Shader_m::currentName() { static std::string none = "(disabled)"; return active() ? names[currentIndex] : none; }

void Shader_m::beginScenePass() {
    // Recreate target on resize
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (w != lastW || h != lastH) {
        if (sceneTarget.id) UnloadRenderTexture(sceneTarget);
        sceneTarget = LoadRenderTexture(w, h);
        lastW = w; lastH = h;
    }
    BeginTextureMode(sceneTarget);
}

void Shader_m::endScenePass() {
    EndTextureMode();
}

void Shader_m::present() {
    // Deprecated: use blit() inside existing BeginDrawing to avoid flicker
    blit();
}

void Shader_m::handleInput() {
    if (IsKeyPressed(KEY_RIGHT)) next();
    else if (IsKeyPressed(KEY_LEFT)) prev();
    else if (IsKeyPressed(KEY_F1)) disable();
    else if (IsKeyPressed(KEY_F5)) reload();
}

void Shader_m::routine() {
    handleInput();
    // Resize check (defer expensive reload if only size changed; recreate RT only)
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (w != lastW || h != lastH) {
        if (sceneTarget.id) UnloadRenderTexture(sceneTarget);
        sceneTarget = LoadRenderTexture(w, h);
        lastW = w; lastH = h;
    }
}

void Shader_m::blit() {
    bool use = active();
    if (use) BeginShaderMode(current());
    DrawTextureRec(sceneTarget.texture, {0,0,(float)sceneTarget.texture.width, -(float)sceneTarget.texture.height}, {0,0}, WHITE);
    if (use) EndShaderMode();
}
