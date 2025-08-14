#include "sprite_m.h"
#include "definitions.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

bool Sprite_m::loaded_ = false;
std::unordered_map<std::string, SpriteDesc> Sprite_m::meta_;

void Sprite_m::load() {
    if (loaded_) return;
    loaded_ = true;
    try {
        std::string path = std::string(TEXTURES_PATH) + "sprites_meta.json";
        std::ifstream f(path.c_str());
        if (!f) return; // silently ignore if missing
        json root; f >> root;
        if (!root.is_object()) return;
        for (auto it = root.begin(); it != root.end(); ++it) {
            const std::string key = it.key();
            const json& v = it.value();
            if (!v.is_object()) continue;
            SpriteDesc d;
            if (v.contains("filename")) d.filename = v["filename"].get<std::string>();
            if (v.contains("nb_frames")) d.nb_frames = v["nb_frames"].get<int>();
            if (v.contains("frame_padding")) d.frame_padding = v["frame_padding"].get<int>();
            if (v.contains("animation_speed")) d.animation_speed = v["animation_speed"].get<float>();
            if (v.contains("flipX")) d.flipX = v["flipX"].get<bool>();
            if (v.contains("flipY")) d.flipY = v["flipY"].get<bool>();
            if (v.contains("source") && v["source"].is_object()) {
                auto& s = v["source"];
                d.source = Rectangle{ s.value("x", 0.0f), s.value("y", 0.0f), s.value("w", d.source.width), s.value("h", d.source.height) };
            }
            if (v.contains("tint")) {
                const json& t = v["tint"];
                if (t.is_array() && t.size() >= 3) {
                    int r = t[0].get<int>();
                    int g = t[1].get<int>();
                    int b = t[2].get<int>();
                    int a = (t.size() >= 4) ? t[3].get<int>() : 255;
                    d.tint = Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
                }
            }
            if (!d.filename.empty()) meta_[key] = d;
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
