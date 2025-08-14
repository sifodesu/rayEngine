#include "upgradePickup.h"
#include "character.h"
#include "collisionRect.h"
#include <raylib.h>
#include "upgradeRegistry.h"
#include "sound_m.h"
#include "object_m.h"
#include "shader_m.h"
#include "raycam_m.h"

UpgradePickup::UpgradePickup(const SpawnData& data) : BasicEnt(data) {
    upgradeType_ = data.type; // e.g. "upgrade_speed"
    if (body_) body_->setSolid(false); // allow passing through
}

void UpgradePickup::routine() {
    BasicEnt::routine();
    if (to_delete_) return;
    if (!body_) return;
    Rectangle mine = body_->getSurface();
    // Check dynamic bodies only (player is dynamic); with_solid=false includes non-solid
    for (CollisionRect* other : CollisionRect::query(mine, false)) {
        if (other->getFather() == this) continue;
        if (!CheckCollisionRecs(mine, other->getSurface())) continue;
        if (auto* chr = dynamic_cast<Character*>(other->getFather())) {
            if (UpgradeRegistry::apply(upgradeType_, *chr)) {
                to_delete_ = true;
            }
            break;
        }
    }
}
