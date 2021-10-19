#include <iostream>
#include "definitions.h"
#include "bullet.h"
#include "character.h"

using json = nlohmann::json;
using namespace std;

Bullet::Bullet(json obj, bool newSurface) : GObject(obj["ID"]), ttl_(2), dmg_(1) {
    sprite_ = new Sprite(obj);
    if (newSurface) {
        surface_ = new CollisionRect(obj, this);
    }
    else {
        surface_ = nullptr;
    }

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
    sprite_->draw(surface_->getCoord());
}

void Bullet::setNoDmg(std::unordered_set<GObject*> no_dmg) {
    no_dmg_ = no_dmg;
}

void Bullet::routine() {
    auto vec = RigidBody::query(surface_->getSurface());
    for (auto body : vec) {
        if (no_dmg_.empty()) {
            break;
        }
        if (!no_dmg_.contains(body->getFather())) {
            body->getFather()->onCollision(this);
            if (body->isSolid()) {
                ttl_ = 0;
            }
        }
    }

    ttl_ -= Clock::getLap();
    sprite_->routine();
}

void Bullet::setCoord(Vector2 pos) {
    surface_->setCoord(pos);
}