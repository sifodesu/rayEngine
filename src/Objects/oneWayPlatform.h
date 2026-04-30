#pragma once

#include "basicEnt.h"

class OneWayPlatform : public BasicEnt {
public:
    explicit OneWayPlatform(const SpawnData& data);

    static bool isOneWayPlatformBody(CollisionRect* body);
    static bool supportsBody(CollisionRect* movingBody, CollisionRect* platformBody);
    static bool blocksBody(CollisionRect* movingBody, Rectangle proposedRect, CollisionRect* platformBody);
};
