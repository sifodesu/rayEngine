#include "character.h"
#include "object_m.h"
#include "functional"
#include "raymath.h"
#include "input.h"
#include "raycam_m.h"

#define SPEED 100
#define DASHFACTOR 4

Character::Character(nlohmann::json obj) : GObject(Object_m::genID()) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    dashing_ = 0;

    Raycam_m::setTarget(body_);
}

void Character::routine() {
    double delta = Clock::getLap();
    dashing_ -= delta;

    Vector2 bodySpeed = body_->getSpeed();
    if (dashing_ <= 0) {
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
    }
    bodySpeed = body_->getSpeed();
    if (InputMap::checkPressed("dash")) {
        if (dashing_ <= 0 && (bodySpeed.x != 0 || bodySpeed.y != 0)) {
            dashing_ = 0.1;
            body_->setSpeed(Vector2Multiply(bodySpeed, { DASHFACTOR, DASHFACTOR }));
        }
    }

    sprite_->routine();
    body_->routine();

    bodySpeed = body_->getSpeed();
    if (bodySpeed.x == 0 && bodySpeed.y == 0) {
        dashing_ = 0;
    }
}

void Character::draw() {
    sprite_->draw(body_->getCoord());
}
