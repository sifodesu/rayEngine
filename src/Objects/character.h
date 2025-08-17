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
    void addSpeedBoost(float factor) { speedMultiplier_ += factor; }
    void addDashBoost(float factor) { dashMultiplier_ += factor; }
    // Respawn API
    void setRespawn(Vector2 p) { respawnPos_ = p; }
    void respawn();

    RigidBody *body_;

private:
    // Helpers
    bool isOnGround() const;

    std::unordered_map<std::string, Sprite *> anims_;
    double dashing_;
    float speedMultiplier_ = 1.0f; // movement speed multiplier
    float dashMultiplier_ = 1.0f;   // dash distance/speed multiplier
    Sprite* current_anim_ = nullptr;
    Vector2 respawnPos_{0,0};
    int jumps_ = 0;
    int dashCount_ = 0;
};
