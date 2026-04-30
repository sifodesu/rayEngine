#include "oneWayPlatform.h"

#include "rigidBody.h"

namespace {
constexpr float kSupportTolerance = 0.5f;
constexpr float kMotionEpsilon = 0.001f;

bool overlapsHorizontal(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width && a.x + a.width > b.x;
}

bool overlapsVertical(Rectangle a, Rectangle b) {
    return a.y < b.y + b.height && a.y + a.height > b.y;
}

GravityDirection gravityDirectionFor(CollisionRect* body) {
    if (auto* rigidBody = dynamic_cast<RigidBody*>(body)) {
        return rigidBody->getGravityDirection();
    }
    return GravityDirection::DOWN;
}

bool movingTowardPositive(float currentLeading, float proposedLeading, float supportPlane) {
    return proposedLeading > currentLeading + kMotionEpsilon &&
           currentLeading <= supportPlane + kSupportTolerance &&
           proposedLeading >= supportPlane - kSupportTolerance;
}

bool movingTowardNegative(float currentLeading, float proposedLeading, float supportPlane) {
    return proposedLeading < currentLeading - kMotionEpsilon &&
           currentLeading >= supportPlane - kSupportTolerance &&
           proposedLeading <= supportPlane + kSupportTolerance;
}
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

    switch (gravityDirectionFor(movingBody)) {
        case GravityDirection::DOWN: {
            float currentBottom = currentRect.y + currentRect.height;
            float platformTop = platformRect.y;
            return overlapsHorizontal(currentRect, platformRect) &&
                   currentRect.y < platformTop &&
                   currentBottom <= platformTop + kSupportTolerance;
        }
        case GravityDirection::UP: {
            float currentTop = currentRect.y;
            float platformBottom = platformRect.y + platformRect.height;
            return overlapsHorizontal(currentRect, platformRect) &&
                   currentRect.y + currentRect.height > platformBottom &&
                   currentTop >= platformBottom - kSupportTolerance;
        }
        case GravityDirection::LEFT: {
            float currentLeft = currentRect.x;
            float platformRight = platformRect.x + platformRect.width;
            return overlapsVertical(currentRect, platformRect) &&
                   currentRect.x + currentRect.width > platformRight &&
                   currentLeft >= platformRight - kSupportTolerance;
        }
        case GravityDirection::RIGHT: {
            float currentRight = currentRect.x + currentRect.width;
            float platformLeft = platformRect.x;
            return overlapsVertical(currentRect, platformRect) &&
                   currentRect.x < platformLeft &&
                   currentRight <= platformLeft + kSupportTolerance;
        }
    }
    return false;
}

bool OneWayPlatform::blocksBody(CollisionRect* movingBody, Rectangle proposedRect, CollisionRect* platformBody) {
    if (!movingBody || !isOneWayPlatformBody(platformBody)) return false;

    Rectangle currentRect = movingBody->getSurface();
    Rectangle platformRect = platformBody->getSurface();

    switch (gravityDirectionFor(movingBody)) {
        case GravityDirection::DOWN: {
            float currentBottom = currentRect.y + currentRect.height;
            float proposedBottom = proposedRect.y + proposedRect.height;
            float platformTop = platformRect.y;
            return overlapsHorizontal(proposedRect, platformRect) &&
                   movingTowardPositive(currentBottom, proposedBottom, platformTop);
        }
        case GravityDirection::UP: {
            float currentTop = currentRect.y;
            float proposedTop = proposedRect.y;
            float platformBottom = platformRect.y + platformRect.height;
            return overlapsHorizontal(proposedRect, platformRect) &&
                   movingTowardNegative(currentTop, proposedTop, platformBottom);
        }
        case GravityDirection::LEFT: {
            float currentLeft = currentRect.x;
            float proposedLeft = proposedRect.x;
            float platformRight = platformRect.x + platformRect.width;
            return overlapsVertical(proposedRect, platformRect) &&
                   movingTowardNegative(currentLeft, proposedLeft, platformRight);
        }
        case GravityDirection::RIGHT: {
            float currentRight = currentRect.x + currentRect.width;
            float proposedRight = proposedRect.x + proposedRect.width;
            float platformLeft = platformRect.x;
            return overlapsVertical(proposedRect, platformRect) &&
                   movingTowardPositive(currentRight, proposedRight, platformLeft);
        }
    }
    return false;
}
