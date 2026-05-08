#pragma once

#include "basicEnt.h"

#include <string>
#include <unordered_map>

class AdiComponent;
class PlayerClone;

class CloneSpawner : public BasicEnt {
public:
    explicit CloneSpawner(const SpawnData& data);
    ~CloneSpawner() override;

    void routine() override;
    void draw() override;
    std::optional<AdiComponent*> getAdiComponent() override;

private:
    static void notifyCloneDestroyed(int cloneId);
    static std::unordered_map<int, CloneSpawner*> cloneOwners_;

    void handleTriggered(const std::string& sourceId, bool triggered);
    void spawnClone();
    void registerActiveClone(PlayerClone* clone);
    void unregisterActiveClone();
    void resetAfterCloneDestroyed(int cloneId);

    AdiComponent* adiComponent_{nullptr};
    bool ready_{true};
    int activeCloneId_{0};
    std::string activeSourceId_;
};
