#include "receptacle.h"
#include "character.h"
#include "raylib.h"
#include "adiComponent.h"

#include <algorithm>

Receptacle::Receptacle(const SpawnData& data) : BasicEnt(data) {
    // Créer et configurer l'AdiComponent depuis SpawnData
    if (data.interaction.adiComponent.has_value()) {
        const auto& desc = *data.interaction.adiComponent;
        if (desc.canReceiveAdi || desc.canBeTriggered || !desc.targetIds.empty()) {
            // Passer l'ID LDtk au constructeur pour l'auto-enregistrement
            std::string ldtkId = data.ldtk.iid.value_or("");
            adiComponent_ = new AdiComponent(desc, ldtkId);
        }
    }
    
    // S'assurer qu'un Receptacle peut toujours recevoir des ADI
    if (adiComponent_) {
        adiComponent_->canReceiveAdi = true;
    }
}

Receptacle::~Receptacle() {
    delete adiComponent_;
}

void Receptacle::draw() {
    BasicEnt::draw();
    if (body_ && adiComponent_) {
        Vector2 pos = body_->getCoord();
        int stored = adiComponent_->getStoredAdi();
        DrawText(TextFormat("R:%d", stored), (int)pos.x, (int)pos.y - 10, 8, stored > 0 ? YELLOW : GRAY);
    }
}
