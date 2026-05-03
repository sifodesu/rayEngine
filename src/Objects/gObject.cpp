#include "gObject.h"

#include "killComponent.h"
#include "lightComponent.h"

GObject::GObject(const int id) : id_(id), to_delete_(false), layer_(0) {}

GObject::~GObject() {
    delete killOnColComponent_;
    delete lightComponent_;
}

void GObject::setKillOnCollision(bool enabled) {
    killOnCol_ = enabled;

    if (killOnCol_) {
        if (!killOnColComponent_) {
            killOnColComponent_ = new KillComponent();
        }
    } else {
        delete killOnColComponent_;
        killOnColComponent_ = nullptr;
    }
}

void GObject::applyKillOnCollision(GObject* other) {
    if (!killOnCol_ || !killOnColComponent_ || !other) return;
    killOnColComponent_->onCollision(other);
}

void GObject::configureLight(const SpawnData& data) {
    delete lightComponent_;
    lightComponent_ = nullptr;

    if (!data.light.has_value()) return;
    const LightDesc& desc = *data.light;
    if (!desc.enabled || desc.radius <= 0.0f || desc.intensity <= 0.0f) return;

    lightComponent_ = new LightComponent(this, desc);
}
