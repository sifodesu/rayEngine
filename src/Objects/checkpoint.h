#pragma once
#include "basicEnt.h"

class Checkpoint : public BasicEnt {
public:
    explicit Checkpoint(const SpawnData& data);
    ~Checkpoint();
    void onCollision(GObject* other) override;
};
