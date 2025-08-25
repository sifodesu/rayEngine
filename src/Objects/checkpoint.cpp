#include "checkpoint.h"
#include "character.h"
#include "save_manager.h"

Checkpoint::Checkpoint(const SpawnData& data) : BasicEnt(data) {}

Checkpoint::~Checkpoint() = default;

void Checkpoint::onCollision(GObject* other) {
    if (auto* chr = dynamic_cast<Character*>(other)) {
        if (body_) {
            Vector2 pos = body_->getCoord();
            chr->setRespawn(pos);
            SaveManager::registerCheckpoint(pos);
        }
    }
}
