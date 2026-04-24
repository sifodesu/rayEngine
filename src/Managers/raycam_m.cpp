#include "raycam_m.h"
#include "definitions.h"
#include <algorithm>
#include <cmath>

Raycam Raycam_m::camera_;
Camera3D Raycam_m::camera3d_ = {};

void Raycam_m::sync3DFrom2D() {
    Camera2D cam2d = camera_.getCam();
    const float zoom = std::max(cam2d.zoom, 0.001f);
    const float viewHeight = static_cast<float>(NATIVE_RES_HEIGHT) / zoom;
    constexpr float k3DCameraRecoil = 420.0f;
    constexpr float kRadToDeg = 57.29577951308232f;

    camera3d_.position = {
        cam2d.target.x,
        k3DCameraRecoil,
        cam2d.target.y
    };
    camera3d_.target = { cam2d.target.x, 0.0f, cam2d.target.y };
    camera3d_.up = { 0.0f, 0.0f, -1.0f };
    camera3d_.fovy = 2.0f * std::atan(viewHeight / (2.0f * k3DCameraRecoil)) * kRadToDeg;
    camera3d_.projection = CAMERA_PERSPECTIVE;
}
