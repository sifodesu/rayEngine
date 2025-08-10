#pragma once
#include <optional>
#include <string>
#include <raylib.h>

struct SpriteDesc {
    std::string filename;
    Rectangle source{0, 0, 32, 32};
    Color tint{WHITE};
};

struct CollisionDesc {
    Rectangle rect{0, 0, 0, 0};
    bool solid{true};
    bool isStatic{true};
};

struct BodyDesc {
    Vector2 speed{0, 0};
    double acceleration{0};
    double curve{0};
    double gravityAcceleration{800};
};

struct SpawnData {
    int id{0};
    std::string type; // e.g. "tile", "character"
    int layer{0};
    std::optional<SpriteDesc> sprite;
    std::optional<CollisionDesc> collision;
    std::optional<BodyDesc> body;
};


