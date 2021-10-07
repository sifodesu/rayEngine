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
        Bullet::~Bullet();
        if (body_) {
            delete body_;
        }
    }
    void routine() {
        auto vec = body_->getCollisions();
        for (auto body : vec) {
            // HObject* father = (HObject*)(body->father_);
            if (body->isSolid()) {// && targets_.contains(father)) {
                ttl_ = 0;
                // father->changeHP(-dmg_);
            }
        }
        Bullet::routine();
        body_->routine();
        pos_ = body_->getCoord();
    }

    RigidBody* body_;
private:
};