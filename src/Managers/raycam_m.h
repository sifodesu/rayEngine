#pragma once
#include "raycam.h"
#include "rigidBody.h"

class Raycam_m
{
public:
    static void init() { camera_ = Raycam(); sync3DFrom2D(); };
    static Camera2D &getCam() { return camera_.getCam(); }
    static Camera3D &getCam3D() { sync3DFrom2D(); return camera3d_; }
    static Raycam &getRayCam() { return camera_; }
    static RigidBody *getTarget() { return camera_.to_follow_; }
    static void setTarget(RigidBody *_target, bool _level_bound) { camera_.to_follow_ = _target; camera_.level_bound_ = _level_bound; }

private:
    static void sync3DFrom2D();
    static Raycam camera_;
    static Camera3D camera3d_;
};
