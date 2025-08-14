#include "kill.h"
#include "character.h"

void Kill::onCollision(GObject* other) {
    if (auto* chr = dynamic_cast<Character*>(other)) {
        chr->respawn();
    }
}
