#pragma once
#include "basicEnt.h"

// Zone qui tue le joueur au contact et le fait respawn
class KillComponent;

class Kill : public BasicEnt {
public:
    explicit Kill(const SpawnData& data);
    ~Kill() override;
    void onCollision(GObject* other) override;

private:
    KillComponent* killComponent_;
};
