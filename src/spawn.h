#pragma once
#include <optional>
#include <string>
#include <raylib.h>
#include <vector>

struct SpriteDesc {
    std::string filename{"inv.png"};
    Color tint{WHITE};
    bool flipX{false};
    bool flipY{false};
    // New: explicit frame rectangles (overrides nb_frames/frame_padding when non-empty)
    std::vector<Rectangle> frameRects; // each frame sub-rect
    std::vector<float> frameDurations; // seconds per frame (same size as frameRects) optional
    float defaultFrameDuration{0.2f}; // used if frameDurations empty (uniform timing)
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
