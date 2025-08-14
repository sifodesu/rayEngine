#include "raycam.h"
#include "rigidBody.h"
#include "raymath.h"
#include <cmath>
Raycam::Raycam(RigidBody* to_follow, bool level_bound) : to_follow_(to_follow), camera_{}, level_bound_(level_bound) {
    camera_.offset = { (float) GetScreenWidth() / 2.0f, (float) GetScreenHeight() / 2.0f };
    if (to_follow_)
        camera_.target = to_follow_->getCoord();
    camera_.zoom = (float) GetScreenWidth() / (480.0f);
}

Camera2D& Raycam::getCam() {
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
        auto coords = to_follow_->getCenterCoord();
        if (level_bound_) {
            // Snap camera to the logical 480x360 grid so the view's top-left is the grid's top-left
            const float cellW = 480.0f;
            const float cellH = 360.0f;
            float gx = floorf(coords.x / cellW);
            float gy = floorf(coords.y / cellH);
            // Camera2D.target is the world-space point at screen 'offset' (center). To align top-left to grid,
            // set target to the center of the grid cell: topLeft + half cell size.
            camera_.target = {
                gx * cellW + cellW * 0.5f,
                gy * cellH + cellH * 0.5f
            };
        }
        else {
            camera_.target = coords;
        }
    }
    else {
        // camera_.target = camera_.offset;
    }
}