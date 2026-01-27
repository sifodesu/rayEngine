#include "killComponent.h"
#include "character.h"

bool KillComponent::onCollision(GObject* other) {
    if (auto* chr = dynamic_cast<Character*>(other)) {
        chr->respawn();
        return true;
    }
    return false;
}
