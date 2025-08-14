#pragma once
#include "raylib.h"
#include "gObject.h"
#include "rigidBody.h"

class Raycam {
public:
    Raycam(RigidBody* to_follow = NULL, bool level_bound = false);
    void routine();
    Camera2D& getCam();
    Rectangle getRect();

    RigidBody* to_follow_;
    bool level_bound_;
private:
    Camera2D camera_;
};
