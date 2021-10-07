#include <iostream>
#include "definitions.h"
#include "bullet.h"
#include "character.h"

using json = nlohmann::json;
using namespace std;

Bullet::Bullet(json obj) : GObject(obj["ID"]), ttl_(2), dmg_(1) {
    sprite_ = new Sprite(obj);

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
}

void Bullet::draw() {
    sprite_->draw(pos_);
}

void Bullet::setNoDmg(std::unordered_set<GObject*> no_dmg) {
    no_dmg_ = no_dmg;
}

void Bullet::routine() {
    Rectangle rect = { pos_.x, pos_.y, sprite_->getFrameDim().x, sprite_->getFrameDim().y };
    auto vec = RigidBody::query(rect);
    for (auto body : vec) {
        if (!no_dmg_.contains(body->father_)) {
            body->father_->onCollision(this);
            if (body->isSolid()) {
                ttl_ = 0;
            }
        }
    }

    ttl_ -= clock_.getLap();
    if (ttl_ <= 0) {
        to_delete_ = true;
        return;
    }

    sprite_->routine();
}
