#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include "gObject.h"
#include "sprite.h"
#include "rigidBody.h"
#include "clock.h"

class Bullet : public GObject {
public:
    Bullet(nlohmann::json obj);
    ~Bullet();
    void draw();
    void routine();
    void setTargets(std::unordered_set<HObject*> targets);

    Sprite* sprite_;
    RigidBody* body_;
    Clock clock_;

    double ttl_;
private:
    std::unordered_set<HObject*> targets_;
    double dmg_;
};