#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <unordered_set>

#include "bullet.h"
#include "rigidBody.h"

class RigidBullet : public Bullet {
public:
    RigidBullet(nlohmann::json obj) : Bullet(obj) {
        body_ = new RigidBody(obj, this);
    }
    ~RigidBullet() {
        if (body_) {
            delete body_;
        }
        // Bullet::~Bullet();
    }
    void routine() {
        body_->routine();
        pos_ = body_->getCoord();
        Bullet::routine();
    }
    RigidBullet& operator=(const RigidBullet& other) {
        Bullet::operator=(other);
        *body_ = *other.body_;
        setCoord(other.body_->getCoord());
        return *this;
    }
    void setSpeed(Vector2 speed) {
        body_->setSpeed(speed);
    }
    void setCoord(Vector2 coord) {
        Bullet::setCoord(coord);
        body_->setCoord(coord);
    }
    void setCurve(double curve){
        body_->setCurve(curve);
    }
    void setAcceleration(double acc){
        body_->setAcceleration(acc);
    }
    RigidBody* body_;
private:
};