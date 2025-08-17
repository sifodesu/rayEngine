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
    bool ended_{false};
    size_t shown_{0};
    // Pagination state
    size_t pageStart_{0};      // Index of first char in current page
    size_t pageEnd_{0};        // Exclusive end index of current page

    float speedCps_{60.0f}; // characters per second
    float charAccumulator_{0.0f}; // Accumulator for fractional character advancement
    float defaultSpeedCps_{60.0f}; // default speed for reset
    float waitTimer_{0.0f};        // time to wait before revealing next char (seconds)

    // Compute the end index (exclusive) of the current page starting at startIdx,
    // based on current camera/box metrics and wrapping rules. Guarantees progress (>= startIdx+1)
    size_t computePageEnd(size_t startIdx) const;
};
