#include "playerClone.h"

#include "object_m.h"
#include "portal.h"
#include "raycam_m.h"

#include <utility>
#include <vector>

namespace {

SpawnData makeCloneData(SpawnData data) {
    data.setAsCameraTarget = false;
    if (!data.sprite.has_value()) data.sprite = SpriteDesc{};
    data.sprite->glitched = true;
    return data;
}

bool touchesPlayer(CollisionRect* cloneBody, float padding);

bool overlapsPlayer(CollisionRect* cloneBody) {
    return touchesPlayer(cloneBody, 0.0f);
}

Rectangle expanded(Rectangle rect, float amount) {
    rect.x -= amount;
    rect.y -= amount;
    rect.width += amount * 2.0f;
    rect.height += amount * 2.0f;
    return rect;
}

bool touchesPlayer(CollisionRect* cloneBody, float padding) {
    RigidBody* playerBody = Raycam_m::getTarget();
    if (!cloneBody || !playerBody || cloneBody == playerBody) return false;

    GObject* clone = cloneBody->getFather();
    GObject* player = playerBody->getFather();
    std::vector<Rectangle> cloneSurfaces = Portal::getVisibleCollisionSurfaces(clone);
    std::vector<Rectangle> playerSurfaces = Portal::getVisibleCollisionSurfaces(player);
    if (cloneSurfaces.empty()) cloneSurfaces.push_back(cloneBody->getSurface());
    if (playerSurfaces.empty()) playerSurfaces.push_back(playerBody->getSurface());

    for (Rectangle cloneSurface : cloneSurfaces) {
        Rectangle touchRect = expanded(cloneSurface, padding);
        for (Rectangle playerSurface : playerSurfaces) {
            if (CheckCollisionRecs(touchRect, playerSurface)) return true;
        }
    }
    return false;
}

bool touchesPlayer(CollisionRect* cloneBody) {
    return touchesPlayer(cloneBody, 0.5f);
}

bool isPlayerObject(GObject* obj) {
    RigidBody* playerBody = Raycam_m::getTarget();
    return obj && playerBody && playerBody->getFather() == obj;
}

} // namespace

PlayerClone::PlayerClone(const SpawnData& data)
    : Character(makeCloneData(data)) {
    if (body_) body_->setSolid(true);
}

void PlayerClone::destroyClone() {
    if (to_delete_) return;
    to_delete_ = true;
    notifyDestroyed();
}

void PlayerClone::setOnDestroyed(std::function<void(PlayerClone&)> onDestroyed) {
    onDestroyed_ = std::move(onDestroyed);
}

void PlayerClone::notifyDestroyed() {
    if (destructionNotified_) return;
    destructionNotified_ = true;
    if (onDestroyed_) {
        onDestroyed_(*this);
    }
}

PlayerClone* PlayerClone::spawnFromPlayerAt(Rectangle spawnArea, int layer) {
    RigidBody* playerBody = Raycam_m::getTarget();
    if (!playerBody) return nullptr;

    auto* player = dynamic_cast<Character*>(playerBody->getFather());
    if (!player || !player->getCollisionBody()) return nullptr;

    Rectangle playerRect = playerBody->getSurface();
    Rectangle cloneRect{
        spawnArea.x + spawnArea.width * 0.5f - playerRect.width * 0.5f,
        spawnArea.y + spawnArea.height * 0.5f - playerRect.height * 0.5f,
        playerRect.width,
        playerRect.height
    };

    SpawnData cloneData;
    cloneData.id = Object_m::genID();
    cloneData.entityType = EntityType::PlayerClone;
    cloneData.typeDetail = "PlayerClone";
    cloneData.setAsCameraTarget = false;
    cloneData.layer = layer;
    cloneData.sprite = SpriteDesc{};
    cloneData.sprite->glitched = true;
    cloneData.physics.collision = CollisionDesc{cloneRect, true};

    auto* playerRigid = dynamic_cast<RigidBody*>(playerBody);
    if (playerRigid) {
        BodyDesc body;
        body.speed = playerRigid->getSpeed();
        body.gravityAcceleration = playerRigid->getMass();
        cloneData.physics.body = body;
    }

    auto* clone = dynamic_cast<PlayerClone*>(Object_m::createFromSpawn(cloneData));
    if (clone && playerRigid) {
        clone->body_->setGravityDirection(playerRigid->getGravityDirection());
        clone->body_->setMaxFallSpeedEnabled(playerRigid->isMaxFallSpeedEnabled());
        clone->body_->setMaxFallSpeed(playerRigid->getMaxFallSpeed());
    }
    return clone;
}

void PlayerClone::routine() {
    if (to_delete_) return;
    Character::routine();
    if (to_delete_) return;
    if (!armed_) {
        if (!overlapsPlayer(body_)) {
            armed_ = true;
        }
        return;
    }
    if (touchesPlayer(body_)) {
        if (RigidBody* playerBody = Raycam_m::getTarget()) {
            if (auto* player = dynamic_cast<Character*>(playerBody->getFather())) {
                player->respawn();
            }
        }
    }
}

bool PlayerClone::blocksMovementFor(GObject* moving) const {
    return !isPlayerObject(moving);
}

void PlayerClone::onCollision(GObject* other) {
    if (to_delete_) return;
    Character::onCollision(other);

    if (!armed_ || !isPlayerObject(other) || !touchesPlayer(body_)) return;
    if (auto* player = dynamic_cast<Character*>(other)) {
        player->respawn();
    }
}
