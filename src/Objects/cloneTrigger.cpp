#include "cloneTrigger.h"

#include "character.h"
#include "object_m.h"
#include "playerClone.h"
#include "raycam_m.h"

namespace {

bool isPlayerObject(GObject* obj) {
    RigidBody* playerBody = Raycam_m::getTarget();
    return obj && playerBody && playerBody->getFather() == obj;
}

} // namespace

CloneTrigger::CloneTrigger(const SpawnData& data)
    : BasicEnt(data) {
    if (body_) body_->setSolid(false);
}

void CloneTrigger::onCollision(GObject* other) {
    if (triggered_ || !body_ || !isPlayerObject(other)) return;

    auto* player = dynamic_cast<Character*>(other);
    if (!player || !player->getCollisionBody()) return;

    CollisionRect* playerBody = player->getCollisionBody();
    Rectangle triggerRect = body_->getSurface();
    Rectangle playerRect = playerBody->getSurface();
    Rectangle cloneRect{
        triggerRect.x + triggerRect.width * 0.5f - playerRect.width * 0.5f,
        triggerRect.y + triggerRect.height * 0.5f - playerRect.height * 0.5f,
        playerRect.width,
        playerRect.height
    };

    SpawnData cloneData;
    cloneData.id = Object_m::genID();
    cloneData.entityType = EntityType::PlayerClone;
    cloneData.typeDetail = "PlayerClone";
    cloneData.setAsCameraTarget = false;
    cloneData.layer = layer_;
    cloneData.sprite = SpriteDesc{};
    cloneData.sprite->glitched = true;
    cloneData.physics.collision = CollisionDesc{cloneRect, true};

    if (auto* playerRigid = dynamic_cast<RigidBody*>(playerBody)) {
        BodyDesc body;
        body.speed = playerRigid->getSpeed();
        body.gravityAcceleration = playerRigid->getMass();
        cloneData.physics.body = body;
    }

    GObject* spawned = Object_m::createFromSpawn(cloneData);
    if (auto* clone = dynamic_cast<PlayerClone*>(spawned)) {
        if (auto* playerRigid = dynamic_cast<RigidBody*>(playerBody)) {
            clone->body_->setGravityDirection(playerRigid->getGravityDirection());
            clone->body_->setMaxFallSpeedEnabled(playerRigid->isMaxFallSpeedEnabled());
            clone->body_->setMaxFallSpeed(playerRigid->getMaxFallSpeed());
        }
    }

    triggered_ = true;
    to_delete_ = true;
}
