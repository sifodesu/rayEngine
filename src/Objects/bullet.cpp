#include <iostream>
#include "definitions.h"
#include "bullet.h"
#include "character.h"

using json = nlohmann::json;
using namespace std;

Bullet::Bullet(json obj) : GObject(obj["ID"]), ttl_(10), dmg_(1) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    if (obj.contains("ttl")) {
        ttl_ = obj["ttl"];
    }
    if (obj.contains("dmg")) {
        dmg_ = obj["dmg"];
    }
}

Bullet::~Bullet() {
    if (sprite_) {
        delete sprite_;
    }
    if (body_) {
        delete body_;
    }
}

void Bullet::draw() {
    sprite_->draw(body_->getCoord());
}

void Bullet::setTargets(std::unordered_set<HObject*> targets) {
    targets_ = targets;
}

void Bullet::routine() {
    auto vec = body_->getCollisions();
    for (auto body : vec) {
        HObject* father = (HObject*)(body->father_);
        if (body->isSolid() && targets_.contains(father)) {
            ttl_ = 0;
            father->changeHP(-dmg_);
        }
    }

    ttl_ -= clock_.getLap();

    if (ttl_ <= 0) {
        to_delete_ = true;
        return;
    }

    body_->routine();
    sprite_->routine();
}
