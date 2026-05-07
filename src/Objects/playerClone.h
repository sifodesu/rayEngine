#pragma once

#include "character.h"

class PlayerClone : public Character {
public:
    explicit PlayerClone(const SpawnData& data);
    static PlayerClone* spawnFromPlayerAt(Rectangle spawnArea, int layer);
    void routine() override;
    bool isPlayerClone() const override { return true; }
    bool blocksMovementFor(GObject* moving) const override;
    void onCollision(GObject* other) override;

private:
    bool armed_{false};
};
