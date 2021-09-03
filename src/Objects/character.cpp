#include "character.h"
#include "input.h"
#include "runes.h"

#define SPEED 300

Character::Character(nlohmann::json obj) : GObject(obj["ID"]) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
}


void Character::routine() {
    Vector2 bodySpeed = body_->getSpeed();
    if (InputMap::checkDown("up") && InputMap::checkDown("down")) {
        body_->setSpeed({ bodySpeed.x, 0 });
    }
    else {
        if (InputMap::checkDown("up"))
            body_->setSpeed({ bodySpeed.x, -SPEED });
        else if (bodySpeed.y < 0)
            body_->setSpeed({ bodySpeed.x, 0 });

        if (InputMap::checkDown("down"))
            body_->setSpeed({ bodySpeed.x, SPEED });
        else if (bodySpeed.y > 0)
            body_->setSpeed({ bodySpeed.x, 0 });
    }

    bodySpeed = body_->getSpeed();
    if (InputMap::checkDown("left") && InputMap::checkDown("right")) {
        body_->setSpeed({ 0, bodySpeed.y });
    }
    else {
        if (InputMap::checkDown("left"))
            body_->setSpeed({ -SPEED, bodySpeed.y });
        else if (bodySpeed.x < 0)
            body_->setSpeed({ 0, bodySpeed.y });

        if (InputMap::checkDown("right"))
            body_->setSpeed({ SPEED, bodySpeed.y });
        else if (bodySpeed.x > 0)
            body_->setSpeed({ 0, bodySpeed.y });
    }
    
    sprite_->routine();
    body_->routine();

}

void Character::draw(Vector2 pos) {
    sprite_->draw(pos);
}