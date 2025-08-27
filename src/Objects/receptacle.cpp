#include "receptacle.h"
#include "character.h"
#include "raylib.h"
#include "object_m.h"
#include "plateforme.h"
#include "ldtk_m.h"
#include <algorithm>

std::vector<Receptacle*> Receptacle::all_;

Receptacle::Receptacle(const SpawnData& data) : BasicEnt(data) {
    if (body_) body_->setSolid(false);
    targetId_ = data.targetId.value_or("");
    all_.push_back(this);
}

bool Receptacle::deposit(Character& c) {
    if (!c.canDepositAdi()) return false;
    if (!c.depositOneAdi()) return false;
    storedAdi_ += 1;
    
    // Activate referenced object when we receive an ADI
    activateTargetObject(true);
    
    return true;
}

void Receptacle::recallAll(Character& c) {
    for (auto* r : all_) {
        while (r && r->storedAdi_ > 0 && c.retrieveOneAdi()) {
            r->storedAdi_ -= 1;
            r->activateTargetObject(false);
        }
    }
}

void Receptacle::resetAll() {
    for (auto* r : all_) if (r) r->storedAdi_ = 0;
}

void Receptacle::draw() {
    BasicEnt::draw();
    if (body_) {
        Vector2 pos = body_->getCoord();
        DrawText(TextFormat("R:%d", storedAdi_), (int)pos.x, (int)pos.y - 10, 8, isActive() ? YELLOW : GRAY);
    }
}

void Receptacle::activateTargetObject(bool enabled) {
    if (targetId_.empty()) return; // No target specified
    
    // Get engine ID from LDtk ID
    int engineId = Ldtk_m::getEngineIdFromLdtkId(targetId_);
    if (engineId == -1) return; // Target not found
    
    // Search in level entities first
    auto it = Object_m::level_ents_.find(engineId);
    if (it != Object_m::level_ents_.end()) {
        GObject* obj = it->second.get();
        if (obj) {
            // Try to cast to Plateforme and enable it
            if (Plateforme* platform = dynamic_cast<Plateforme*>(obj)) {
                platform->setEnabled(enabled);
            }
            // Add other object types as needed
        }
        return;
    }
}
