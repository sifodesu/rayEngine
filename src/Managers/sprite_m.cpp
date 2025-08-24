#include "sprite_m.h"
#include "definitions.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

bool Sprite_m::loaded_ = false;
std::unordered_map<std::string, SpriteDesc> Sprite_m::meta_;

void Sprite_m::load() {
    if (loaded_) return;
    loaded_ = true;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(TEXTURES_PATH)) {
            if (entry.path().extension() == ".json") {
                std::ifstream f(entry.path());
                if (!f) continue; // silently ignore if missing
                json root; f >> root;
                if (!root.is_object()) continue;
                if (!root.contains("frames") || !root.contains("meta")) continue;

                SpriteDesc d;
                std::string imageName = root["meta"].value("image", std::string("inv.png"));
                d.filename = imageName;
                const json& frames = root["frames"]; // object with each frame key
                if (!frames.is_object()) continue;
                // Collect frame rects and durations
                for (auto it = frames.begin(); it != frames.end(); ++it) {
                    const json& f = it.value();
                    if (!f.is_object()) continue;
                    if (f.contains("frame") && f["frame"].is_object()) {
                        const json& fr = f["frame"];
                        float x = fr.value("x", 0.0f);
                        float y = fr.value("y", 0.0f);
                        float w = fr.value("w", 0.0f);
                        float h = fr.value("h", 0.0f);
                        d.frameRects.push_back(Rectangle{ x, y, w, h });
                    }
                    // Duration in ms per Aseprite spec
                    float durMs = f.value("duration", 100.0f);
                    d.frameDurations.push_back(durMs / 1000.0f);
                }
                if (!d.frameRects.empty() && !d.frameDurations.empty()) {
                    double sum = 0.0;
                    for (float s : d.frameDurations) sum += s;
                    if (sum > 0) d.defaultFrameDuration = (float)(sum / d.frameDurations.size());
                }
                // Key for this sprite: strip extension from imageName (e.g., chara_walk.png -> chara_walk)
                std::string key = imageName;
                auto posDot = key.find_last_of('.');
                if (posDot != std::string::npos) key.erase(posDot);
                if (!d.filename.empty()) meta_[key] = d;
            }
        }
    } catch (...) {
        // ignore malformed files
    }
}

void Sprite_m::unload() {
    meta_.clear();
    loaded_ = false;
}

std::optional<SpriteDesc> Sprite_m::get(const std::string& key) {
    if (!loaded_) load();
    auto it = meta_.find(key);
    if (it == meta_.end()) return std::nullopt;
    return it->second;
}
