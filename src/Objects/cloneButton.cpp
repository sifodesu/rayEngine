#include "cloneButton.h"

#include "adiComponent.h"

#include <algorithm>

CloneButton::CloneButton(const SpawnData& data)
    : BasicEnt(data) {
    if (body_) body_->setSolid(false);

    AdiComponentDesc desc = data.interaction.adiComponent.value_or(AdiComponentDesc{});
    desc.canReceiveAdi = true;
    desc.maxCapacity = std::max(desc.maxCapacity, 1);
    desc.activationThreshold = std::max(desc.activationThreshold, 1);
    adiComponent_ = new AdiComponent(desc, data.ldtk.iid.value_or(""));
}

CloneButton::~CloneButton() {
    delete adiComponent_;
}

void CloneButton::routine() {
    BasicEnt::routine();
    if (adiComponent_) adiComponent_->routine();
}

void CloneButton::draw() {
    if (!body_) return;

    Rectangle rect = body_->getSurface();
    bool active = adiComponent_ && adiComponent_->getActivated();
    DrawRectangleRec(rect, active ? Color{255, 232, 96, 255} : Color{124, 92, 255, 255});
    DrawRectangleLinesEx(rect, 1.0f, active ? Color{255, 255, 208, 255} : Color{70, 44, 150, 255});
}

std::optional<AdiComponent*> CloneButton::getAdiComponent() {
    return adiComponent_ ? std::optional<AdiComponent*>{adiComponent_} : std::nullopt;
}
