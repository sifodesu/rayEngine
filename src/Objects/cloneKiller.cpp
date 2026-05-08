#include "cloneKiller.h"

#include "playerClone.h"

CloneKiller::CloneKiller(const SpawnData& data)
    : BasicEnt(data) {
    if (body_) body_->setSolid(false);
}

void CloneKiller::draw() {
    if (!body_) return;

    Rectangle rect = body_->getSurface();
    DrawRectangleRec(rect, Color{255, 56, 96, 120});
    DrawRectangleLinesEx(rect, 1.0f, Color{255, 96, 128, 255});
}

void CloneKiller::onCollision(GObject* other) {
    auto* clone = dynamic_cast<PlayerClone*>(other);
    if (!clone) return;

    clone->destroyClone();
}
