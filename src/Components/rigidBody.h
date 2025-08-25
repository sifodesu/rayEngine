#pragma once
#include <vector>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <map>

#include "gObject.h"
#include "Quadtree.h"
#include "clock.h"
#include "collisionRect.h"
#include "spawn.h"

class RigidBody : public CollisionRect {
public:
    RigidBody(const CollisionDesc& col, const BodyDesc& body, GObject* father);

    void setSpeed(Vector2 speed);
    void setCurve(double curve);
    void setAcceleration(double acc);
    Vector2 getSpeed();
    void routine();
    // Gravity acceleration accessor (named mass_ internally)
    double getMass() const { return mass_; }
    void setMass(double m) { mass_ = m; }

private:
    Vector2 speed_;
    double acceleration_;
    double curve_;
    double mass_;

    void fixSpeed();    // set to 0 if collision ahead
};