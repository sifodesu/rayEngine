#include <iostream>
#include "bullet_m.h"
#include "object_m.h"
#include "rigidBullet.h"
#include "zigzagBullet.h"
#include "definitions.h"
#define POOL_SIZE 10000

using namespace std;

unordered_map<BulletType, unordered_set<Bullet*>> Bullet_m::pool;
unordered_map<BulletType, unordered_set<Bullet*>> Bullet_m::active_bullets;

void Bullet_m::init() {
    for (BulletType type : { RIGID, ZIGZAG }) {
        for (int i = 0; i < POOL_SIZE; i++) {
            if (type == RIGID) {
                RigidBullet* b = new RigidBullet(Object_m::blueprints_[BULLET]);
                pool[type].insert(b);
            }

            if (type == ZIGZAG) {
                ZigzagBullet* b = new ZigzagBullet(Object_m::blueprints_[BULLET]);
                pool[type].insert(b);
            }
        }

    }
}

void Bullet_m::routine() {
    vector<std::tuple<Bullet*, BulletType>> toDestroy;
    for (auto [type, bullets] : active_bullets) {
        for (auto bullet : bullets) {
            bullet->routine();
            if (bullet->ttl_ < 0) {
                toDestroy.push_back(std::make_tuple(bullet, type));
            }
        }
    }
    for (auto [bullet, type] : toDestroy) {
        destroyBullet(bullet, type);
    }
}

void Bullet_m::draw() {
    for (auto [type, bullets] : active_bullets) {
        for (auto bullet : bullets) {
            bullet->draw();
        }
    }
}

Bullet* Bullet_m::createBullet(BulletType type, unordered_set<GObject*> no_dmg) {
    if (!pool[type].empty()) {
        Bullet* newBullet = *pool[type].begin();
        pool[type].erase(newBullet);
        active_bullets[type].insert(newBullet);

        newBullet->ttl_ = 2;
        newBullet->to_delete_ = false;
        newBullet->clock_.getLap();
        newBullet->no_dmg_ = no_dmg;
        return newBullet;
    }
    
    return *active_bullets[type].begin();
}

void Bullet_m::destroyBullet(Bullet* toDestroy, BulletType type) {
    if (active_bullets[type].contains(toDestroy)) {
        active_bullets[type].erase(toDestroy);
        pool[type].insert(toDestroy);
    }
}