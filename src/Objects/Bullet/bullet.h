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
    virtual void setCoord(Vector2 pos);
    void setNoDmg(std::unordered_set<GObject*> no_dmg);

    Bullet& operator=(const Bullet& other) {
        *sprite_ = *other.sprite_;
        ttl_ = other.ttl_;
        pos_ = other.pos_;
        no_dmg_ = other.no_dmg_;
        dmg_ = other.dmg_;
        return *this;
    }

    Sprite* sprite_;
    double ttl_;
    Vector2 pos_;
    std::unordered_set<GObject*> no_dmg_;
    double dmg_;
};