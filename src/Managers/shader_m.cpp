#include "shader_m.h"
#include "raycam_m.h"
#include <algorithm>

std::unordered_map<std::string, Shader> Shader_m::shaders_;
std::filesystem::path Shader_m::dir_;
RenderTexture2D Shader_m::sceneRT_{};
RenderTexture2D Shader_m::prevSceneRT_{};
RenderTexture2D Shader_m::ping_[2] = {};
int Shader_m::pingIndex_ = 0;
int Shader_m::lastW_ = 0;
int Shader_m::lastH_ = 0;
std::vector<Shader_m::Pass> Shader_m::queue_;
std::unordered_map<std::string, std::filesystem::file_time_type> Shader_m::fileTimes_;

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
    unload();
    lastW_ = GetScreenWidth();
    lastH_ = GetScreenHeight();
    sceneRT_ = LoadRenderTexture(lastW_, lastH_);
    prevSceneRT_ = LoadRenderTexture(lastW_, lastH_);
    ping_[0] = LoadRenderTexture(lastW_, lastH_);
    ping_[1] = LoadRenderTexture(lastW_, lastH_);
    for (auto &pr : collect()) {
        const char* vs = pr.second.vs.empty()? nullptr : pr.second.vs.string().c_str();
        const char* fs = pr.second.fs.string().c_str();
        Shader sh = LoadShader(vs, fs);
        if (sh.id) shaders_[pr.first] = sh;
    }
    snapshot();
}

void Shader_m::unload() {
    if (sceneRT_.id) { UnloadRenderTexture(sceneRT_); sceneRT_.id = 0; }
    if (prevSceneRT_.id) { UnloadRenderTexture(prevSceneRT_); prevSceneRT_.id = 0; }
    for (auto &r : ping_) if (r.id) { UnloadRenderTexture(r); r.id = 0; }
    for (auto &kv : shaders_) if (kv.second.id) UnloadShader(kv.second);
    shaders_.clear();
    queue_.clear();
}

void Shader_m::reload() { load(dir_); }

bool Shader_m::has(const std::string& name) { return shaders_.count(name)!=0; }

Shader Shader_m::get(const std::string& name) { auto it = shaders_.find(name); return it==shaders_.end()? Shader{0} : it->second; }

void Shader_m::ensureTargets() {
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (w==lastW_ && h==lastH_) return;
    if (sceneRT_.id) UnloadRenderTexture(sceneRT_);
    if (prevSceneRT_.id) UnloadRenderTexture(prevSceneRT_);
    for (auto &r: ping_) if (r.id) UnloadRenderTexture(r);
    sceneRT_ = LoadRenderTexture(w,h);
    prevSceneRT_ = LoadRenderTexture(w,h);
    ping_[0] = LoadRenderTexture(w,h);
    ping_[1] = LoadRenderTexture(w,h);
    lastW_ = w; lastH_ = h;
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

static void drawFullscreenTexture(Texture2D tex) {
    DrawTextureRec(tex, {0,0,(float)tex.width, -(float)tex.height}, {0,0}, WHITE);
}

static bool clampToScreen(Rectangle &r) {
    int W = GetScreenWidth();
    int H = GetScreenHeight();
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

Texture2D Shader_m::applyQueue() {
    Texture2D current = sceneRT_.texture;          // Start with captured scene
    for (auto &pass : queue_) {                    // Iterate each queued pass sequentially
        RenderTexture2D &dst = ping_[pingIndex_^1]; // Select the destination RT (the "next" ping target)
        BeginTextureMode(dst);                     // Begin drawing into destination
            switch(pass.type) {
                case Pass::Fullscreen: {
                    // Activate shader for entire screen
                    if (has(pass.shader)) {
                        Shader sh = get(pass.shader);
                        BeginShaderMode(sh);
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
                        if (prevSceneRT_.id) {
                            int loc = GetShaderLocation(sh, "prevTexture");
                            if (loc >= 0) SetShaderValueTexture(sh, loc, prevSceneRT_.texture);
                        }
                        Rectangle r = pass.rect; // already in screen coordinates
                        if (r.width > 0 && r.height > 0 && clampToScreen(r)) {
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
                        Rectangle sr{tl.x, tl.y, br.x - tl.x, br.y - tl.y};
                        if (sr.width > 0 && sr.height > 0 && clampToScreen(sr)) {
                            Shader sh = get(pass.shader);
                            BeginShaderMode(sh);
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
    Texture2D outTex = queue_.empty()? sceneRT_.texture : applyQueue();
    drawFullscreenTexture(outTex);
    // After presenting, keep copy as prev for persistence
    if (prevSceneRT_.id) {
        BeginTextureMode(prevSceneRT_);
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
