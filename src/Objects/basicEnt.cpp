#include <iostream>
#include "basicEnt.h"

using json = nlohmann::json;
using namespace std;

BasicEnt::BasicEnt(json obj) : GObject(obj["ID"]) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
}

BasicEnt::~BasicEnt() {
    if (sprite_)
        delete sprite_;
    if (body_)
        delete body_;
}

void BasicEnt::draw(Vector2 pos) {
    sprite_->draw(pos);
}

void BasicEnt::routine(){
    body_->routine();
}