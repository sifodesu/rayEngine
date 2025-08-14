#pragma once
#include "basicEnt.h"
#include <string>
#include <raylib.h>

// "Pano" object: when the player collides with it, it opens a dialog box
// that scrolls text on screen.
class Pano : public BasicEnt {
public:
    explicit Pano(const SpawnData& data);
    ~Pano() override;

    void routine() override;
    void draw() override;
    void onCollision(GObject* other) override;

private:
    std::string text_;
    bool active_{false};
    size_t shown_{0};
    float speedCps_{1000.0f}; // characters per second (much higher for testing)
    float charAccumulator_{0.0f}; // Accumulator for fractional character advancement
};
