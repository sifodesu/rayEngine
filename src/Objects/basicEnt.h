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
    CollisionRect* getCollisionBody() override { return body_; }
    void collectDebugSprites(std::vector<Sprite*>& sprites) override;
    
    CollisionRect *body_;

protected:
    Sprite *sprite_;
};
