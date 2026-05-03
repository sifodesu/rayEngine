#pragma once
#include <optional>
#include <string>
#include <raylib.h>
#include <vector>

enum class EntityType {
    Tile,
    Basic,
    Character,
    Model3D,
    Portal,
    Plateforme,
    OneWayPlatform,
    FriablePlatform,
    Water,
    Shooter,
    Checkpoint,
    Kill,
    Projectile,
    Pano,
    Receptacle,
    UpgradePickup
};

// Helper function to convert string to EntityType
inline EntityType stringToEntityType(const std::string& typeStr) {
    if (typeStr == "tile") return EntityType::Tile;
    if (typeStr == "basic") return EntityType::Basic;
    if (typeStr == "Character") return EntityType::Character;
    if (typeStr == "Model3D") return EntityType::Model3D;
    if (typeStr == "Portal") return EntityType::Portal;
    if (typeStr == "Plateforme") return EntityType::Plateforme;
    if (typeStr == "OneWayPlatform") return EntityType::OneWayPlatform;
    if (typeStr == "FriablePlatform") return EntityType::FriablePlatform;
    if (typeStr == "Water" || typeStr == "water") return EntityType::Water;
    if (typeStr == "Waterfall" || typeStr == "waterfall") return EntityType::Water;
    if (typeStr == "Shooter") return EntityType::Shooter;
    if (typeStr == "Checkpoint") return EntityType::Checkpoint;
    if (typeStr == "Kill") return EntityType::Kill;
    if (typeStr == "Projectile") return EntityType::Projectile;
    if (typeStr == "Pano") return EntityType::Pano;
    if (typeStr == "Receptacle") return EntityType::Receptacle;
    if (typeStr.rfind("upgrade_", 0) == 0) return EntityType::UpgradePickup;
    return EntityType::Basic; // default fallback
}

enum class ModelPrimitive {
    Cube,
    Sphere,
    Cylinder,
    Plane
};

inline ModelPrimitive stringToModelPrimitive(const std::string& primitiveStr) {
    if (primitiveStr == "Cube" || primitiveStr == "cube") return ModelPrimitive::Cube;
    if (primitiveStr == "Sphere" || primitiveStr == "sphere") return ModelPrimitive::Sphere;
    if (primitiveStr == "Cylinder" || primitiveStr == "cylinder") return ModelPrimitive::Cylinder;
    if (primitiveStr == "Plane" || primitiveStr == "plane") return ModelPrimitive::Plane;
    return ModelPrimitive::Cube;
}

struct SpriteDesc {
    std::string filename{"inv.png"};
    Color tint{WHITE};
    bool flipX{false};
    bool flipY{false};
    bool glitched{false};
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
    std::optional<bool> killOnCol;
    std::optional<PortalDirection> direction;
    std::optional<bool> forceGravity; // Portal forces gravity direction on player
    std::optional<bool> isLoop;
    std::optional<std::string> behavior;
    std::optional<float> breakTime; // seconds
    std::optional<float> fireInterval; // seconds
    std::optional<float> projectileSpeed; // pixels per second
    std::optional<std::string> projectileSprite; // sprite key/file name
    std::optional<int> maxRipple; // max ricochets on solid surfaces
};

struct LdtkMetadata {
    std::optional<std::string> iid;
    std::optional<std::string> linkId;
    std::optional<std::vector<std::string>> targetIds;
};

struct ModelDesc {
    std::string modelFile{};
    ModelPrimitive primitive{ModelPrimitive::Cube};
    Vector3 rotation{0.0f, 0.0f, 0.0f}; // degrees (XYZ Euler)
    Vector3 scale{1.0f, 1.0f, 1.0f};
    Vector3 spin{0.0f, 0.0f, 0.0f}; // degrees per second
    Color tint{WHITE};
};

struct SpawnData {
    int id{0};
    EntityType entityType{EntityType::Basic};
    std::string typeDetail; // e.g., "upgrade_speed", "upgrade_jump" for subtypes
    std::optional<int> sourceObjectId; // runtime-only owner id (not from LDtk)
    bool isTileInstance{false}; // Spawned from a LDtk tile layer, even if typed as Basic/Kill/etc.
    int layer{0};
    std::optional<SpriteDesc> sprite;
    std::optional<ModelDesc> model;
    PhysicsConfig physics;
    InteractionConfig interaction;
    LdtkMetadata ldtk;
};
