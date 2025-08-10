#include "rigidBody.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

//TODO: handle case when out of box quad
RigidBody::RigidBody(const CollisionDesc& col, const BodyDesc& body, GObject* father)
    : CollisionRect(col, father) {
    speed_ = body.speed;
    acceleration_ = body.acceleration;
    curve_ = body.curve;
    mass_ = body.gravityAcceleration;
}


void RigidBody::setCurve(double curve) {
    curve_ = curve;
}
void RigidBody::setAcceleration(double acc) {
    acceleration_ = acc;
}

void RigidBody::setSpeed(Vector2 speed) {
    speed_ = speed;
}
Vector2 RigidBody::getSpeed() {
    return speed_;
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
        for (CollisionRect* body : query(probeRect, true, true)) {
            if (body->isSolid() && (body->getId() != pool_id_)) { speed_.x = 0.0f; break; }
        }
        if (speed_.x != 0.0f) {
            for (CollisionRect* body : query(probeRect, false, true)) {
                if (body->isSolid() && (body->getId() != pool_id_)) { speed_.x = 0.0f; break; }
            }
        }
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
        for (CollisionRect* body : query(probeRect, true, true)) {
            if (body->isSolid() && (body->getId() != pool_id_)) { speed_.y = 0.0f; break; }
        }
        if (speed_.y != 0.0f) {
            for (CollisionRect* body : query(probeRect, false, true)) {
                if (body->isSolid() && (body->getId() != pool_id_)) { speed_.y = 0.0f; break; }
            }
        }
    }
}

void RigidBody::routine() {
    float delta = (float)Clock::getLap();
    
    if (delta > 0.2)
        return;

    if (mass_)
        speed_.y += mass_ * delta;

    if (std::fabs(speed_.x) < FLT_EPSILON && std::fabs(speed_.y) < FLT_EPSILON)
        return;

    float tempX = std::cos(curve_ * delta) * speed_.x - std::sin(curve_ * delta) * speed_.y;
    float tempY = std::sin(curve_ * delta) * speed_.x + std::cos(curve_ * delta) * speed_.y;
    speed_ = { tempX, tempY };

    fixSpeed();

    remove();

    // Move forward incrementally until a collision would occur, then stop at last free position
    const float maxDistance = static_cast<float>(std::hypot(speed_.x * delta, speed_.y * delta));
    if (maxDistance > 0.0f) {
        Vector2 unitDir = { (speed_.x * delta) / maxDistance, (speed_.y * delta) / maxDistance };
        const float stepSize = 0.1f; // world units per step
        const Rectangle startRect = getSurface();
        Rectangle lastFree = startRect;

        auto collides = [&](const Rectangle &rect) -> bool {
            // Check against static
            for (CollisionRect* body : query(rect, true, true)) {
                if (body->isSolid() && solid_ && (body->getId() != pool_id_))
                    return true;
            }
            // And dynamic
            for (CollisionRect* body : query(rect, false, true)) {
                if (body->isSolid() && solid_ && (body->getId() != pool_id_))
                    return true;
            }
            return false;
        };

        float travelled = 0.0f;
        while (travelled < maxDistance) {
            float advance = std::min(stepSize, maxDistance - travelled);
            Rectangle next = lastFree;
            next.x += unitDir.x * advance;
            next.y += unitDir.y * advance;

            if (collides(next)) {
                break; // stop right before collision
            }
            lastFree = next;
            travelled += advance;
        }

        setSurface(lastFree);
    }

    add();
    speed_.x += acceleration_ * delta * speed_.x;
    speed_.y += acceleration_ * delta * speed_.y;
}


