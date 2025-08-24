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
    void setDashCooldownMs(int ms) { dashCooldown_ = ms / 1000.0; }
    // Adi API
    int getAdi() const { return adiCount_; }
    int getAdiMax() const { return adiMax_; }
    bool canDepositAdi() const { return adiCount_ > 0; }
    bool depositOneAdi(); // returns true if deposited
    bool retrieveOneAdi(); // returns true if retrieved
    void resetAdiFull() { adiCount_ = adiMax_; }
    void onRoomEntered(); // call when transitioning rooms
    
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
    // Adi charges (energy mechanic)
    int adiMax_ = 3;
    int adiCount_ = 3; // current owned
    // scaling factors influenced by current adi count (computed each frame)
    float adiJumpBonusPerCharge_ = 0.12f; // +12% jump speed per adi owned
    float adiDashBonusPerCharge_ = 0.08f; // +8% dash factor per adi owned
    
    // internal helper to compute current jump speed factoring adi
    float currentJumpSpeed() const;
    float currentDashFactor() const;
    
    double dashCooldown_ = 0.35;        // seconds between dashes (default 350 ms)
    double dashCooldownLeft_ = 0.0;     // seconds remaining until dash is available
    // Track last room grid (camera snap grid) to detect transitions
    int lastRoomGX_ = INT32_MIN;
    int lastRoomGY_ = INT32_MIN;
};
