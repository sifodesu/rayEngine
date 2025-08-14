#include "checkpoint.h"
#include "character.h"

Checkpoint::Checkpoint(const SpawnData& data) : BasicEnt(data) {}

Checkpoint::~Checkpoint() = default;

void Checkpoint::onCollision(GObject* other) {
    if (auto* chr = dynamic_cast<Character*>(other)) {
        if (body_) chr->setRespawn(body_->getCoord());
    }
}
