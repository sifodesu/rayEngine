#include "receptacle.h"
#include "character.h"
#include "raylib.h"
#include "adiComponent.h"

#include <algorithm>

Receptacle::Receptacle(const SpawnData& data) : BasicEnt(data) {
    // Créer et configurer l'AdiComponent depuis SpawnData
    if (data.adiComponent.has_value()) {
        const auto& desc = *data.adiComponent;
        if (desc.canReceiveAdi || desc.canBeTriggered || !desc.targetIds.empty()) {
            adiComponent_ = new AdiComponent(desc);
        }
    }
    
    // S'assurer qu'un Receptacle peut toujours recevoir des ADI
    if (adiComponent_) {
        adiComponent_->canReceiveAdi = true;
    }
}

void Receptacle::draw() {
    BasicEnt::draw();
    if (body_ && adiComponent_) {
        Vector2 pos = body_->getCoord();
        int stored = adiComponent_->getStoredAdi();
        DrawText(TextFormat("R:%d", stored), (int)pos.x, (int)pos.y - 10, 8, stored > 0 ? YELLOW : GRAY);
    }
}
