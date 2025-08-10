#include <iostream>
#include "basicEnt.h"

using namespace std;

BasicEnt::BasicEnt(const SpawnData& data) : GObject(data.id) {
    if (data.sprite) {
        sprite_ = new Sprite(data.sprite->filename, data.sprite->source);
        sprite_->setTint(data.sprite->tint);
    } else {
        std::string fallback2 = "inv.png";
        sprite_ = new Sprite(fallback2, Rectangle{0, 0, 32, 32});
    }
    if (data.collision) {
        body_ = new CollisionRect(*data.collision, this);
    } else {
        body_ = new CollisionRect(CollisionDesc{}, this);
    }
}

BasicEnt::~BasicEnt()
{
    if (sprite_)
        delete sprite_;
    if (body_)
        delete body_;
}

void BasicEnt::draw()
{
    sprite_->draw(body_->getCoord());
}

void BasicEnt::routine()
{
    sprite_->routine();
}