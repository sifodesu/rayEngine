#pragma once
#include <string>
#include <tuple>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "bullet.h"

enum BulletType {
    RIGID,
    ZIGZAG,
    EXPLOSIVE
};

class Bullet_m {
public:
    static void init();
    static void routine();
    static void draw();
    static Bullet* createBullet(BulletType type, std::unordered_set<GObject*> no_dmg, double delay = 0);
    static void destroyBullet(Bullet* toDestroy, BulletType type);

    static std::unordered_map<BulletType, std::unordered_set<Bullet*>> pool;
    static std::unordered_map<BulletType, std::unordered_set<Bullet*>> active_bullets;

    static std::queue<std::tuple<double, BulletType, Bullet*>> waiting_bullets;
};
