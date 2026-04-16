#pragma once

#include "basicEnt.h"

class Character;

class FriablePlatform : public BasicEnt {
public:
    explicit FriablePlatform(const SpawnData& data);

    void routine() override;
    void onCollision(GObject* other) override;

private:
    bool isCharacterStandingOn(const Character& character) const;
    bool detectStandingCharacter() const;
    void startBreaking();

    bool breaking_ = false;
    float breakTime_ = 1.0f;
    float breakElapsed_ = 0.0f;
    float contactTolerance_ = 1.5f;
};
