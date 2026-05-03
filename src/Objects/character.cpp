#include "character.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "input.h"
#include "raycam_m.h"
#include "sprite_m.h"
#include "receptacle.h"
#include "plateforme.h"
#include "basicEnt.h"
#include "adiComponent.h"
#include "adiComponent.h"
#include "definitions.h"
#include "portal.h"
#include "oneWayPlatform.h"
#include "particle_m.h"
#include "water.h"

static constexpr float CHARACTER_DASH_FACTOR_BASE = 4.0f; // base before adi scaling

namespace {

float fallSpeedAlongGravity(RigidBody* body) {
    if (!body) return 0.0f;

    Vector2 speed = body->getSpeed();
    switch (body->getGravityDirection()) {
        case GravityDirection::DOWN:
            return speed.y;
        case GravityDirection::UP:
            return -speed.y;
        case GravityDirection::LEFT:
            return -speed.x;
        case GravityDirection::RIGHT:
            return speed.x;
    }
    return 0.0f;
}

float impactStrength(float fallSpeed, float threshold) {
    return std::clamp((fallSpeed - threshold) / 220.0f + 0.65f, 0.65f, 1.85f);
}

} // namespace

Character::Character(const SpawnData& data) : GObject(data.id) {
    CollisionDesc col = data.physics.collision.value_or(CollisionDesc{});
    BodyDesc body = data.physics.body.value_or(BodyDesc{});
    body_ = new RigidBody(col, body, this);
    dashing_ = 0;
    Raycam_m::setTarget(body_, true);
    if (auto s = Sprite_m::get("chara_idle")) anims_["idle"] = new Sprite(*s); else anims_["idle"] = new Sprite(SpriteDesc{});
    if (auto s = Sprite_m::get("chara_walk")) anims_["walk"] = new Sprite(*s); else anims_["walk"] = new Sprite(SpriteDesc{});
    if (data.sprite && data.sprite->glitched) {
        for (auto& [name, sprite] : anims_) {
            if (sprite) sprite->setGlitched(true);
        }
    }
    current_anim_ = anims_["idle"];
    
    // Store original hitbox dimensions
    originalHitboxDims_ = body_->getDims();
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
}

float Character::currentJumpSpeed() const {
    return debugJumpSpeed_;
}
float Character::currentDashFactor() const {
    return CHARACTER_DASH_FACTOR_BASE;
}

