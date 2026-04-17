#pragma once

#include <string>

#include "basicEnt.h"

class Shooter : public BasicEnt {
public:
    explicit Shooter(const SpawnData& data);

    void routine() override;

private:
    void spawnProjectile();
    bool initializeDirectionFromPoint(const SpawnData& data);

    float fireInterval_ = 1.0f;
    float projectileSpeed_ = 160.0f;
    std::string projectileSprite_ = "bullet.png";
    int maxRipple_ = 0;
    float fireCooldown_ = 0.0f;
    bool hasDirection_ = false;
    Vector2 fireDirection_{1.0f, 0.0f};
    float projectileSize_ = 8.0f;
};
