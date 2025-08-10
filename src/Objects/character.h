#pragma once
#include <raylib.h>
#include "spawn.h"

#include "gObject.h"
#include "rigidBody.h"
#include "sprite.h"

class Character : public GObject
{
public:
    explicit Character(const SpawnData& data);
    void routine();
    void draw();
    Rectangle getRect() { return body_->getSurface(); }

    RigidBody *body_;

private:
    Sprite *sprite_;
    double dashing_;
};
