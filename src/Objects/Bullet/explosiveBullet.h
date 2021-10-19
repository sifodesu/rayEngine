#pragma once
#include "rigidBody.h"
#include "pattern.h"
#include "object_m.h"

class ExplosiveBullet : public RigidBullet {
public:
    ExplosiveBullet(nlohmann::json obj) : RigidBullet(obj), nb_bullets(10) {}
    void routine() {
        RigidBullet::routine();
        if (ttl_ <= 0) {
            RigidBullet* bp = new RigidBullet(Object_m::blueprints_[BULLET]);
            bp->setCurve(0);
            bp->setAcceleration(0);
            bp->no_dmg_ = { no_dmg_ };
            bp->setSpeed({ 200, 200 });
            bp->setCoord({ surface_->getCenterCoord().x - bp->surface_->getDims().x / 2,
                                    surface_->getCenterCoord().y - bp->surface_->getDims().y / 2 });
            bp->sprite_->setTint(sprite_->getTint());
            Pattern::circle(bp, nb_bullets);
            delete bp;
        }
    }

    int nb_bullets;
};
