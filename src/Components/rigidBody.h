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

class RigidBody : public CollisionRect {
public:
    RigidBody(nlohmann::json obj, GObject* father);

    void setSpeed(Vector2 speed);
    void setCurve(double curve);
    void setAcceleration(double acc);
    Vector2 getSpeed();
    bool isSolid();
    void routine();

private:
    Vector2 speed_;
    double acceleration_;
    double curve_;

    void fixSpeed();    //set to 0 if collision
};