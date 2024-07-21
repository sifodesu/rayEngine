#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>

#include "gObject.h"
#include "rigidBody.h"
#include "sprite.h"

class Character : public GObject
{
public:
    Character(nlohmann::json obj);
    void routine();
    void draw();
    Rectangle getRect() { return body_->getSurface(); }

    RigidBody *body_;

private:
    Sprite *sprite_;
    double dashing_;
};
