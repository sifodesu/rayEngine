#pragma once
#include <raylib.h>
#include "gObject.h"
#include "sprite.h"
#include "rigidBody.h"
#include "spawn.h"

// Json fields:
// sprite
// collisionRect

class BasicEnt : public GObject
{
public:
    explicit BasicEnt(const SpawnData& data);
    ~BasicEnt();
    void draw();
    void routine();
    Rectangle getRect() { return body_->getSurface(); }
    
    CollisionRect *body_;

private:
    Sprite *sprite_;
};