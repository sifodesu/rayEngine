#include "cloneSpawner.h"

#include "adiComponent.h"
#include "playerClone.h"

#include <algorithm>

CloneSpawner::CloneSpawner(const SpawnData& data)
    : BasicEnt(data) {
    if (body_) body_->setSolid(false);

    AdiComponentDesc desc = data.interaction.adiComponent.value_or(AdiComponentDesc{});
    desc.canBeTriggered = true;
    adiComponent_ = new AdiComponent(desc, data.ldtk.iid.value_or(""));
    adiComponent_->onTriggered = [this](bool triggered) {
        if (triggered) {
            spawnClone();
        } else {
            ready_ = true;
        }
    };
}

CloneSpawner::~CloneSpawner() {
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

void CloneSpawner::spawnClone() {
    if (!ready_ || !body_) return;
    if (PlayerClone::spawnFromPlayerAt(body_->getSurface(), layer_)) {
        ready_ = false;
    }
}
