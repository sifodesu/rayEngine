#include "character.h"
#include "raymath.h"
#include "input.h"
#include "raycam_m.h"
#include "sprite_m.h"
#include "receptacle.h"
#include "definitions.h"

static constexpr float CHARACTER_DASH_FACTOR_BASE = 4.0f; // base before adi scaling

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

bool Character::depositOneAdi() {
    if (adiCount_ <= 0) return false;
    adiCount_ -= 1;
    return true;
}

bool Character::retrieveOneAdi() {
    if (adiCount_ >= adiMax_) return false;
    adiCount_ += 1;
    return true;
}

void Character::onRoomEntered() {
    Receptacle::recallAll(*this);
}

float Character::currentJumpSpeed() const {
    // base plus per-charge bonus
    return debugJumpSpeed_;// * (1.0f + adiJumpBonusPerCharge_ * adiCount_);
}
float Character::currentDashFactor() const {
    return CHARACTER_DASH_FACTOR_BASE; //* (1.0f + adiDashBonusPerCharge_ * adiCount_);
}

void Character::routine() {
    double delta = Clock::getLap();
    dashing_ -= delta;
    
    // Collision separation - push character out if embedded in solid objects
    separateFromCollisions();

    Vector2 bodySpeed = body_->getSpeed();

    // Reset jump counter if we are on the ground
    if (isOnGround()) {
        jumps_ = 2;
    } else {
        jumps_ = std::min(jumps_, 1);
    }
    if (dashing_ > 0) {
        body_->setSpeed({ bodySpeed.x, 0});
    }
    else {
        // Jump: allow at most 2 jumps until grounded again
        if (InputMap::checkPressed("r1") && jumps_ > 0) {
            body_->setSpeed({ bodySpeed.x, -currentJumpSpeed()});
            jumps_--;
            jumpHeld_ = true;
        }

        bodySpeed = body_->getSpeed();
        if (InputMap::checkDown("left") && InputMap::checkDown("right")) {
            body_->setSpeed({ 0, bodySpeed.y });
        }
        else {
            float moveSpeed = debugBaseSpeed_ * speedMultiplier_;
            if (InputMap::checkDown("left")) {
                body_->setSpeed({ -moveSpeed, bodySpeed.y });
            } else if (bodySpeed.x < 0) {
                body_->setSpeed({ 0, bodySpeed.y });
            }

            if (InputMap::checkDown("right")) {
                body_->setSpeed({ moveSpeed, bodySpeed.y });
            } else if (bodySpeed.x > 0) {
                body_->setSpeed({ 0, bodySpeed.y });
            }
        }

        bodySpeed = body_->getSpeed();
        if (InputMap::checkReleased("r1")) {
            // Cut jump short if going up
            if (bodySpeed.y < 0 && jumpHeld_) {
                body_->setSpeed({ bodySpeed.x, bodySpeed.y * 0.5f });
            }
        }
    }
    if (InputMap::checkUp("r1")) {
        jumpHeld_ = false;
    }
    bodySpeed = body_->getSpeed();
    
    dashCooldownLeft_ = std::max(dashCooldownLeft_ - delta, 0.0);
    if (InputMap::checkPressed("dash")) {
        // Conditions: not already dashing, some horizontal movement (direction), cooldown ready
        if (dashing_ <= 0 && dashCooldownLeft_ <= 0 && bodySpeed.x != 0) {
            dashing_ = 0.1; // dash active window duration (seconds)
            body_->setSpeed(Vector2Multiply(bodySpeed, { currentDashFactor() * dashMultiplier_, 0 }));
            dashCooldownLeft_ = dashCooldown_; // reset cooldown
        }
    }

    // Deposit adi into overlapping receptacle (press r2)
    if (InputMap::checkPressed("r2") && canDepositAdi()) {
        Rectangle mine = body_->getSurface();
        for (CollisionRect* other : CollisionRect::query(mine)) {
            if (other->getFather() == this) continue;
            if (!CheckCollisionRecs(mine, other->getSurface())) continue;
            if (auto* rec = dynamic_cast<Receptacle*>(other->getFather())) {
                if (rec->deposit(*this)) break; // deposit only one per press
            }
        }
    }

    // Recall all adis from all receptacles (press r3)
    if (InputMap::checkPressed("r3")) {
        Receptacle::recallAll(*this);
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

    Vector2 center = body_->getCenterCoord();
    int gx = (int)floorf(center.x / (float)NATIVE_RES_WIDTH);
    int gy = (int)floorf(center.y / (float)NATIVE_RES_HEIGHT);
    if (lastRoomGX_ == INT32_MIN && lastRoomGY_ == INT32_MIN) {
        lastRoomGX_ = gx; lastRoomGY_ = gy; // init
    } else if (gx != lastRoomGX_ || gy != lastRoomGY_) {
        lastRoomGX_ = gx; lastRoomGY_ = gy;
        onRoomEntered(); // refill adi (or extend with future per-room logic)
    }
}

void Character::draw() {
    current_anim_->draw(body_->getCoord());
    // Draw adi count above character (simple UI for now)
    Vector2 pos = body_->getCoord();
    // DrawText(TextFormat("ADI: %d/%d", adiCount_, adiMax_), (int)pos.x, (int)pos.y - 20, 8, WHITE);
    // DrawText(TextFormat("Speed: %.0f Jump: %.0f", debugBaseSpeed_, debugJumpSpeed_), (int)pos.x, (int)pos.y - 30, 8, YELLOW);
}

void Character::respawn() {
    if (body_) {
        body_->setCoord(respawnPos_);
        body_->setSpeed({0,0});
        jumps_ = 1; // reset on respawn
        dashCooldownLeft_ = 0; // dash immediately available on respawn
    }
}

void Character::separateFromCollisions() {
    Rectangle myRect = body_->getSurface();
    const float separationStep = 0.5f;
    const int maxAttempts = 20;
    
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        bool hasCollision = false;
        Vector2 separation = {0, 0};
        
        for (CollisionRect* other : CollisionRect::query(myRect, true)) {
            if (!other->isSolid() || other->getId() == body_->getId()) continue;
            
            Rectangle otherRect = other->getSurface();
            if (!CheckCollisionRecs(myRect, otherRect)) continue;
            
            hasCollision = true;
            
            // Calculate overlap
            float overlapX = std::min(myRect.x + myRect.width - otherRect.x, 
                                    otherRect.x + otherRect.width - myRect.x);
            float overlapY = std::min(myRect.y + myRect.height - otherRect.y,
                                    otherRect.y + otherRect.height - myRect.y);
            
            // Separate along the axis with smaller overlap
            if (overlapX < overlapY) {
                // Separate horizontally
                if (myRect.x < otherRect.x) {
                    separation.x -= separationStep;
                } else {
                    separation.x += separationStep;
                }
            } else {
                // Separate vertically
                if (myRect.y < otherRect.y) {
                    separation.y -= separationStep;
                } else {
                    separation.y += separationStep;
                }
            }
        }
        
        if (!hasCollision) break;
        
        // Apply separation
        myRect.x += separation.x;
        myRect.y += separation.y;
        body_->setSurface(myRect);
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