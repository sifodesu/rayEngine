#include "raycam.h"
#include "rigidBody.h"

Raycam::Raycam(int resX, int resY, RigidBody* to_follow) : camera_({ 0 }), to_follow_(to_follow) {
    camera_.offset = (Vector2){ resX / 2.0f, resY / 2.0f };
    if (to_follow_)
        camera_.target = to_follow_->getCoord();
    camera_.zoom = 4.0f;
}

Camera2D Raycam::getCam() {
    return camera_;
}

Rectangle Raycam::getRect() {
    if (to_follow_)
        return Rectangle{ camera_.target.x - camera_.offset.x, camera_.target.y - camera_.offset.y,
        (camera_.offset.x) * 2, (camera_.offset.y) * 2 };

    else
        return Rectangle{ 0.0f, 0.0f,
        (camera_.offset.x) * 2, (camera_.offset.y) * 2 };
}

void Raycam::routine() {
    if (to_follow_) {
        auto coords = to_follow_->getCoord();
        camera_.target = coords;
    }
    else {
        // camera_.target = camera_.offset;
    }
}