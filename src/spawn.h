#pragma once
#include <optional>
#include <string>
#include <raylib.h>
#include <vector>

enum class EntityType {
    Tile,
    Basic,
    Character,
    Portal,
    Plateforme,
    Checkpoint,
    Kill,
    Projectile,
    Pano,
    Receptacle,
    UpgradePickup
};

// Helper function to convert string to EntityType
inline EntityType stringToEntityType(const std::string& typeStr) {
    if (typeStr == "Character")
        return EntityType::Character;
    if (typeStr == "tile") return EntityType::Tile;
    if (typeStr == "basic") return EntityType::Basic;
    if (typeStr == "Character") return EntityType::Character;
    if (typeStr == "Portal") return EntityType::Portal;
    if (typeStr == "Plateforme") return EntityType::Plateforme;
    if (typeStr == "Checkpoint") return EntityType::Checkpoint;
    if (typeStr == "Kill") return EntityType::Kill;
    if (typeStr == "Projectile") return EntityType::Projectile;
    if (typeStr == "Pano") return EntityType::Pano;
    if (typeStr == "Receptacle") return EntityType::Receptacle;
    if (typeStr.rfind("upgrade_", 0) == 0) return EntityType::UpgradePickup;
    return EntityType::Basic; // default fallback
}

struct SpriteDesc {
    std::string filename{"inv.png"};
    Color tint{WHITE};
    bool flipX{false};
    bool flipY{false};
    std::vector<Rectangle> frameRects;
    std::vector<float> frameDurations;
    float defaultFrameDuration{0.2f};
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

struct AdiComponentDesc {
    bool canReceiveAdi = false;
    bool canBeTriggered = false;
    int maxCapacity = 1;
    int activationThreshold = 1;
    std::vector<std::string> targetIds;
};

enum class PortalDirection {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

struct PhysicsConfig {
    std::optional<CollisionDesc> collision;
    std::optional<BodyDesc> body;
};

struct InteractionConfig {
    std::optional<AdiComponentDesc> adiComponent;
    std::optional<std::string> dialog;
    std::optional<std::vector<Vector2>> pathPoints;
    std::optional<bool> enabled;
    std::optional<PortalDirection> direction;
    std::optional<bool> forceGravity; // Portal forces gravity direction on player
    std::optional<bool> isKill;
    std::optional<bool> isLoop;
    std::optional<std::string> behavior;
};

struct LdtkMetadata {
    std::optional<std::string> iid;
    std::optional<std::string> linkId;
    std::optional<std::vector<std::string>> targetIds;
};

struct SpawnData {
    int id{0};
    EntityType entityType{EntityType::Basic};
    std::string typeDetail; // e.g., "upgrade_speed", "upgrade_jump" for subtypes
    int layer{0};
    std::optional<SpriteDesc> sprite;
    PhysicsConfig physics;
    InteractionConfig interaction;
    LdtkMetadata ldtk;
};
