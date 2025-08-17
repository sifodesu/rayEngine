#include "character.h"
#include "raymath.h"
#include "input.h"
#include "raycam_m.h"
#include "sprite_m.h"

static constexpr float CHARACTER_BASE_SPEED = 200.0f;
static constexpr float JUMP_SPEED = 300.0f;
static constexpr float CHARACTER_DASH_FACTOR = 4.0f;

Character::Character(const SpawnData& data) : GObject(data.id) {
    CollisionDesc col = data.collision.value_or(CollisionDesc{});
    BodyDesc body = data.body.value_or(BodyDesc{});
    body_ = new RigidBody(col, body, this);
    dashing_ = 0;
    Raycam_m::setTarget(body_, true);
    if (auto s = Sprite_m::get("chara_idle")) anims_["idle"] = new Sprite(*s); else anims_["idle"] = new Sprite(SpriteDesc{});
    if (auto s = Sprite_m::get("chara_walk")) anims_["walk"] = new Sprite(*s); else anims_["walk"] = new Sprite(SpriteDesc{});
    current_anim_ = anims_["idle"];
}

void Character::routine() {
    double delta = Clock::getLap();
    dashing_ -= delta;

    Vector2 bodySpeed = body_->getSpeed();
    // Reset jump counter if we are on the ground
    if (isOnGround()) {
        jumps_ = 2;
        dashCount_ = 1;
    }
    else {
        jumps_ = std::min(jumps_, 1);
    }
    if (dashing_ > 0) {
        body_->setSpeed({ bodySpeed.x, 0});
    }
    else {
        // Jump: allow at most 2 jumps until grounded again
        if (InputMap::checkPressed("r1") && jumps_ > 0) {
            body_->setSpeed({ bodySpeed.x, -JUMP_SPEED});
            jumps_--;
        }

        bodySpeed = body_->getSpeed();
        if (InputMap::checkDown("left") && InputMap::checkDown("right"))
            body_->setSpeed({ 0, bodySpeed.y });
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
    if (InputMap::checkPressed("dash") && dashCount_ > 0) {
        if (dashing_ <= 0 && (bodySpeed.x != 0)) {
            dashing_ = 0.1;
            body_->setSpeed(Vector2Multiply(bodySpeed, { CHARACTER_DASH_FACTOR * dashMultiplier_, 0 }));
            dashCount_--;
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
        jumps_ = 1; // reset on respawn
        dashCount_ = 1; // reset dash count on respawn
    }
}

bool Character::isOnGround() const {
    // Consider grounded if there's a solid directly below the character rect
    Rectangle probe = body_->getSurface();
    probe.y += 0.1f;
    for (CollisionRect* other : CollisionRect::query(probe, true)) {
        if (other->isSolid() && (other->getId() != body_->getId()))
            return true;
    }
    return false;
}