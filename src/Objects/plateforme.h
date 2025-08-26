#pragma once
#include "basicEnt.h"
#include <vector>
#include <raylib.h> // ensure Vector2

class Plateforme : public BasicEnt {
public:
    explicit Plateforme(const SpawnData& data);
    void routine() override;
private:
    std::vector<Vector2> waypoints_; // center waypoints
    int current_ = 0;
    int dir_ = 1; // direction through waypoints
    float speed_ = 40.0f; // pixels per second
    float waitTime_ = 0.4f; // seconds to wait at endpoints
    float waiting_ = 0.0f;
    Vector2 lastCenter_{0,0}; // previous center for stable delta
}; // Plateforme
