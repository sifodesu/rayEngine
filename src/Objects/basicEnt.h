#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "gObject.h"
#include "sprite.h"
#include "rigidBody.h"

// Json fields:
// sprite
// collisionRect

class BasicEnt : public GObject
{
public:
    BasicEnt(nlohmann::json obj);
    ~BasicEnt();
    void draw();
    void routine();
    Rectangle getRect() { return body_->getSurface(); }
    
    CollisionRect *body_;

private:
    Sprite *sprite_;
};