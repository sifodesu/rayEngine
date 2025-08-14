#pragma once
#include <optional>
#include <string>
#include <raylib.h>

struct SpriteDesc {
    std::string filename{"inv.png"};    
    Rectangle source{0, 0, 20, 20};
    Color tint{WHITE};
    int nb_frames{1};
    int frame_padding{0};
    float animation_speed{5.0f}; // Frames per second
    bool flipX{false};
    bool flipY{false};
};

struct CollisionDesc {
    Rectangle rect{0, 0, 0, 0};
    bool solid{false};
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
    std::optional<std::string> dialog; // Optional dialog text (from LDtk field "Dialog")
};


