#include "character.h"
#include "input.h"
#include "runes.h"
#include "object_m.h"
#include "bullet.h"
#include "pattern.h"
#include "functional"
#include "bullet_m.h"
#include "rigidBullet.h"
#include "zigzagBullet.h"
#include "explosiveBullet.h"
#include "raymath.h"

#define SPEED 100
#define DASHFACTOR 4

Character::Character(nlohmann::json obj) : HObject(obj) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    dashing_ = 0;
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

    while (Runes::getBullet()) {
        shoot();
    }
}

void Character::draw() {
    sprite_->draw(body_->getCoord());
}

void Character::shoot() {
    RigidBullet* bullet = (RigidBullet*)Bullet_m::createBullet(RIGID, { this });
    bullet->setCoord(body_->getCoord());
    bullet->setSpeed({ 0, -200 });
    bullet->ttl_ = 5;
    bullet->sprite_->setTint(BLUE);
    bullet->setCurve(0);
    bullet->setAcceleration(0);
}
