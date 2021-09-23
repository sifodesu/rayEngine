#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "gObject.h"
#include "bullet.h"
#include <queue>
#include <functional>
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