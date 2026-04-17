#include "projectile.h"
#include "clock.h"
#include "collisionRect.h" // For getting surface
#include "rigidBody.h"
#include <cmath>

Projectile::Projectile(const SpawnData& data) : BasicEnt(data) {
    killComponent_ = new KillComponent();
    sourceObjectId_ = data.sourceObjectId.value_or(-1);
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

    Rectangle prevSurf = body_->getSurface();

    if (auto* rb = dynamic_cast<RigidBody*>(body_)) {
        rb->routine();
    }

    double dt = Clock::getLap();
    age_ += dt;
    if (age_ >= lifetime_) {
        to_delete_ = true;
        return;
    }

    Rectangle surf = body_->getSurface();

    auto resolveHitsAt = [&](const Rectangle& probe) -> bool {
        std::vector<CollisionRect*> collisions = CollisionRect::query(probe);
        for (auto* other : collisions) {
            if (!other || other == body_) continue;
            GObject* otherFather = other->getFather();
            if (!otherFather) continue;
            if (otherFather->id_ == sourceObjectId_) continue;
            if (!CheckCollisionRecs(probe, other->getSurface())) continue;

            // Always prioritize kill test before generic solid handling.
            if (killComponent_ && killComponent_->onCollision(otherFather)) {
                to_delete_ = true;
                return true;
            }

            if (other->isSolid()) {
                // Hit wall/solid object -> destroy
                to_delete_ = true;
                return true;
            }
        }
        return false;
    };

    if (resolveHitsAt(surf)) return;

    // Swept collision for fast projectiles to avoid tunneling between frames.
    Vector2 motion{surf.x - prevSurf.x, surf.y - prevSurf.y};
    float distance = std::hypot(motion.x, motion.y);
    if (distance <= 0.0f) return;

    Vector2 dir{motion.x / distance, motion.y / distance};
    const float stepSize = 0.1f;
    Rectangle probe = prevSurf;
    float travelled = 0.0f;

    while (travelled < distance) {
        float advance = std::min(stepSize, distance - travelled);
        probe.x += dir.x * advance;
        probe.y += dir.y * advance;
        if (resolveHitsAt(probe)) return;
        travelled += advance;
    }
}
