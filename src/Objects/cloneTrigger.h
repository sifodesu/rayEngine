#pragma once

#include "basicEnt.h"

class CloneTrigger : public BasicEnt {
public:
    explicit CloneTrigger(const SpawnData& data);
    void onCollision(GObject* other) override;

private:
    bool triggered_{false};
};
