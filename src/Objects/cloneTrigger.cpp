#include "cloneTrigger.h"

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

    if (PlayerClone::spawnFromPlayerAt(body_->getSurface(), layer_)) {
        triggered_ = true;
        to_delete_ = true;
    }
}
