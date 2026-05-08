#include "cloneSpawner.h"

#include "adiComponent.h"
#include "character.h"
#include "playerClone.h"
#include "raycam_m.h"

#include <algorithm>

std::unordered_map<int, CloneSpawner*> CloneSpawner::cloneOwners_;

namespace {

Character* currentPlayer() {
    RigidBody* playerBody = Raycam_m::getTarget();
    if (!playerBody) return nullptr;
    return dynamic_cast<Character*>(playerBody->getFather());
}

} // namespace

CloneSpawner::CloneSpawner(const SpawnData& data)
    : BasicEnt(data) {
    if (body_) body_->setSolid(false);

    AdiComponentDesc desc = data.interaction.adiComponent.value_or(AdiComponentDesc{});
    desc.canBeTriggered = true;
    adiComponent_ = new AdiComponent(desc, data.ldtk.iid.value_or(""));
    adiComponent_->onTriggeredBy = [this](const std::string& sourceId, bool triggered) {
        handleTriggered(sourceId, triggered);
    };
}

CloneSpawner::~CloneSpawner() {
    unregisterActiveClone();
    delete adiComponent_;
}

void CloneSpawner::routine() {
    BasicEnt::routine();
    if (adiComponent_) adiComponent_->routine();
}

void CloneSpawner::draw() {
    if (!body_) return;

    Rectangle rect = body_->getSurface();
    DrawRectangleLinesEx(rect, 1.0f, ready_ ? Color{255, 96, 220, 255} : Color{112, 72, 112, 255});
    Rectangle core{rect.x + 2.0f, rect.y + 2.0f, std::max(rect.width - 4.0f, 1.0f), std::max(rect.height - 4.0f, 1.0f)};
    DrawRectangleRec(core, ready_ ? Color{182, 92, 255, 180} : Color{70, 44, 90, 140});
}

std::optional<AdiComponent*> CloneSpawner::getAdiComponent() {
    return adiComponent_ ? std::optional<AdiComponent*>{adiComponent_} : std::nullopt;
}

void CloneSpawner::handleTriggered(const std::string& sourceId, bool triggered) {
    if (triggered) {
        activeSourceId_ = sourceId;
        spawnClone();
        return;
    }

    if (sourceId.empty() || sourceId == activeSourceId_) {
        unregisterActiveClone();
        activeSourceId_.clear();
        ready_ = true;
    }
}

void CloneSpawner::spawnClone() {
    if (!ready_ || !body_) return;
    if (auto* clone = PlayerClone::spawnFromPlayerAt(body_->getSurface(), layer_)) {
        ready_ = false;
        registerActiveClone(clone);
    }
}

void CloneSpawner::registerActiveClone(PlayerClone* clone) {
    if (!clone) return;

    unregisterActiveClone();
    activeCloneId_ = clone->id_;
    cloneOwners_[activeCloneId_] = this;
    clone->setOnDestroyed([](PlayerClone& destroyedClone) {
        CloneSpawner::notifyCloneDestroyed(destroyedClone.id_);
    });
}

void CloneSpawner::unregisterActiveClone() {
    if (activeCloneId_ == 0) return;

    auto it = cloneOwners_.find(activeCloneId_);
    if (it != cloneOwners_.end() && it->second == this) {
        cloneOwners_.erase(it);
    }
    activeCloneId_ = 0;
}

void CloneSpawner::notifyCloneDestroyed(int cloneId) {
    auto it = cloneOwners_.find(cloneId);
    if (it == cloneOwners_.end()) return;

    CloneSpawner* spawner = it->second;
    cloneOwners_.erase(it);
    if (spawner) {
        spawner->resetAfterCloneDestroyed(cloneId);
    }
}

void CloneSpawner::resetAfterCloneDestroyed(int cloneId) {
    if (activeCloneId_ != cloneId) return;

    activeCloneId_ = 0;
    ready_ = true;

    std::string sourceId = activeSourceId_;
    activeSourceId_.clear();
    if (sourceId.empty()) return;

    if (AdiComponent* source = AdiComponent::findRegistered(sourceId)) {
        source->clearStoredAdi(currentPlayer());
    }
}
