#include "gObject.h"

#include "killComponent.h"

GObject::GObject(const int id) : id_(id), to_delete_(false), layer_(0) {}

GObject::~GObject() {
    delete killOnColComponent_;
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
