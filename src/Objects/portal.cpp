#include "portal.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "character.h"
#include "collisionRect.h"
#include "ldtk_m.h"
#include "object_m.h"
#include "portal_m.h"
#include "raycam_m.h"
#include "rigidBody.h"

Portal* Portal::playerSpawnedPortal_ = nullptr;

Portal::Portal(const SpawnData& data)
    : BasicEnt(data),
      linkable_(nullptr),
      isPlayerSpawned_(false) {
    spriteRect_ = body_->getSurface();
    direction_ = data.interaction.direction;
    forceGravity_ = data.interaction.forceGravity.value_or(false);

    if (direction_.has_value()) {
        Rectangle collisionRect = body_->getSurface();
        switch (*direction_) {
            case PortalDirection::UP:
                collisionRect.y -= 1.0f;
                collisionRect.height += 1.0f;
                break;
            case PortalDirection::DOWN:
                collisionRect.height += 1.0f;
                break;
            case PortalDirection::LEFT:
                collisionRect.x -= 1.0f;
                collisionRect.width += 1.0f;
                break;
            case PortalDirection::RIGHT:
                collisionRect.width += 1.0f;
                break;
        }
        body_->setSurface(collisionRect);
    }
    body_->setSolid(false);

    linkable_ = new LinkableComponent(this, std::to_string(id_));
    linkable_->onTriggerReceived =
        [this](const std::string& sourceId, const std::string& message, void* data) {
            onPortalLink(sourceId, message, data);
        };

    targetLdtkId_ = data.ldtk.linkId.value_or("");
    if (data.ldtk.targetIds.has_value()) {
        for (const std::string& targetId : *data.ldtk.targetIds) {
            linkable_->addTargetId(targetId);
        }
    }

    Portal_m::registerPortal(this);
}

Portal::~Portal() {
    Portal_m::unregisterPortal(this);
    delete linkable_;

    if (playerSpawnedPortal_ == this) {
        playerSpawnedPortal_ = nullptr;
    }
}

void Portal::routine() {
    BasicEnt::routine();

    if (overlappingEntities_.empty()) return;

    Rectangle portalRect = body_->getSurface();
    std::vector<int> endedOverlaps;
    for (int entityId : overlappingEntities_) {
        auto it = Object_m::level_ents_.find(entityId);
        if (it == Object_m::level_ents_.end()) {
            endedOverlaps.push_back(entityId);
            continue;
        }

        CollisionRect* entityBody = it->second->getCollisionBody();
        if (!entityBody || !CheckCollisionRecs(portalRect, entityBody->getSurface())) {
            endedOverlaps.push_back(entityId);
        }
    }

    for (int entityId : endedOverlaps) {
        overlappingEntities_.erase(entityId);
    }
}

void Portal::draw() {
    Color portalColor = WHITE;
    if (Portal* linkedPortal = getLinkedPortal()) {
        int colorSeed = std::min(id_, linkedPortal->id_);
        float hue = std::fmod(colorSeed * 137.5f / 360.0f, 1.0f);
        float saturation = 0.8f;
        float brightness = 0.9f;
        float chroma = brightness * saturation;
        float secondary = chroma * (1.0f - std::fabs(std::fmod(hue * 6.0f, 2.0f) - 1.0f));
        float match = brightness - chroma;

        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        if (hue < 1.0f / 6.0f) {
            red = chroma;
            green = secondary;
        } else if (hue < 2.0f / 6.0f) {
            red = secondary;
            green = chroma;
        } else if (hue < 3.0f / 6.0f) {
            green = chroma;
            blue = secondary;
        } else if (hue < 4.0f / 6.0f) {
            green = secondary;
            blue = chroma;
        } else if (hue < 5.0f / 6.0f) {
            red = secondary;
            blue = chroma;
        } else {
            red = chroma;
            blue = secondary;
        }

        portalColor = {
            static_cast<unsigned char>((red + match) * 255.0f),
            static_cast<unsigned char>((green + match) * 255.0f),
            static_cast<unsigned char>((blue + match) * 255.0f),
            255
        };
    }

    sprite_->setTint(portalColor);
    sprite_->draw(spriteRect_);
    sprite_->setTint(WHITE);
}

