#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
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
    void trigger() {}


    Sprite* sprite_;
    RigidBody* body_;
    Clock clock_;
    double ttl_;
};