#pragma once

#include "basicEnt.h"

class AdiComponent;

class CloneSpawner : public BasicEnt {
public:
    explicit CloneSpawner(const SpawnData& data);
    ~CloneSpawner() override;

    void routine() override;
    void draw() override;
    std::optional<AdiComponent*> getAdiComponent() override;

private:
    void spawnClone();

    AdiComponent* adiComponent_{nullptr};
    bool ready_{true};
};
