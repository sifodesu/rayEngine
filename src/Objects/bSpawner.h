#pragma once
#include <vector>
#include <queue>
#include <raylib.h>
#include <nlohmann/json.hpp>

#include "gObject.h"
#include "rigidBody.h"
#include "bullet.h"
#include "sprite.h"

class BSpawner : public GObject {
    BSpawner(nlohmann::json obj);
    void routine();
    ~BSpawner();

private:
    RigidBody* body_;
    Sprite* sprite_;
    std::queue<Bullet*> to_spawn_;

};