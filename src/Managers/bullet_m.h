#pragma once
#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>

#include "bullet.h"

class Bullet_m {
public:
    static void init();
    static void routine();
    static void draw();
    static Bullet* createBullet();
    static void destroyBullet(Bullet* toDestroy);

    static std::unordered_set<Bullet*> pool;
    static std::unordered_set<Bullet*> active_bullets;
};