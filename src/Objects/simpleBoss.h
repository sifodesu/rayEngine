#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <queue>
#include <functional>

#include "hObject.h"
#include "bullet.h"
#include "clock.h"

class SimpleBoss : public HObject {
public:
    SimpleBoss(nlohmann::json obj);
    void routine();
    void draw();
    void shoot();

private:
    RigidBody* body_;
    Sprite* sprite_;
    std::queue<std::tuple<double, std::function<void()>>> patterns_;
    Clock clock_;
};