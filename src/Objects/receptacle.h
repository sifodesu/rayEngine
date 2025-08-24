#pragma once
#include "basicEnt.h"
#include <vector>
class Character;

// Receptacle stores deposited adi charges; when it has at least 1 it is considered active.
class Receptacle : public BasicEnt {
public:
    explicit Receptacle(const SpawnData& data);
    void draw() override;
    int stored() const { return storedAdi_; }
    bool isActive() const { return storedAdi_ > 0; }
    bool deposit(Character& c); // player gives one adi
    static void recallAll(Character& c); // pull all adis from all receptacles back to the player
    static void resetAll(); // clear receptacles
private:
    int storedAdi_ = 0;
    static std::vector<Receptacle*> all_;
};
