#include "bSpawner.h"

BSpawner::BSpawner(nlohmann::json obj) : GObject(obj["ID"]) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
}

void BSpawner::routine() {
    
}

BSpawner::~BSpawner() {
    if (sprite_)
        delete sprite_;
    if (body_)
        delete body_;
}