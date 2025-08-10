#include "character.h"
#include "raymath.h"
#include "input.h"
#include "raycam_m.h"

static constexpr float CHARACTER_SPEED = 300.0f;
static constexpr float CHARACTER_DASH_FACTOR = 4.0f;

Character::Character(const SpawnData& data) : GObject(data.id) {
    if (data.sprite) {
        sprite_ = new Sprite(data.sprite->filename, data.sprite->source);
        sprite_->setTint(data.sprite->tint);
    } else {
        std::string fallback = "inv.png";
        sprite_ = new Sprite(fallback, Rectangle{0, 0, 32, 32});
    }
    CollisionDesc col = data.collision.value_or(CollisionDesc{});
    BodyDesc body = data.body.value_or(BodyDesc{});
    body_ = new RigidBody(col, body, this);
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
            if (InputMap::checkPressed("up"))
                body_->setSpeed({ bodySpeed.x, -CHARACTER_SPEED });
            // else if (bodySpeed.y < 0)
                // body_->setSpeed({ bodySpeed.x, 0 });

            // if (InputMap::checkDown("down"))
            //     body_->setSpeed({ bodySpeed.x, SPEED });
            // else if (bodySpeed.y > 0)
            //     body_->setSpeed({ bodySpeed.x, 0 });
        }

        bodySpeed = body_->getSpeed();
        if (InputMap::checkDown("left") && InputMap::checkDown("right")) {
            body_->setSpeed({ 0, bodySpeed.y });
        }
        else {
            if (InputMap::checkDown("left"))
                body_->setSpeed({ -CHARACTER_SPEED, bodySpeed.y });
            else if (bodySpeed.x < 0)
                body_->setSpeed({ 0, bodySpeed.y });

            if (InputMap::checkDown("right"))
                body_->setSpeed({ CHARACTER_SPEED, bodySpeed.y });
            else if (bodySpeed.x > 0)
                body_->setSpeed({ 0, bodySpeed.y });
        }
    }
    bodySpeed = body_->getSpeed();
    if (InputMap::checkPressed("dash")) {
        if (dashing_ <= 0 && (bodySpeed.x != 0 || bodySpeed.y != 0)) {
            dashing_ = 0.1;
            body_->setSpeed(Vector2Multiply(bodySpeed, { CHARACTER_DASH_FACTOR, 1 }));
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
