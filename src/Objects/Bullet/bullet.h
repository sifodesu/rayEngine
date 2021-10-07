#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <unordered_set>

#include "sprite.h"
#include "rigidBody.h"
#include "clock.h"

class Bullet : public GObject {
public:
    Bullet(nlohmann::json obj);
    ~Bullet();
    void draw();
    void routine();
    void setCoord(Vector2 pos);
    void setNoDmg(std::unordered_set<GObject*> no_dmg);

    Sprite* sprite_;
    Clock clock_;
    double ttl_;
    Vector2 pos_;
    std::unordered_set<GObject*> no_dmg_;
    double dmg_;
};