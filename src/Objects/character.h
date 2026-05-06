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
    ~Character() override;
    void routine();
    void draw();
    void drawAtBody(Rectangle bodyRect) override;
    bool blocksMovementFor(GObject* moving) const override;
    void onCollision(GObject* other) override;
    Rectangle getRect() { return body_->getSurface(); }
    CollisionRect* getCollisionBody() override { return body_; }
    void collectDebugSprites(std::vector<Sprite*>& sprites) override;
    void addSpeedBoost(float factor) { speedMultiplier_ += factor; }
    void addDashBoost(float factor) { dashMultiplier_ += factor; }
    void setDashCooldownMs(int ms) { dashCooldown_ = ms / 1000.0; }
    // Debug tuning API
    float getDebugJumpSpeed() const { return debugJumpSpeed_; }
    void setDebugJumpSpeed(float v) { if (v < 50.0f) v = 50.0f; if (v > 800.0f) v = 800.0f; debugJumpSpeed_ = v; }

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
    void separateFromCollisions();
    void updateDeathRespawn(double delta);
    void finishRespawn();
    bool shouldDrawDuringDeath() const;

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
    
    // Debug speed controls
    float debugBaseSpeed_ = 100.0f;
    float debugJumpSpeed_ = 250.0f;

    bool cancelJump_ = false; // if true, cut jump short on next routine (for variable jump height
    bool jumpHeld_ = false; // track if jump button is held across frames
    bool groundStateInitialized_ = false;
    bool wasGrounded_ = false;
    bool wasTouchingStillWater_ = false;
    double lastStillWaterTouchAt_ = -1000.0;
    float lastFallSpeedBeforeMove_ = 0.0f;
    double lastWaterSplashAt_ = -1000.0;
    bool dying_ = false;
    double deathElapsed_ = 0.0;
    static constexpr double deathRespawnDelay_ = 1.0;
    static constexpr double deathBlinkPeriod_ = 0.16;
    
    // Original hitbox dimensions (for rotation)
    Vector2 originalHitboxDims_{0, 0};
    GravityDirection lastGravityDir_ = GravityDirection::DOWN;
    
    void updateHitboxRotation();
    void updateSpriteRotation();
    Rectangle spriteRectForBody(Rectangle bodyRect) const;
};
