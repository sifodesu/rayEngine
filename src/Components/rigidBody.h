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
    bool hadDiscontinuousMovement() const { return discontinuousMovement_; }
    const std::vector<GObject*>& getSweepContactOwners() const { return sweepContactOwners_; }
    // Gravity acceleration accessor (named mass_ internally)
    double getMass() const { return mass_; }
    void setMass(double m) { mass_ = m; }

    // Gravity control
    bool isGravityEnabled() const { return gravityEnabled_; }
    void setGravityEnabled(bool enabled) { gravityEnabled_ = enabled; }
    GravityDirection getGravityDirection() const { return gravityDirection_; }
    void setGravityDirection(GravityDirection dir);
    bool isMaxFallSpeedEnabled() const { return maxFallSpeedEnabled_; }
    void setMaxFallSpeedEnabled(bool enabled);
    double getMaxFallSpeed() const { return maxFallSpeed_; }
    void setMaxFallSpeed(double speed);

private:
    Vector2 speed_;
    double acceleration_;
    double curve_;
    double mass_;

    bool gravityEnabled_;
    GravityDirection gravityDirection_;
    bool maxFallSpeedEnabled_ = true;
    double maxFallSpeed_ = 500.0;
    bool discontinuousMovement_ = false;
    std::vector<GObject*> sweepContactOwners_;

    void fixSpeed();    // set to 0 if collision ahead
    void addSweepContacts(const Rectangle& probeRect);
    void registerSweepContactOwner(GObject* owner);
    double getFallSpeedAlongGravity() const;
    void applyMaxFallSpeed();
};
