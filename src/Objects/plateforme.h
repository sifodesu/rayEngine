#pragma once
#include "basicEnt.h"
#include <vector>
#include <raylib.h> // ensure Vector2

#include "adiComponent.h"


class Plateforme : public BasicEnt {
public:
    explicit Plateforme(const SpawnData& data);
    ~Plateforme();

    enum Behavior {
        PING_PONG,
        LOOP
    };

    void routine() override;
    
    void draw() override;
    
    // Interaction interface
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // Override pour le système optional (kept for now, as instruction snippet was ambiguous)
    std::optional<AdiComponent*> getAdiComponent() override { 
        return adiComponent_ ? std::optional<AdiComponent*>{adiComponent_} : std::nullopt; 
    }

private:
    struct CarriedObject {
        CollisionRect* body = nullptr;
        Vector2 offset{0.0f, 0.0f};
        bool targetSide = false;
    };

    // Helper methods for cleaner code organization
    Vector2 getCurrentCenter() const;
    Vector2 getCurrentTarget() const;
    // Generic object transport
    std::vector<CarriedObject> findRidingObjects() const;
    void attachCarriedObjects(const std::vector<CarriedObject>& objects);
    void rememberCarriedObjects(const std::vector<CarriedObject>& objects);
    
    // Movement logic
    Vector2 calculateMovement(Vector2 currentCenter, double deltaTime);
    void snapToTarget(Vector2 target);
    bool shouldSwitchDirection();
    void switchDirection();

    // Member variables
    std::vector<Vector2> waypoints_; // center waypoints
    int current_ = 0;
    int dir_ = 1; // direction through waypoints
    float speed_ = 80.0f; // pixels per second
    
    // Wait logic
    float waitTime_ = 0.4f; // seconds to wait at endpoints
    float waiting_ = 0.0f;
    
    Vector2 lastCenter_{0,0}; // previous center for stable delta
    // Axis-specific accumulators for pixel-perfect movement
    float accX_ = 0.0f;
    float accY_ = 0.0f;
    std::vector<int> carriedOwnerIds_;
    
    // Interaction state
    bool enabled_ = true; // whether platform should move
    AdiComponent* adiComponent_ = nullptr; // Component pour être triggerable

    Behavior behavior_ = PING_PONG; // Default behavior
}; // Plateforme
