#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <unordered_set>

#include "bullet.h"
#include "rigidBody.h"

class RigidBullet : public Bullet {
public:
    RigidBullet(nlohmann::json obj) : Bullet(obj, false) {
        surface_ = new RigidBody(obj, this);
    }
    ~RigidBullet() {
        if (surface_) {
            delete surface_;
        }
    }
    void routine() {
        surface_->routine();
        Bullet::routine();
    }
    RigidBullet& operator=(const RigidBullet& other) {
        Bullet::operator=(other);
        *((RigidBody*)surface_) = *((RigidBody*)other.surface_);
        return *this;
    }
    void setSpeed(Vector2 speed) {
        ((RigidBody*)surface_)->setSpeed(speed);
    }
    Vector2 getSpeed() {
        return ((RigidBody*)surface_)->getSpeed();
    }
    void setCoord(Vector2 coord) {
        Bullet::setCoord(coord);
        ((RigidBody*)surface_)->setCoord(coord);
    }
    Vector2 getCoord() {
        return ((RigidBody*)surface_)->getCoord();
    }
    void setCurve(double curve) {
        ((RigidBody*)surface_)->setCurve(curve);
    }
    void setAcceleration(double acc) {
        ((RigidBody*)surface_)->setAcceleration(acc);
    }
};