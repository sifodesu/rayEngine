#include <iostream>
#include "definitions.h"
#include "bullet.h"
#include "character.h"

using json = nlohmann::json;
using namespace std;

Bullet::Bullet(json obj) : GObject(obj["ID"]), ttl_(10) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    if (obj.contains("ttl")) {
        ttl_ = obj["ttl"];
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

void Bullet::routine() {
    auto vec = body_->getCollisions();
    for (auto body : vec) {
        if (body->isSolid() && t(*body->father_) != t(Character)
            && t(*body->father_) != t(Bullet)) {
            ttl_ = 0;
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
