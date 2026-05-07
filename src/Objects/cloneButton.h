#pragma once

#include "basicEnt.h"

class AdiComponent;

class CloneButton : public BasicEnt {
public:
    explicit CloneButton(const SpawnData& data);
    ~CloneButton() override;

    void routine() override;
    void draw() override;
    std::optional<AdiComponent*> getAdiComponent() override;

private:
    AdiComponent* adiComponent_{nullptr};
};
