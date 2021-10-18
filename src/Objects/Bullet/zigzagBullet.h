#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <unordered_set>

#include "bullet.h"

class ZigzagBullet : public Bullet {
public:
    ZigzagBullet(nlohmann::json obj) : Bullet(obj) {
        speed_ = { 0, -50 };
        sin_speed_ = { 10, 0 };
        amplitude_ = 0.5;
        sin_arg_ = 0;
    }
    // ~ZigzagBullet() {
        // Bullet::~Bullet();
    // }
    void routine() {
        double t = Clock::getLap();
        Vector2 pos = pos_;
        pos_.y += sin(sin_arg_ * sin_speed_.y) * amplitude_ + speed_.y * t;
        pos_.x += sin(sin_arg_ * sin_speed_.x) * amplitude_ + speed_.x * t;
        
        Bullet::routine();
        sin_arg_ += t;
    }
    void setSpeed(Vector2 speed) { speed_ = speed; }
    void setSinSpeed(Vector2 sinSpeed) { sin_speed_ = sinSpeed; }
    void setAmplitude(double amplitude) { amplitude_ = amplitude; }

private:
    Vector2 speed_;
    Vector2 sin_speed_;
    double amplitude_;
    double sin_arg_;
};