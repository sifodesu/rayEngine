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

enum class GravityDirection {
    DOWN,   // Normal gravity (positive Y)
    UP,     // Reversed gravity (negative Y)
    LEFT,   // Gravity to the left (negative X)
    RIGHT   // Gravity to the right (positive X)
};

class RigidBody : public CollisionRect {
public:
    RigidBody(const CollisionDesc& col, const BodyDesc& body, GObject* father);

    void setSpeed(Vector2 speed);
    void setCurve(double curve);
    void setAcceleration(double acc);
    Vector2 getSpeed();
    void routine();
    const std::vector<CollisionRect*>& getSweepContacts() const { return sweepContacts_; }
    // Gravity acceleration accessor (named mass_ internally)
    double getMass() const { return mass_; }
    void setMass(double m) { mass_ = m; }

    // Gravity control
    bool isGravityEnabled() const { return gravityEnabled_; }
    void setGravityEnabled(bool enabled) { gravityEnabled_ = enabled; }
    GravityDirection getGravityDirection() const { return gravityDirection_; }
    void setGravityDirection(GravityDirection dir) { gravityDirection_ = dir; }

private:
    Vector2 speed_;
    double acceleration_;
    double curve_;
    double mass_;

    bool gravityEnabled_;
    GravityDirection gravityDirection_;
    std::vector<CollisionRect*> sweepContacts_;

    void fixSpeed();    // set to 0 if collision ahead
    void addSweepContacts(const Rectangle& probeRect);
    void registerSweepContact(CollisionRect* body);
};
