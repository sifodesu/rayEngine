#pragma once
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "bullet.h"

enum BulletType {
    RIGID,
    ZIGZAG
};

class Bullet_m {
public:
    static void init();
    static void routine();
    static void draw();
    static Bullet* createBullet(BulletType type, std::unordered_set<GObject*> no_dmg);
    static void destroyBullet(Bullet* toDestroy, BulletType type);

    static std::unordered_map<BulletType, std::unordered_set<Bullet*>> pool;
    static std::unordered_map<BulletType, std::unordered_set<Bullet*>> active_bullets;
};