std::optional<LinkableComponent*> Portal::getLinkableComponent() {
    return linkable_;
}

Portal* Portal::getLinkedPortal() const {
    for (GObject* object : linkable_->getLinkedObjects()) {
        if (auto* portal = dynamic_cast<Portal*>(object)) {
            return portal;
        }
    }
    return nullptr;
}

void Portal::onCollision(GObject* other) {
    if (!other || other == this || dynamic_cast<Portal*>(other)) return;
    if (Object_m::level_ents_.find(other->id_) == Object_m::level_ents_.end()) return;
    if (!other->getCollisionBody()) return;
    if (overlappingEntities_.contains(other->id_)) return;

    Portal* target = getLinkedPortal();
    if (!target) return;

    if (direction_.has_value() && target->direction_.has_value()) {
        Portal_m::beginTransit(this, other);
    } else {
        performTeleportation(other);
    }
}

Vector2 Portal::getCenter() const {
    Rectangle rect = body_->getSurface();
    return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

void Portal::spawnPortalAtPlayer() {
    Character* player = nullptr;
    for (auto& [_, object] : Object_m::level_ents_) {
        if (auto* character = dynamic_cast<Character*>(object.get())) {
            player = character;
            break;
        }
    }
    if (!player) return;

    if (playerSpawnedPortal_) {
        playerSpawnedPortal_->to_delete_ = true;
        playerSpawnedPortal_ = nullptr;
    }

    Rectangle playerRect = player->getRect();
    Vector2 center{
        playerRect.x + playerRect.width * 0.5f,
        playerRect.y + playerRect.height * 0.5f
    };

    SpawnData portalData;
    portalData.id = Object_m::genID();
    portalData.entityType = EntityType::Portal;
    portalData.layer = 1;
    portalData.ldtk.linkId = "player_portal";
    portalData.sprite = SpriteDesc{"gateway.png"};
    portalData.physics.collision = CollisionDesc{
        {center.x - 4.0f, center.y - 4.0f, 8.0f, 8.0f},
        false
    };

    auto* portal = dynamic_cast<Portal*>(Object_m::createFromSpawn(portalData));
    if (!portal) return;

    portal->setIsPlayerSpawned(true);
    playerSpawnedPortal_ = portal;
}

void Portal::teleportPlayerToSpawnedPortal() {
    if (!playerSpawnedPortal_) return;

    Character* player = nullptr;
    for (auto& [_, object] : Object_m::level_ents_) {
        if (auto* character = dynamic_cast<Character*>(object.get())) {
            player = character;
            break;
        }
    }
    if (!player || !player->body_) return;

    Vector2 portalCenter = playerSpawnedPortal_->getCenter();
    Rectangle playerRect = player->body_->getSurface();
    playerRect.x = portalCenter.x - playerRect.width * 0.5f;
    playerRect.y = portalCenter.y - playerRect.height * 0.5f;
    player->body_->setSurface(playerRect);
    playerSpawnedPortal_->overlappingEntities_.insert(player->id_);
}

bool Portal::findSafePosition(
    Rectangle& entityRect,
    CollisionRect* entityBody,
    const Rectangle&,
    PortalDirection direction
) {
    Portal* target = getLinkedPortal();

    auto isFree = [&](Rectangle candidate) {
        for (CollisionRect* collision : CollisionRect::query(candidate, true)) {
            if (!collision || collision == entityBody) continue;
            GObject* owner = collision->getFather();
            if (owner == this || owner == target) continue;
            if (CheckCollisionRecs(candidate, collision->getSurface())) return false;
        }
        return true;
    };

    if (isFree(entityRect)) return true;

    constexpr float pushStep = 2.0f;
    constexpr int maxAttempts = 20;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        Rectangle candidate = entityRect;
        float distance = pushStep * attempt;
        switch (direction) {
            case PortalDirection::UP:
                candidate.y -= distance;
                break;
            case PortalDirection::DOWN:
                candidate.y += distance;
                break;
            case PortalDirection::LEFT:
                candidate.x -= distance;
                break;
            case PortalDirection::RIGHT:
                candidate.x += distance;
                break;
        }
        if (!isFree(candidate)) continue;

        entityRect = candidate;
        return true;
    }

    return false;
}

