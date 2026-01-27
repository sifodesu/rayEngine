#pragma once
#include "basicEnt.h"
#include "killComponent.h"

class Projectile : public BasicEnt {
public:
    explicit Projectile(const SpawnData& data);
    ~Projectile() override;

    void routine() override;
    // BasicEnt handles draw via sprite

private:
    KillComponent* killComponent_;
    float lifetime_ = 5.0f; // Seconds before self-destruct
    float age_ = 0.0f;
};
