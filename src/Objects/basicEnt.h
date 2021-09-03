#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "gObject.h"
#include "sprite.h"
#include "rigidBody.h"

class BasicEnt : public GObject {
public:
    BasicEnt(nlohmann::json obj);
    ~BasicEnt();
    void draw(Vector2 pos);
    void routine();

private:
    Sprite* sprite_;
    RigidBody* body_;
};