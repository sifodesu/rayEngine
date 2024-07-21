#pragma once
#include "raylib.h"
#include "gObject.h"
#include "rigidBody.h"

class Raycam {
public:
    Raycam(RigidBody* to_follow = NULL);
    void routine();
    Camera2D& getCam();
    Rectangle getRect();

    RigidBody* to_follow_;
private:
    Camera2D camera_;
};