bool Portal::performTeleportation(GObject* entity) {
    Portal* target = getLinkedPortal();
    CollisionRect* entityBody = entity ? entity->getCollisionBody() : nullptr;
    if (!target || !target->body_ || !entityBody) return false;

    Rectangle entityRect = entityBody->getSurface();
    Rectangle sourceRect = body_->getSurface();
    Rectangle targetRect = target->body_->getSurface();
    float relativeX = std::clamp(
        (entityRect.x + entityRect.width * 0.5f - sourceRect.x) / sourceRect.width,
        0.0f,
        1.0f
    );
    float relativeY = std::clamp(
        (entityRect.y + entityRect.height * 0.5f - sourceRect.y) / sourceRect.height,
        0.0f,
        1.0f
    );

    auto* rigidBody = dynamic_cast<RigidBody*>(entityBody);
    Vector2 speed = rigidBody ? rigidBody->getSpeed() : Vector2{};
    Rectangle destination = entityRect;
    destination.x = targetRect.x + targetRect.width * 0.5f - entityRect.width * 0.5f;
    destination.y = targetRect.y + targetRect.height * 0.5f - entityRect.height * 0.5f;

    if (target->direction_.has_value()) {
        switch (*target->direction_) {
            case PortalDirection::UP:
                destination.x = targetRect.x + relativeX * targetRect.width - entityRect.width * 0.5f;
                destination.y = targetRect.y - entityRect.height;
                speed.y = -std::fabs(speed.y);
                break;
            case PortalDirection::DOWN:
                destination.x = targetRect.x + relativeX * targetRect.width - entityRect.width * 0.5f;
                destination.y = targetRect.y + targetRect.height;
                speed.y = std::fabs(speed.y);
                break;
            case PortalDirection::LEFT:
                destination.x = targetRect.x - entityRect.width;
                destination.y = targetRect.y + relativeY * targetRect.height - entityRect.height * 0.5f;
                speed.x = -std::fabs(speed.x);
                break;
            case PortalDirection::RIGHT:
                destination.x = targetRect.x + targetRect.width;
                destination.y = targetRect.y + relativeY * targetRect.height - entityRect.height * 0.5f;
                speed.x = std::fabs(speed.x);
                break;
        }
    } else {
        speed.y = -speed.y;
    }

    PortalDirection safetyDirection = target->direction_.value_or(PortalDirection::UP);
    if (!findSafePosition(destination, entityBody, targetRect, safetyDirection)) {
        destination.x = targetRect.x + targetRect.width * 0.5f - entityRect.width * 0.5f;
        destination.y = targetRect.y + targetRect.height * 0.5f - entityRect.height * 0.5f;
        if (!findSafePosition(destination, entityBody, targetRect, safetyDirection)) return false;
    }

    entityBody->setSurface(destination);
    if (rigidBody) {
        if (target->forceGravity_ && target->direction_.has_value()) {
            switch (*target->direction_) {
                case PortalDirection::UP:
                    rigidBody->setGravityDirection(GravityDirection::UP);
                    break;
                case PortalDirection::DOWN:
                    rigidBody->setGravityDirection(GravityDirection::DOWN);
                    break;
                case PortalDirection::LEFT:
                    rigidBody->setGravityDirection(GravityDirection::LEFT);
                    break;
                case PortalDirection::RIGHT:
                    rigidBody->setGravityDirection(GravityDirection::RIGHT);
                    break;
            }
        }
        rigidBody->setSpeed(speed);
    }

    overlappingEntities_.insert(entity->id_);
    target->overlappingEntities_.insert(entity->id_);
    return true;
}

void Portal::resolveLinkTarget() {
    if (targetLdtkId_.empty()) return;

    int targetId = Ldtk_m::getEngineIdFromLdtkId(targetLdtkId_);
    if (targetId != -1) {
        linkable_->addTargetId(std::to_string(targetId));
    }
}

void Portal::onPortalLink(const std::string&, const std::string&, void*) {
}
