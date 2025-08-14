#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include "spawn.h"

class Sprite_m {
public:
    // Load all sprite descriptors from TEXTURES_PATH/sprites_meta.json
    static void load();
    static void unload();
    static std::optional<SpriteDesc> get(const std::string& key);

private:
    static bool loaded_;
    static std::unordered_map<std::string, SpriteDesc> meta_;
};
