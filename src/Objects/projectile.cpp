#include "projectile.h"
#include "clock.h"
#include "collisionRect.h" // For getting surface
#include "portal_m.h"
#include "rigidBody.h"
#include <algorithm>
#include <cmath>

Projectile::Projectile(const SpawnData& data) : BasicEnt(data) {
    killComponent_ = new KillComponent();
    sourceObjectId_ = data.sourceObjectId.value_or(-1);
    remainingRipples_ = std::max(data.interaction.maxRipple.value_or(0), 0);
    // Replace default CollisionRect with RigidBody
    if (body_) delete body_;
    
    CollisionDesc col = data.physics.collision.value_or(CollisionDesc{});
    BodyDesc bodyDesc = data.physics.body.value_or(BodyDesc{});
    RigidBody* rb = new RigidBody(col, bodyDesc, this);
    rb->setGravityEnabled(false);
    body_ = rb;
}

Projectile::~Projectile() {
    delete killComponent_;
}

void Projectile::routine() {
    BasicEnt::routine();
    if (!body_) return;

    auto* rb = dynamic_cast<RigidBody*>(body_);
    if (!rb) return;

    Rectangle prevSurf = body_->getSurface();
    rb->routine();

    double dt = Clock::getLap();
    age_ += dt;
    if (age_ >= lifetime_) {
        to_delete_ = true;
        return;
    }

    Rectangle targetSurf = body_->getSurface();
    Vector2 move{targetSurf.x - prevSurf.x, targetSurf.y - prevSurf.y};
    if (rb->hadDiscontinuousMovement()) {
        prevSurf = targetSurf;
        move = {0.0f, 0.0f};
    }
    float distance = std::hypot(move.x, move.y);

    auto resolveHitsAt = [&](const Rectangle& fromRect, const Rectangle& probe) -> bool {
        std::vector<CollisionRect*> collisions = CollisionRect::query(probe, false);
        for (auto* other : collisions) {
            if (!other || other == body_) continue;
            GObject* otherFather = other->getFather();
            if (!otherFather) continue;
            if (otherFather->id_ == sourceObjectId_) continue;
            if (Portal_m::shouldIgnoreCollision(this, other)) continue;
            if (!CheckCollisionRecs(probe, other->getSurface())) continue;

            // Always prioritize kill test before generic solid handling.
            if (killComponent_ && killComponent_->onCollision(otherFather)) {
                to_delete_ = true;
                return true;
            }

            if (other->isSolid()) {
                if (remainingRipples_ <= 0) {
                    to_delete_ = true;
                    return true;
                }

                Rectangle wall = other->getSurface();
                bool flipX = false;
                bool flipY = false;

                // Detect axis hit from movement crossing against the wall face.
                if (move.x > 0.0f &&
                    fromRect.x + fromRect.width <= wall.x &&
                    probe.x + probe.width > wall.x) {
                    flipX = true;
                } else if (move.x < 0.0f &&
                           fromRect.x >= wall.x + wall.width &&
                           probe.x < wall.x + wall.width) {
                    flipX = true;
                }

                if (move.y > 0.0f &&
                    fromRect.y + fromRect.height <= wall.y &&
                    probe.y + probe.height > wall.y) {
                    flipY = true;
                } else if (move.y < 0.0f &&
                           fromRect.y >= wall.y + wall.height &&
                           probe.y < wall.y + wall.height) {
                    flipY = true;
                }

                if (!flipX && !flipY) {
                    // Fallback: choose reflection axis from minimum overlap.
                    float overlapLeft = std::fabs((probe.x + probe.width) - wall.x);
                    float overlapRight = std::fabs((wall.x + wall.width) - probe.x);
                    float overlapTop = std::fabs((probe.y + probe.height) - wall.y);
                    float overlapBottom = std::fabs((wall.y + wall.height) - probe.y);
                    float overlapX = std::min(overlapLeft, overlapRight);
                    float overlapY = std::min(overlapTop, overlapBottom);

                    if (overlapX < overlapY) {
                        flipX = true;
                    } else if (overlapY < overlapX) {
                        flipY = true;
                    } else {
                        flipX = true;
                        flipY = true;
                    }
                }

                Vector2 speed = rb->getSpeed();
                if (flipX) speed.x = -speed.x;
                if (flipY) speed.y = -speed.y;
                rb->setSpeed(speed);
                body_->setSurface(fromRect);
                remainingRipples_ = std::max(remainingRipples_ - 1, 0);
                return true;
            }
        }
        return false;
    };

    if (distance <= 0.0f) {
        (void)resolveHitsAt(prevSurf, targetSurf);
        return;
    }

    // Swept collision for fast projectiles to avoid tunneling between frames.
    Vector2 dir{move.x / distance, move.y / distance};
    const float stepSize = 0.1f;
    Rectangle lastFree = prevSurf;
    float travelled = 0.0f;

    while (travelled < distance) {
        float advance = std::min(stepSize, distance - travelled);
        Rectangle probe = lastFree;
        probe.x += dir.x * advance;
        probe.y += dir.y * advance;
        if (resolveHitsAt(lastFree, probe)) return;
        lastFree = probe;
        travelled += advance;
    }

    body_->setSurface(lastFree);
}
