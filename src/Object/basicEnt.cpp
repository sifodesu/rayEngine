#include <iostream>
#include "basicEnt.h"

using json = nlohmann::json;
using namespace std;

BasicEnt::BasicEnt(json obj) : GObject(obj["ID"]), sprite_(NULL), body_(NULL) {
    if (!obj.contains("sprite")) {
        cout << "ERROR: no sprite for basic ent" << endl;
        return;
    }
    if (!obj.contains("body")) {
        cout << "ERROR: no rigidbody for basic ent" << endl;
        return;
    }

    sprite_ = new Sprite(obj["sprite"]);
    body_ = new RigidBody(obj["body"], this);
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