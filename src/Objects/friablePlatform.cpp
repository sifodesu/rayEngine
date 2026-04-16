#include "friablePlatform.h"

#include <algorithm>
#include <cmath>

#include "character.h"
#include "clock.h"
#include "collisionRect.h"
#include "rigidBody.h"

namespace {
float axisOverlap(float aMin, float aMax, float bMin, float bMax) {
    return std::min(aMax, bMax) - std::max(aMin, bMin);
}
}

FriablePlatform::FriablePlatform(const SpawnData& data) : BasicEnt(data) {
    breakTime_ = data.interaction.breakTime.value_or(1.0f);
    breakTime_ = std::max(breakTime_, 0.01f);

    if (sprite_) {
        sprite_->freeze(true);
        sprite_->resetAnimation();
    }
}

void FriablePlatform::routine() {
    BasicEnt::routine();

    if (to_delete_ || !body_) return;

    if (!breaking_) {
        if (!detectStandingCharacter()) return;
        startBreaking();
    }

    breakElapsed_ += static_cast<float>(Clock::getLap());
    if (breakElapsed_ >= breakTime_) {
        to_delete_ = true;
    }
}

void FriablePlatform::onCollision(GObject* other) {
    if (breaking_ || !other) return;
    auto* chr = dynamic_cast<Character*>(other);
    if (!chr) return;
    if (isCharacterStandingOn(*chr)) {
        startBreaking();
    }
}

bool FriablePlatform::isCharacterStandingOn(const Character& character) const {
    if (!body_ || !character.body_) return false;

    Rectangle platformRect = body_->getSurface();
    Rectangle charRect = character.body_->getSurface();

    float xOverlap = axisOverlap(charRect.x, charRect.x + charRect.width,
                                 platformRect.x, platformRect.x + platformRect.width);
    float yOverlap = axisOverlap(charRect.y, charRect.y + charRect.height,
                                 platformRect.y, platformRect.y + platformRect.height);

    GravityDirection gravityDir = character.body_->getGravityDirection();
    switch (gravityDir) {
        case GravityDirection::DOWN: {
            float feetToTop = std::fabs((charRect.y + charRect.height) - platformRect.y);
            return xOverlap > 0.5f && feetToTop <= contactTolerance_;
        }
        case GravityDirection::UP: {
            float headToBottom = std::fabs(charRect.y - (platformRect.y + platformRect.height));
            return xOverlap > 0.5f && headToBottom <= contactTolerance_;
        }
        case GravityDirection::LEFT: {
            float leftToRight = std::fabs(charRect.x - (platformRect.x + platformRect.width));
            return yOverlap > 0.5f && leftToRight <= contactTolerance_;
        }
        case GravityDirection::RIGHT: {
            float rightToLeft = std::fabs((charRect.x + charRect.width) - platformRect.x);
            return yOverlap > 0.5f && rightToLeft <= contactTolerance_;
        }
    }
    return false;
}

bool FriablePlatform::detectStandingCharacter() const {
    if (!body_) return false;

    Rectangle probe = body_->getSurface();
    probe.x -= contactTolerance_;
    probe.y -= contactTolerance_;
    probe.width += 2.0f * contactTolerance_;
    probe.height += 2.0f * contactTolerance_;

    for (CollisionRect* rect : CollisionRect::query(probe)) {
        if (!rect || rect == body_) continue;
        auto* chr = dynamic_cast<Character*>(rect->getFather());
        if (!chr) continue;
        if (isCharacterStandingOn(*chr)) return true;
    }
    return false;
}

void FriablePlatform::startBreaking() {
    if (breaking_) return;

    breaking_ = true;
    breakElapsed_ = 0.0f;

    if (!sprite_) return;

    sprite_->resetAnimation();
    int frameCount = std::max(sprite_->getFrameCount(), 1);
    float perFrameDuration = breakTime_ / static_cast<float>(frameCount);
    sprite_->setForcedUniformFrameDuration(perFrameDuration);
    sprite_->freeze(false);
}
