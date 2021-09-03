#include "character.h"
#include "input.h"

#define SPEED 300

Character::Character(nlohmann::json obj) : GObject(obj["ID"]) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
}


void Character::routine() {
    Vector2 bodySpeed = body_->getSpeed();
    if (InputMap::checkAction("left"))
        body_->setSpeed({ -SPEED, bodySpeed.y });
    else if (bodySpeed.x < 0)
        body_->setSpeed({ 0, bodySpeed.y });

    if (InputMap::checkAction("right"))
        body_->setSpeed({ SPEED, bodySpeed.y });
    else if (bodySpeed.x > 0)
        body_->setSpeed({ 0, bodySpeed.y });

    if (InputMap::checkAction("up"))
        body_->setSpeed({ bodySpeed.x, -SPEED });
    else if (bodySpeed.y < 0)
        body_->setSpeed({ bodySpeed.x, 0 });

    if (InputMap::checkAction("down"))
        body_->setSpeed({ bodySpeed.x, SPEED });
    else if (bodySpeed.y > 0)
        body_->setSpeed({ bodySpeed.x, 0 });

    // if (InputMap::checkAction(""))
    sprite_->routine();
    body_->routine();

}

void Character::draw(Vector2 pos) {
    sprite_->draw(pos);
}