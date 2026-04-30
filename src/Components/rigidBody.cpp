#include "rigidBody.h"
#include "portal.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

//TODO: handle case when out of box quad
RigidBody::RigidBody(const CollisionDesc& col, const BodyDesc& body, GObject* father)
    : CollisionRect(col, father), speed_(body.speed), acceleration_(body.acceleration), 
      curve_(body.curve), mass_(body.gravityAcceleration),
      gravityEnabled_(true), gravityDirection_(GravityDirection::DOWN) {
    applyMaxFallSpeed();
}

void RigidBody::setCurve(double curve) {
    curve_ = curve;
}
void RigidBody::setAcceleration(double acc) {
    acceleration_ = acc;
}

void RigidBody::setSpeed(Vector2 speed) {
    speed_ = speed;
    applyMaxFallSpeed();
}
Vector2 RigidBody::getSpeed() {
    return speed_;
}

void RigidBody::setGravityDirection(GravityDirection dir) {
    gravityDirection_ = dir;
    applyMaxFallSpeed();
}

void RigidBody::setMaxFallSpeedEnabled(bool enabled) {
    maxFallSpeedEnabled_ = enabled;
    applyMaxFallSpeed();
}

void RigidBody::setMaxFallSpeed(double speed) {
    maxFallSpeed_ = std::max(1.0, speed);
    applyMaxFallSpeed();
}

double RigidBody::getFallSpeedAlongGravity() const {
    switch (gravityDirection_) {
        case GravityDirection::DOWN:
            return speed_.y;
        case GravityDirection::UP:
            return -speed_.y;
        case GravityDirection::LEFT:
            return -speed_.x;
        case GravityDirection::RIGHT:
            return speed_.x;
    }
    return 0.0;
}

void RigidBody::applyMaxFallSpeed() {
    if (!maxFallSpeedEnabled_) return;
    if (getFallSpeedAlongGravity() <= maxFallSpeed_) return;

    switch (gravityDirection_) {
        case GravityDirection::DOWN:
            speed_.y = maxFallSpeed_;
            break;
        case GravityDirection::UP:
            speed_.y = -maxFallSpeed_;
            break;
        case GravityDirection::LEFT:
            speed_.x = -maxFallSpeed_;
            break;
        case GravityDirection::RIGHT:
            speed_.x = maxFallSpeed_;
            break;
    }
}

void RigidBody::registerSweepContactOwner(GObject* owner) {
    if (!owner || owner == father_) return;
    for (GObject* existing : sweepContactOwners_) {
        if (existing == owner) return;
    }
    sweepContactOwners_.push_back(owner);
}

void RigidBody::addSweepContacts(const Rectangle& probeRect) {
    std::vector<CollisionRect*> candidates = CollisionRect::query(probeRect);
    for (CollisionRect* body : candidates) {
        if (!body || body == this) continue;
        if (!CheckCollisionRecs(probeRect, body->getSurface())) continue;
        // Portal transit proxy bodies can be destroyed before Object_m consumes
        // swept contacts, so keep the stable owner instead of the body pointer.
        registerSweepContactOwner(body->getFather());
    }
}

void RigidBody::fixSpeed() {
    if (!solid_) return;

    // Probe slightly ahead in X
    Rectangle probeRect = getSurface();
    if (speed_.x > 0.0f) {
        probeRect.width += 0.1f;
    } else if (speed_.x < 0.0f) {
        probeRect.x -= 0.1f;
        probeRect.width += 0.1f;
    }
    if (speed_.x != 0.0f) {
        addSweepContacts(probeRect);
        if (Portal::isMovementBlocked(father_, this, probeRect)) speed_.x = 0.0f;
    }

    // Probe slightly ahead in Y
    probeRect = getSurface();
    if (speed_.y > 0.0f) {
        probeRect.height += 0.1f;
    } else if (speed_.y < 0.0f) {
        probeRect.y -= 0.1f;
        probeRect.height += 0.1f;
    }
    if (speed_.y != 0.0f) {
        addSweepContacts(probeRect);
        if (Portal::isMovementBlocked(father_, this, probeRect)) speed_.y = 0.0f;
    }
}

void RigidBody::routine() {
    float delta = (float)Clock::getLap();
    sweepContactOwners_.clear();
    discontinuousMovement_ = false;
    
    if (delta > 0.2)
        return;

    bool inPortalTransit = Portal::isEntityInTransit(father_);
    if (inPortalTransit) {
        Portal::separateTransitCollisions(father_, this);
    }

    // Gravity must keep running through portal transit; otherwise traversal
    // duration changes the height reached after exiting.
    if (gravityEnabled_) {
        if (auto transitGravityStep = Portal::getTransitGravityStep(father_, gravityDirection_, mass_, delta)) {
            speed_.x += transitGravityStep->x;
            speed_.y += transitGravityStep->y;
        } else {
            double gravityStep = mass_ * delta;
            switch (gravityDirection_) {
                case GravityDirection::DOWN:
                    speed_.y += gravityStep;
                    break;
                case GravityDirection::UP:
                    speed_.y -= gravityStep;
                    break;
                case GravityDirection::LEFT:
                    speed_.x -= gravityStep;
                    break;
                case GravityDirection::RIGHT:
                    speed_.x += gravityStep;
                    break;
            }
        }
    }

    if (std::fabs(speed_.x) < FLT_EPSILON && std::fabs(speed_.y) < FLT_EPSILON)
        return;

    float tempX = std::cos(curve_ * delta) * speed_.x - std::sin(curve_ * delta) * speed_.y;
    float tempY = std::sin(curve_ * delta) * speed_.x + std::cos(curve_ * delta) * speed_.y;
    speed_ = { tempX, tempY };
    applyMaxFallSpeed();

    fixSpeed();

    remove();

    // Move forward incrementally until a collision would occur, then stop at last free position
    const float maxDistance = static_cast<float>(std::hypot(speed_.x * delta, speed_.y * delta));
    if (maxDistance > 0.0f) {
        Vector2 unitDir = { (speed_.x * delta) / maxDistance, (speed_.y * delta) / maxDistance };
        const float stepSize = 0.1f; // world units per step
        const Rectangle startRect = getSurface();
        Rectangle lastFree = startRect;

        float travelled = 0.0f;
        while (travelled < maxDistance) {
            float advance = std::min(stepSize, maxDistance - travelled);
            Rectangle next = lastFree;
            next.x += unitDir.x * advance;
            next.y += unitDir.y * advance;
            addSweepContacts(next);

            Portal::prepareMovement(father_, this, lastFree, next);
            if (Portal::isMovementBlocked(father_, this, next)) {
                break; // stop right before collision
            }
            lastFree = next;
            travelled += advance;
        }

        setSurface(lastFree);
        discontinuousMovement_ = Portal::syncTransit(father_);
    }

    add();
    speed_.x += acceleration_ * delta * speed_.x;
    speed_.y += acceleration_ * delta * speed_.y;
    applyMaxFallSpeed();
}
