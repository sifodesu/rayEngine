#include "projectile.h"
#include "clock.h"
#include "collisionRect.h" // For getting surface
#include "rigidBody.h"

Projectile::Projectile(const SpawnData& data) : BasicEnt(data) {
    killComponent_ = new KillComponent();
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
    std::vector<CollisionRect*> collisions = CollisionRect::query(surf, true);
    for (auto* other : collisions) {
        if (other == body_) continue;
        if (!CheckCollisionRecs(surf, other->getSurface())) continue;

        if (other->isSolid()) {
            // Hit wall -> destroy
            to_delete_ = true;
            return;
        }
        
        // Check for killable targets (Character) via KillComponent
        if (killComponent_->onCollision(other->getFather())) {
            to_delete_ = true;
            return;
        }
    }
}
