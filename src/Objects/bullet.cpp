#include <iostream>
#include "definitions.h"
#include "bullet.h"
#include "character.h"

using json = nlohmann::json;
using namespace std;

Bullet::Bullet(json obj) : GObject(obj["ID"]) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    if (obj.contains("ttl"))
        ttl_ = obj["ttl"];
    else
        ttl_ = 10;
}

Bullet::~Bullet() {
    if (sprite_)
        delete sprite_;
    if (body_)
        delete body_;
}

void Bullet::draw() {
    sprite_->draw(body_->getCoord());
}

void Bullet::routine() {
    auto vec = body_->getCollisions();
    for (auto body : vec) {
        if (body->isSolid() && t(*body->father_) != t(Character)) {
            ttl_ = 0;
            return;
        }
    }

    body_->routine();
    sprite_->routine();
    ttl_ -= clock_.getLap();
}

double Bullet::getTTL() {
    return ttl_;
}