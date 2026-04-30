#include "oneWayPlatform.h"

namespace {
constexpr float kSupportTolerance = 0.5f;
constexpr float kMotionEpsilon = 0.001f;
}

OneWayPlatform::OneWayPlatform(const SpawnData& data) : BasicEnt(data) {
    if (body_) body_->setSolid(true);
}

bool OneWayPlatform::isOneWayPlatformBody(CollisionRect* body) {
    return body && dynamic_cast<OneWayPlatform*>(body->getFather());
}

bool OneWayPlatform::supportsBody(CollisionRect* movingBody, CollisionRect* platformBody) {
    if (!movingBody || !isOneWayPlatformBody(platformBody)) return false;

    Rectangle currentRect = movingBody->getSurface();
    Rectangle platformRect = platformBody->getSurface();

    float currentBottom = currentRect.y + currentRect.height;
    float platformTop = platformRect.y;
    bool overlapsX = currentRect.x < platformRect.x + platformRect.width &&
                     currentRect.x + currentRect.width > platformRect.x;

    return overlapsX &&
           currentRect.y < platformTop &&
           currentBottom <= platformTop + kSupportTolerance;
}

bool OneWayPlatform::blocksBody(CollisionRect* movingBody, Rectangle proposedRect, CollisionRect* platformBody) {
    if (!movingBody || !isOneWayPlatformBody(platformBody)) return false;

    Rectangle currentRect = movingBody->getSurface();
    Rectangle platformRect = platformBody->getSurface();

    float currentBottom = currentRect.y + currentRect.height;
    float proposedBottom = proposedRect.y + proposedRect.height;
    float platformTop = platformRect.y;

    return proposedBottom > currentBottom + kMotionEpsilon &&
           currentBottom <= platformTop + kSupportTolerance &&
           proposedBottom >= platformTop - kSupportTolerance;
}
