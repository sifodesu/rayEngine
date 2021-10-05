#include "bullet_m.h"
#include "object_m.h"

#define POOL_SIZE 10000

using namespace std;

unordered_set<Bullet*> Bullet_m::pool;
unordered_set<Bullet*> Bullet_m::active_bullets;

void Bullet_m::init() {
    for (int i = 0; i < POOL_SIZE; i++) {
        Bullet* b = new Bullet(Object_m::blueprints_[BULLET]);
        pool.insert(b);
    }
}

void Bullet_m::routine() {
    vector<Bullet*> toDestroy;
    for (auto bullet : active_bullets) {
        bullet->routine();
        if (bullet->ttl_ < 0) {
            toDestroy.push_back(bullet);
        }
    }
    for (auto bullet : toDestroy) {
        destroyBullet(bullet);
    }
}
void Bullet_m::draw() {
    for (auto bullet : active_bullets) {
        bullet->draw();
    }
}
Bullet* Bullet_m::createBullet() {
    if (!pool.empty()) {
        Bullet* newBullet = *pool.begin();
        pool.erase(newBullet);
        active_bullets.insert(newBullet);

        newBullet->ttl_ = 2;
        newBullet->to_delete_ = false;
        newBullet->clock_.getLap();
        return newBullet;
    }

    return *active_bullets.begin();
}

void Bullet_m::destroyBullet(Bullet* toDestroy) {
    if (active_bullets.contains(toDestroy)) {
        active_bullets.erase(toDestroy);
        pool.insert(toDestroy);
    }
}