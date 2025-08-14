#pragma once
#include "basicEnt.h"

// Zone qui tue le joueur au contact et le fait respawn
class Kill : public BasicEnt {
public:
    explicit Kill(const SpawnData& data) : BasicEnt(data) {}
    ~Kill() override = default;
    void onCollision(GObject* other) override;
};
