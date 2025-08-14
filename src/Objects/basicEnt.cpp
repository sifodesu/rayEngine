#include <iostream>
#include "basicEnt.h"

using namespace std;

BasicEnt::BasicEnt(const SpawnData& data) : GObject(data.id) {
    SpriteDesc sprite = data.sprite.value_or(SpriteDesc{});
    sprite_ = new Sprite(sprite);

    CollisionDesc col = data.collision.value_or(CollisionDesc{});
    body_ = new CollisionRect(col, this);
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