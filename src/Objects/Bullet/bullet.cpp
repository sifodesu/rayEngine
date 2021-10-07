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

void Bullet::setTargets(std::unordered_set<HObject*> targets) {
    targets_ = targets;
}

void Bullet::routine() {
    ttl_ -= clock_.getLap();

    if (ttl_ <= 0) {
        to_delete_ = true;
        return;
    }

    sprite_->routine();
}
