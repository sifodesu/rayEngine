#pragma once

#include "basicEnt.h"

class CloneKiller : public BasicEnt {
public:
    explicit CloneKiller(const SpawnData& data);

    void draw() override;
    void onCollision(GObject* other) override;
};
