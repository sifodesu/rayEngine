#include "receptacle.h"
#include "character.h"
#include "raylib.h"
#include <algorithm>

std::vector<Receptacle*> Receptacle::all_;

Receptacle::Receptacle(const SpawnData& data) : BasicEnt(data) {
    if (body_) body_->setSolid(false);
    all_.push_back(this);
}

bool Receptacle::deposit(Character& c) {
    if (!c.canDepositAdi()) return false;
    if (!c.depositOneAdi()) return false;
    storedAdi_ += 1;
    return true;
}

void Receptacle::recallAll(Character& c) {
    for (auto* r : all_) {
        while (r && r->storedAdi_ > 0 && c.retrieveOneAdi()) {
            r->storedAdi_ -= 1;
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
