#include "character.h"
#include "raymath.h"
#include "input.h"
#include "raycam_m.h"
#include "sprite_m.h"

static constexpr float CHARACTER_BASE_SPEED = 200.0f;
static constexpr float JUMP_SPEED = 300.0f;
static constexpr float CHARACTER_DASH_FACTOR = 4.0f; // scaled by dashMultiplier_

Character::Character(const SpawnData& data) : GObject(data.id) {
    SpriteDesc sprite = data.sprite.value_or(SpriteDesc{});
    CollisionDesc col = data.collision.value_or(CollisionDesc{});
    BodyDesc body = data.body.value_or(BodyDesc{});
    body_ = new RigidBody(col, body, this);
    dashing_ = 0;
    Raycam_m::setTarget(body_, true);
    if (auto s = Sprite_m::get("chara_idle")) anims_["idle"] = new Sprite(*s); else anims_["idle"] = new Sprite("inv.png", Rectangle{0,0,20,20});
    if (auto s = Sprite_m::get("chara_walk")) anims_["walk"] = new Sprite(*s); else anims_["walk"] = new Sprite("inv.png", Rectangle{0,0,20,20});
    current_anim_ = anims_["idle"];
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
                body_->setSpeed({ bodySpeed.x, -JUMP_SPEED});
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
            float moveSpeed = CHARACTER_BASE_SPEED * speedMultiplier_;
            if (InputMap::checkDown("left"))
                body_->setSpeed({ -moveSpeed, bodySpeed.y });
            else if (bodySpeed.x < 0)
                body_->setSpeed({ 0, bodySpeed.y });

            if (InputMap::checkDown("right"))
                body_->setSpeed({ moveSpeed, bodySpeed.y });
            else if (bodySpeed.x > 0)
                body_->setSpeed({ 0, bodySpeed.y });
        }
    }
    bodySpeed = body_->getSpeed();
    if (InputMap::checkPressed("dash")) {
        if (dashing_ <= 0 && (bodySpeed.x != 0 || bodySpeed.y != 0)) {
            dashing_ = 0.1;
            body_->setSpeed(Vector2Multiply(bodySpeed, { CHARACTER_DASH_FACTOR * dashMultiplier_, 1 }));
        }
    }

    current_anim_->routine();
    body_->routine();

    bodySpeed = body_->getSpeed();

    if (bodySpeed.x == 0 && bodySpeed.y == 0) {
        dashing_ = 0;
    }
    if (bodySpeed.x < 0) {
        current_anim_ = anims_["walk"];
        current_anim_->setFlipX(true);
    } else if (bodySpeed.x > 0) {
        current_anim_ = anims_["walk"];
        current_anim_->setFlipX(false);
    }
    else {
        bool was_flipped = current_anim_->getFlipX();
        current_anim_ = anims_["idle"];
        current_anim_->setFlipX(was_flipped);
    }
}

void Character::draw() {
    current_anim_->draw(body_->getCoord());
}

void Character::respawn() {
    if (body_) {
        body_->setCoord(respawnPos_);
        body_->setSpeed({0,0});
    }
}
