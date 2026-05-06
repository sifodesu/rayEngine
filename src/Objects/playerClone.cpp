#include "playerClone.h"

#include "portal.h"
#include "raycam_m.h"

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

void PlayerClone::routine() {
    Character::routine();
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
    Character::onCollision(other);

    if (!armed_ || !isPlayerObject(other) || !touchesPlayer(body_)) return;
    if (auto* player = dynamic_cast<Character*>(other)) {
        player->respawn();
    }
}
