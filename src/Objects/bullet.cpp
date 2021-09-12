#include <iostream>
#include "bullet.h"

using json = nlohmann::json;
using namespace std;

Bullet::Bullet(json obj) : GObject(obj["ID"]) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    body_->setSolid(false);
    if(obj.contains("ttl"))
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
    body_->routine();
    sprite_->routine();
    ttl_ -= clock_.getLap();
}

double Bullet::getTTL(){
    return ttl_;
}