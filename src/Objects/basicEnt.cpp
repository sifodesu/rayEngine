#include <iostream>
#include "basicEnt.h"

using json = nlohmann::json;
using namespace std;

BasicEnt::BasicEnt(json obj) : GObject(obj["ID"]) {
    sprite_ = new Sprite(obj);
    body_ = new CollisionRect(obj, this);
}

BasicEnt::~BasicEnt() {
    if (sprite_)
        delete sprite_;
    if (body_)
        delete body_;
}

void BasicEnt::draw() {
    sprite_->draw(body_->getCoord());
}

void BasicEnt::routine(){
    body_->routine();
    sprite_->routine();
}