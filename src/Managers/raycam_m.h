#pragma once
#include "raycam.h"
#include "rigidBody.h"

class Raycam_m
{
public:
    static void init() { camera_ = Raycam(); };
    static Camera2D &getCam() { return camera_.getCam(); }
    static Raycam &getRayCam() { return camera_; }
    static RigidBody *getTarget() { return camera_.to_follow_; }
    static void setTarget(RigidBody *_target) { camera_.to_follow_ = _target; }

private:
    static Raycam camera_;
};