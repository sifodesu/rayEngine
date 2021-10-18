#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <queue>
#include <functional>

#include "character.h"
#include "hObject.h"
#include "bullet.h"
#include "clock.h"
#include "rigidBullet.h"

class SimpleBoss : public HObject {
public:
    SimpleBoss(nlohmann::json obj);
    ~SimpleBoss();
    void routine();
    void draw();

    void die();
    void youpi();

private:
    RigidBody* body_;
    Sprite* sprite_;
    std::queue<std::tuple<double, std::function<void()>>> patterns_;
    std::queue<Bullet*> bpq_;
    Clock clock_;
    double time_;

    Character* chara_;
    Vector2 getDir();
    Vector2 getRandomDir(double magnitude);
    RigidBullet* createBasicRB();
};