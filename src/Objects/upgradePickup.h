#pragma once
#include "basicEnt.h"
#include <string>

// Pickup that upgrades the player when touched, then self-destructs.
// Type string mapped to effect via UpgradeRegistry.
class UpgradePickup : public BasicEnt {
public:
    explicit UpgradePickup(const SpawnData& data);
    void routine() override; // overlap check with Character
private:
    std::string upgradeType_;
};
