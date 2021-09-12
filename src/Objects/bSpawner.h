#pragma once
#include <vector>
#include <queue>
#include <raylib.h>
#include "gObject.h"
#include "rigidBody.h"
#include "bullet.h"

class BSpawner : public GObject {
    BSpawner();
    void routine();
    ~BSpawner();

private:
    RigidBody* body_;
    std::queue<Bullet*> to_spawn_;

};