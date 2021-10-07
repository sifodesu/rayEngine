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
        Bullet::~Bullet();
    }
    void routine() {
        body_->routine();
        pos_ = body_->getCoord();
        Bullet::routine();
    }

    RigidBody* body_;
private:
};