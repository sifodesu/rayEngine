#include "shooter.h"

#include <algorithm>
#include <cmath>

#include "clock.h"
#include "object_m.h"
#include "sprite_m.h"

Shooter::Shooter(const SpawnData& data) : BasicEnt(data) {
    fireInterval_ = std::max(data.interaction.fireInterval.value_or(1.0f), 0.01f);
    projectileSpeed_ = std::max(data.interaction.projectileSpeed.value_or(160.0f), 1.0f);
    projectileSprite_ = data.interaction.projectileSprite.value_or("bullet.png");
    if (projectileSprite_.empty()) projectileSprite_ = "bullet.png";

    hasDirection_ = initializeDirectionFromPoint(data);
}

void Shooter::routine() {
    BasicEnt::routine();

    if (!body_ || !hasDirection_) return;

    float dt = static_cast<float>(Clock::getLap());
    fireCooldown_ -= dt;

    while (fireCooldown_ <= 0.0f) {
        spawnProjectile();
        fireCooldown_ += fireInterval_;
    }
}

bool Shooter::initializeDirectionFromPoint(const SpawnData& data) {
    if (!body_ || !data.interaction.pathPoints.has_value() || data.interaction.pathPoints->empty()) {
        return false;
    }

    Vector2 center = body_->getCenterCoord();
    Vector2 target = data.interaction.pathPoints->front();
    Vector2 delta{target.x - center.x, target.y - center.y};
    float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= 0.0001f) return false;

    fireDirection_ = {delta.x / length, delta.y / length};
    return true;
}

void Shooter::spawnProjectile() {
    if (!body_) return;

    Rectangle shooterRect = body_->getSurface();
    Vector2 shooterCenter{shooterRect.x + shooterRect.width * 0.5f,
                          shooterRect.y + shooterRect.height * 0.5f};
    float shooterRadius = 0.5f * std::max(shooterRect.width, shooterRect.height);
    float projectileHalf = projectileSize_ * 0.5f;
    float spawnOffset = shooterRadius + projectileHalf + 1.0f;

    Vector2 projectileCenter{
        shooterCenter.x + fireDirection_.x * spawnOffset,
        shooterCenter.y + fireDirection_.y * spawnOffset
    };

    SpawnData projectileData;
    projectileData.id = Object_m::genID();
    projectileData.entityType = EntityType::Projectile;
    projectileData.layer = layer_;
    projectileData.sourceObjectId = id_;

    SpriteDesc spriteDesc;
    if (auto meta = Sprite_m::get(projectileSprite_)) {
        spriteDesc = *meta;
    } else {
        std::string key = projectileSprite_;
        auto pos = key.find_last_of('.');
        if (pos != std::string::npos) key = key.substr(0, pos);
        if (auto metaBase = Sprite_m::get(key)) {
            spriteDesc = *metaBase;
        } else {
            if (projectileSprite_.find('.') == std::string::npos) {
                spriteDesc.filename = projectileSprite_ + ".png";
            } else {
                spriteDesc.filename = projectileSprite_;
            }
        }
    }
    projectileData.sprite = spriteDesc;

    CollisionDesc collision;
    collision.rect = Rectangle{
        projectileCenter.x - projectileHalf,
        projectileCenter.y - projectileHalf,
        projectileSize_,
        projectileSize_
    };
    collision.solid = false;
    projectileData.physics.collision = collision;

    BodyDesc bodyDesc;
    bodyDesc.speed = Vector2{
        fireDirection_.x * projectileSpeed_,
        fireDirection_.y * projectileSpeed_
    };
    bodyDesc.acceleration = 0.0;
    bodyDesc.curve = 0.0;
    bodyDesc.gravityAcceleration = 0.0;
    projectileData.physics.body = bodyDesc;

    Object_m::createFromSpawn(projectileData);
}
