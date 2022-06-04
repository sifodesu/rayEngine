#include <iostream>

#include "bullet_m.h"
#include "object_m.h"
#include "rigidBullet.h"
#include "zigzagBullet.h"
#include "definitions.h"
#include "explosiveBullet.h"
#define POOL_SIZE 2000

using namespace std;

unordered_map<BulletType, unordered_set<Bullet*>> Bullet_m::pool;
unordered_map<BulletType, unordered_set<Bullet*>> Bullet_m::active_bullets;
std::queue<std::tuple<double, BulletType, Bullet*>> Bullet_m::waiting_bullets;

void Bullet_m::init() {
    for (BulletType type : { RIGID, ZIGZAG, EXPLOSIVE }) {
        for (int i = 0; i < POOL_SIZE; i++) {
            if (type == RIGID) {
                RigidBullet* b = new RigidBullet(Object_m::blueprints_[BULLET]);
                pool[type].insert(b);
            }

            if (type == ZIGZAG) {
                ZigzagBullet* b = new ZigzagBullet(Object_m::blueprints_[BULLET]);
                pool[type].insert(b);
            }

            if (type == EXPLOSIVE) {
                ExplosiveBullet* b = new ExplosiveBullet(Object_m::blueprints_[BULLET]);
                pool[type].insert(b);
            }
        }

    }
}

void Bullet_m::routine() {
    double delta = Clock::getLap();
    if (!waiting_bullets.empty()) {
        auto& [delay, type, bullet] = waiting_bullets.front();
        delay -= delta;
        if (delay <= 0) {
            waiting_bullets.pop();
            active_bullets[type].insert(bullet);
        }
    }

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

void Bullet_m::purge() {
    for (auto [type, bullets] : active_bullets) {
        for (auto bullet : bullets) {
            pool[type].insert(bullet);
        }
    }
    active_bullets.clear();
    
    while (!waiting_bullets.empty()) {
        auto& [delay, type, bullet] = waiting_bullets.front();
        pool[type].insert(bullet);
        waiting_bullets.pop();
    }
}

Bullet* Bullet_m::createBullet(BulletType type, unordered_set<GObject*> no_dmg, double delay) {
    if (!pool[type].empty()) {
        Bullet* newBullet = *pool[type].begin();
        pool[type].erase(newBullet);
        newBullet->ttl_ = 2;
        newBullet->no_dmg_ = no_dmg;

        if (!delay) {
            active_bullets[type].insert(newBullet);
        }
        else {
            waiting_bullets.push(make_tuple(delay, type, newBullet));
        }

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
