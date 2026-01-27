#include "kill.h"
#include "character.h"
#include "killComponent.h"

Kill::Kill(const SpawnData& data) : BasicEnt(data) {
    killComponent_ = new KillComponent();
}

Kill::~Kill() {
    delete killComponent_;
}

void Kill::onCollision(GObject* other) {
    if (killComponent_) {
        killComponent_->onCollision(other);
    }
}