void Character::routine() {
    double delta = Clock::getLap();
    wasTouchingStillWater_ = GetTime() - lastStillWaterTouchAt_ <= Particle_m::params().stillWaterTouchGrace;
    dashing_ -= delta;
    
    // Collision separation - push character out if embedded in solid objects
    separateFromCollisions();

    Vector2 bodySpeed = body_->getSpeed();
    
    // Get gravity direction for movement adaptation
    RigidBody* rigidBody = dynamic_cast<RigidBody*>(body_);
    GravityDirection gravityDir = rigidBody ? rigidBody->getGravityDirection() : GravityDirection::DOWN;
    
    // Update hitbox rotation if gravity direction changed
    if (gravityDir != lastGravityDir_) {
        lastGravityDir_ = gravityDir;
        updateHitboxRotation();
    }

    const bool groundedAtStart = isOnGround();
    const bool hadGroundState = groundStateInitialized_;
    const float fallSpeedBeforeBody = fallSpeedAlongGravity(rigidBody);
    lastFallSpeedBeforeMove_ = fallSpeedBeforeBody;
    if (!groundStateInitialized_) {
        wasGrounded_ = groundedAtStart;
        groundStateInitialized_ = true;
    }

    // Reset jump counter if we are on the ground
    if (groundedAtStart) {
        jumps_ = 2;
    } else {
        jumps_ = std::min(jumps_, 1);
    }
    
    // Disable gravity during dash
    if (dashing_ > 0) {
        body_->setGravityEnabled(false);
        // During dash, preserve movement in the appropriate direction based on gravity
        switch (gravityDir) {
            case GravityDirection::DOWN:
            case GravityDirection::UP:
                body_->setSpeed({ bodySpeed.x, 0});
                break;
            case GravityDirection::LEFT:
            case GravityDirection::RIGHT:
                body_->setSpeed({ 0, bodySpeed.y});
                break;
        }
    } else {
        body_->setGravityEnabled(true);
        
        // Jump: allow at most 2 jumps until grounded again
        // Jump direction depends on gravity direction
        if (InputMap::checkPressed("r1") && jumps_ > 0) {
            float jumpSpeed = currentJumpSpeed();
            if (groundedAtStart) {
                Particle_m::emitJumpDust(body_->getSurface(), gravityDir);
            }
            switch (gravityDir) {
                case GravityDirection::DOWN:
                    body_->setSpeed({ bodySpeed.x, -jumpSpeed}); // Jump up
                    break;
                case GravityDirection::UP:
                    body_->setSpeed({ bodySpeed.x, jumpSpeed}); // Jump down (away from ceiling)
                    break;
                case GravityDirection::LEFT:
                    body_->setSpeed({ jumpSpeed, bodySpeed.y}); // Jump right (away from left wall)
                    break;
                case GravityDirection::RIGHT:
                    body_->setSpeed({ -jumpSpeed, bodySpeed.y}); // Jump left (away from right wall)
                    break;
            }
            jumps_--;
            jumpHeld_ = true;
        }

        bodySpeed = body_->getSpeed();
        
        // Movement input - direction depends on gravity
        float moveSpeed = debugBaseSpeed_ * speedMultiplier_;
        
        switch (gravityDir) {
            case GravityDirection::DOWN:
            case GravityDirection::UP:
                // Normal/inverted gravity: left/right moves horizontally
                if (InputMap::checkDown("left") && InputMap::checkDown("right")) {
                    body_->setSpeed({ 0, bodySpeed.y });
                } else {
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
                break;
                
            case GravityDirection::LEFT:
                // Left wall gravity: left/right moves vertically (up/down on the wall)
                if (InputMap::checkDown("left") && InputMap::checkDown("right")) {
                    body_->setSpeed({ bodySpeed.x, 0 });
                } else {
                    // On left wall: "left" = move up wall, "right" = move down wall
                    if (InputMap::checkDown("left")) {
                        body_->setSpeed({ bodySpeed.x, -moveSpeed });
                    } else if (bodySpeed.y < 0) {
                        body_->setSpeed({ bodySpeed.x, 0 });
                    }

                    if (InputMap::checkDown("right")) {
                        body_->setSpeed({ bodySpeed.x, moveSpeed });
                    } else if (bodySpeed.y > 0) {
                        body_->setSpeed({ bodySpeed.x, 0 });
                    }
                }
                break;
                
            case GravityDirection::RIGHT:
                // Right wall gravity: reverse inputs for natural feel
                if (InputMap::checkDown("left") && InputMap::checkDown("right")) {
                    body_->setSpeed({ bodySpeed.x, 0 });
                } else {
                    // On right wall: "left" = move down wall, "right" = move up wall (reversed)
                    if (InputMap::checkDown("left")) {
                        body_->setSpeed({ bodySpeed.x, moveSpeed });
                    } else if (bodySpeed.y > 0) {
                        body_->setSpeed({ bodySpeed.x, 0 });
                    }

                    if (InputMap::checkDown("right")) {
                        body_->setSpeed({ bodySpeed.x, -moveSpeed });
                    } else if (bodySpeed.y < 0) {
                        body_->setSpeed({ bodySpeed.x, 0 });
                    }
                }
                break;
        }

        bodySpeed = body_->getSpeed();
        
        // Cut jump short on release
        if (InputMap::checkReleased("r1")) {
            if (jumpHeld_) {
                switch (gravityDir) {
                    case GravityDirection::DOWN:
                        if (bodySpeed.y < 0) body_->setSpeed({ bodySpeed.x, bodySpeed.y * 0.5f });
                        break;
                    case GravityDirection::UP:
                        if (bodySpeed.y > 0) body_->setSpeed({ bodySpeed.x, bodySpeed.y * 0.5f });
                        break;
                    case GravityDirection::LEFT:
                        if (bodySpeed.x > 0) body_->setSpeed({ bodySpeed.x * 0.5f, bodySpeed.y });
                        break;
                    case GravityDirection::RIGHT:
                        if (bodySpeed.x < 0) body_->setSpeed({ bodySpeed.x * 0.5f, bodySpeed.y });
                        break;
                }
            }
        }
    }
    if (InputMap::checkUp("r1")) {
        jumpHeld_ = false;
    }
    bodySpeed = body_->getSpeed();
    
    dashCooldownLeft_ = std::max(dashCooldownLeft_ - delta, 0.0);
    if (InputMap::checkPressed("dash")) {
        // Conditions: not already dashing, cooldown ready
        if (dashing_ <= 0 && dashCooldownLeft_ <= 0) {
            dashing_ = 0.1; // dash active window duration (seconds)
            bool is_flipped = current_anim_->getFlipX();
            float dashSpeed = currentDashFactor() * dashMultiplier_ * debugBaseSpeed_;
            
            // Dash direction depends on gravity direction
            switch (gravityDir) {
                case GravityDirection::DOWN:
                    // Normal gravity: dash horizontally based on flip
                    body_->setSpeed({ dashSpeed * (is_flipped ? -1 : 1), bodySpeed.y });
                    break;
                case GravityDirection::UP:
                    // Inverted gravity: dash logic is inverted compared to normal gravity
                    // because the visual sprite is rotated 180 degrees.
                    body_->setSpeed({ dashSpeed * (is_flipped ? 1 : -1), bodySpeed.y });
                    break;
                case GravityDirection::LEFT:
                case GravityDirection::RIGHT:
                    // Side gravity: dash vertically based on flip
                    body_->setSpeed({ bodySpeed.x, dashSpeed * (is_flipped ? -1 : 1) });
                    break;
            }
            dashCooldownLeft_ = dashCooldown_; // reset cooldown
        }
    }

    // Deposit adi into overlapping object with AdiComponent (press r2)
    if (InputMap::checkPressed("r2") && canDepositAdi()) {
        Rectangle mine = body_->getSurface();
        
        // Expand detection area slightly to include touching objects
        const float touchDistance = 2.0f; // pixels
        Rectangle expandedMine = {
            mine.x - touchDistance,
            mine.y - touchDistance,
            mine.width + 2 * touchDistance,
            mine.height + 2 * touchDistance
        };
        
        for (CollisionRect* other : CollisionRect::query(expandedMine)) {
            if (other->getFather() == this) continue;
            if (!CheckCollisionRecs(expandedMine, other->getSurface())) continue;
            
            // Try to get AdiComponent from the object using optional system
            if (auto adiCompOpt = other->getFather()->getAdiComponent()) {
                AdiComponent* adiComp = *adiCompOpt;
                if (adiComp->canReceiveAdi && adiComp->depositAdi(*this)) {
                    break; // deposit only one per press
                }
            }
        }
    }
    // Retrieve adi from overlapping object with AdiComponent (press r3)
    if (InputMap::checkPressed("r3") && adiCount_ < adiMax_) {
        Rectangle mine = body_->getSurface();
        
        // Expand detection area slightly to include touching objects
        const float touchDistance = 2.0f; // pixels
        Rectangle expandedMine = {
            mine.x - touchDistance,
            mine.y - touchDistance,
            mine.width + 2 * touchDistance,
            mine.height + 2 * touchDistance
        };
        
        for (CollisionRect* other : CollisionRect::query(expandedMine)) {
            if (other->getFather() == this) continue;
            if (!CheckCollisionRecs(expandedMine, other->getSurface())) continue;
            
            // Try to get AdiComponent from the object using optional system
            if (auto adiCompOpt = other->getFather()->getAdiComponent()) {
                AdiComponent* adiComp = *adiCompOpt;
                if (adiComp->canReceiveAdi && adiComp->withdrawAdi(*this)) {
                    break; // retrieve only one per press
                }
            }
        }
    }


    if (InputMap::checkPressed("r3")) { // N key alternative for spawning portal
        Portal::spawnPortalAtPlayer();
    }
    
    if (InputMap::checkPressed("r4")) { // J key for teleporting to spawned portal
        Portal::teleportPlayerToSpawnedPortal();
    }

    current_anim_->routine();
    body_->routine();

    bodySpeed = body_->getSpeed();
    const bool groundedAfterBody = isOnGround();
    const float landDustMinFallSpeed = Particle_m::params().landDustMinFallSpeed;
    if (hadGroundState && !wasGrounded_ && groundedAfterBody && fallSpeedBeforeBody > landDustMinFallSpeed) {
        Particle_m::emitLandDust(body_->getSurface(), gravityDir, impactStrength(fallSpeedBeforeBody, landDustMinFallSpeed));
    }
    wasGrounded_ = groundedAfterBody;

    if (bodySpeed.x == 0 && bodySpeed.y == 0) {
        dashing_ = 0;
    }
    
    // Determine sprite flip based on gravity direction (reuse rigidBody from earlier)
    bool isMoving = false;
    bool shouldFlip = false;
    
    if (rigidBody) {
        switch (rigidBody->getGravityDirection()) {
            case GravityDirection::DOWN:
                // Normal gravity: flip based on horizontal movement
                if (bodySpeed.x < 0) {
                    isMoving = true;
                    shouldFlip = true;
                } else if (bodySpeed.x > 0) {
                    isMoving = true;
                    shouldFlip = false;
                }
                break;
            case GravityDirection::UP:
                // Upside down: flip based on horizontal movement (reversed)
                if (bodySpeed.x < 0) {
                    isMoving = true;
                    shouldFlip = false; // reversed from normal
                } else if (bodySpeed.x > 0) {
                    isMoving = true;
                    shouldFlip = true; // reversed from normal
                }
                break;
            case GravityDirection::LEFT:
                // Left wall gravity: flip based on vertical movement
                if (bodySpeed.y < 0) {
                    isMoving = true;
                    shouldFlip = true; // moving up on wall = facing left
                } else if (bodySpeed.y > 0) {
                    isMoving = true;
                    shouldFlip = false; // moving down on wall = facing right
                }
                break;
            case GravityDirection::RIGHT:
                // Right wall gravity: flip based on vertical movement (reversed)
                if (bodySpeed.y < 0) {
                    isMoving = true;
                    shouldFlip = false; // moving up on wall = facing right
                } else if (bodySpeed.y > 0) {
                    isMoving = true;
                    shouldFlip = true; // moving down on wall = facing left
                }
                break;
        }
    } else {
        // Fallback to old behavior if not RigidBody
        if (bodySpeed.x < 0) {
            isMoving = true;
            shouldFlip = true;
        } else if (bodySpeed.x > 0) {
            isMoving = true;
            shouldFlip = false;
        }
    }
    
    if (isMoving) {
        current_anim_ = anims_["walk"];
        current_anim_->setFlipX(shouldFlip);
    } else {
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

void Character::updateSpriteRotation() {
    // Set sprite rotation based on gravity direction
    if (body_) {
        RigidBody* rigidBody = dynamic_cast<RigidBody*>(body_);
        if (rigidBody) {
            switch (rigidBody->getGravityDirection()) {
                case GravityDirection::DOWN:
                    current_anim_->setRotation(0.0f); // Normal, upright
                    break;
                case GravityDirection::UP:
                    current_anim_->setRotation(180.0f); // Upside down
                    break;
                case GravityDirection::LEFT:
                    current_anim_->setRotation(90.0f); // Rotated so feet point left
                    break;
                case GravityDirection::RIGHT:
                    current_anim_->setRotation(-90.0f); // Rotated so feet point right
                    break;
            }
        }
    }
}

Rectangle Character::spriteRectForBody(Rectangle bodyRect) const {
    // Create sprite rectangle with original dimensions (not rotated hitbox)
    Vector2 center = {
        bodyRect.x + bodyRect.width / 2.0f,
        bodyRect.y + bodyRect.height / 2.0f
    };
    return Rectangle{
        center.x - originalHitboxDims_.x / 2.0f,
        center.y - originalHitboxDims_.y / 2.0f,
        originalHitboxDims_.x,
        originalHitboxDims_.y
    };
}

void Character::draw() {
    updateSpriteRotation();
    current_anim_->draw(spriteRectForBody(body_->getSurface()));
}

void Character::drawAtBody(Rectangle bodyRect) {
    updateSpriteRotation();
    current_anim_->draw(spriteRectForBody(bodyRect));
}

void Character::collectDebugSprites(std::vector<Sprite*>& sprites) {
    for (auto& [name, sprite] : anims_) {
        if (sprite) sprites.push_back(sprite);
    }
}

void Character::onCollision(GObject* other) {
    Water* water = dynamic_cast<Water*>(other);
    if (!water || !body_) return;

    Particle_m::Params& particleParams = Particle_m::params();
    RigidBody* rigidBody = dynamic_cast<RigidBody*>(body_);
    GravityDirection gravityDir = rigidBody ? rigidBody->getGravityDirection() : GravityDirection::DOWN;
    const double now = GetTime();

    if (water->getKind() == WaterVisualKind::Waterfall) {
        if (now - lastWaterfallTouchAt_ >= particleParams.waterfallTouchCooldown) {
            Particle_m::emitWaterfallTouch(body_->getSurface(), water->getRect());
            lastWaterfallTouchAt_ = now;
        }
        return;
    }

    const bool enteringStillWater = !wasTouchingStillWater_;
    lastStillWaterTouchAt_ = now;

    const float fallSpeed = std::max(fallSpeedAlongGravity(rigidBody), lastFallSpeedBeforeMove_);
    if (enteringStillWater &&
        fallSpeed > particleParams.waterSplashMinFallSpeed &&
        now - lastWaterSplashAt_ >= particleParams.waterSplashCooldown) {
        Particle_m::emitWaterSplash(
            body_->getSurface(),
            water->getRect(),
            gravityDir,
            impactStrength(fallSpeed, particleParams.waterSplashMinFallSpeed));
        lastWaterSplashAt_ = now;
    }
}

void Character::respawn() {
    if (body_) {
        body_->setCoord(respawnPos_);
        body_->setSpeed({0,0});
        jumps_ = 0; // reset on respawn
        dashCooldownLeft_ = 0; // dash immediately available on respawn
        groundStateInitialized_ = false;
        wasGrounded_ = false;
        wasTouchingStillWater_ = false;
        lastStillWaterTouchAt_ = -1000.0;
        lastFallSpeedBeforeMove_ = 0.0f;
        lastWaterSplashAt_ = -1000.0;
        lastWaterfallTouchAt_ = -1000.0;
    }
}

void Character::separateFromCollisions() {
    if (Portal::isEntityInTransit(this)) {
        Portal::separateTransitCollisions(this, body_);
        return;
    }

    Rectangle myRect = body_->getSurface();
    const float separationStep = 0.5f;
    const int maxAttempts = 20;
    
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        bool hasCollision = false;
        Vector2 separation = {0, 0};
        
        for (CollisionRect* other : CollisionRect::query(myRect, true)) {
            if (!other->isSolid() || other->getId() == body_->getId()) continue;
            if (Portal::shouldIgnoreTransitCollision(this, other)) continue;
            if (OneWayPlatform::isOneWayPlatformBody(other) &&
                !OneWayPlatform::supportsBody(body_, other)) continue;
            
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
    // Consider grounded if there's a solid in the direction of gravity
    Rectangle probe = body_->getSurface();
    
    RigidBody* rigidBody = dynamic_cast<RigidBody*>(body_);
    GravityDirection gravityDir = rigidBody ? rigidBody->getGravityDirection() : GravityDirection::DOWN;
    
    switch (gravityDir) {
        case GravityDirection::DOWN:
            probe.y += 0.1f; // Check below
            break;
        case GravityDirection::UP:
            probe.y -= 0.1f; // Check above
            break;
        case GravityDirection::LEFT:
            probe.x -= 0.1f; // Check left
            break;
        case GravityDirection::RIGHT:
            probe.x += 0.1f; // Check right
            break;
    }

    if (Portal::isEntityInTransit(const_cast<Character*>(this))) {
        return Portal::isTransitProbeBlocked(const_cast<Character*>(this), body_, probe);
    }
    
    for (CollisionRect* other : CollisionRect::query(probe, true)) {
        if (Portal::shouldIgnoreTransitCollision(const_cast<Character*>(this), other)) continue;
        if (!other || other->getId() == body_->getId()) continue;
        if (OneWayPlatform::isOneWayPlatformBody(other) &&
            !OneWayPlatform::supportsBody(body_, other) &&
            !OneWayPlatform::blocksBody(body_, probe, other)) continue;
        if (other->isSolid())
            return true;
    }
    return false;
}

void Character::updateHitboxRotation() {
    // Rotate hitbox dimensions based on gravity direction
    Rectangle currentHitbox = body_->getSurface();
    Vector2 center = {currentHitbox.x + currentHitbox.width / 2.0f, currentHitbox.y + currentHitbox.height / 2.0f};
    
    Vector2 newDims;
    switch (lastGravityDir_) {
        case GravityDirection::DOWN:
        case GravityDirection::UP:
            // Normal orientation: use original dimensions
            newDims = originalHitboxDims_;
            break;
        case GravityDirection::LEFT:
        case GravityDirection::RIGHT:
            // Rotated 90 degrees: swap width and height
            newDims = {originalHitboxDims_.y, originalHitboxDims_.x};
            break;
    }
    
    // Update hitbox with new dimensions, keeping center position
    Rectangle newHitbox = {
        center.x - newDims.x / 2.0f,
        center.y - newDims.y / 2.0f,
        newDims.x,
        newDims.y
    };
    
    body_->setSurface(newHitbox);
}
