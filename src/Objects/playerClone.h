#pragma once

#include "character.h"

#include <functional>

class PlayerClone : public Character {
public:
    explicit PlayerClone(const SpawnData& data);
    static PlayerClone* spawnFromPlayerAt(Rectangle spawnArea, int layer);
    void destroyClone();
    void setOnDestroyed(std::function<void(PlayerClone&)> onDestroyed);
    void routine() override;
    bool isPlayerClone() const override { return true; }
    bool blocksMovementFor(GObject* moving) const override;
    void onCollision(GObject* other) override;

private:
    void notifyDestroyed();

    bool armed_{false};
    bool destructionNotified_{false};
    std::function<void(PlayerClone&)> onDestroyed_;
};
