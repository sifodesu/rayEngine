#pragma once
#include "basicEnt.h"
#include <vector>
#include <raylib.h> // ensure Vector2

class AdiComponent;

class Plateforme : public BasicEnt {
public:
    explicit Plateforme(const SpawnData& data);
    void routine() override;
    
    // Interface publique (legacy compatibility)
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void draw() override;
    
    // Override pour le système optional
    std::optional<AdiComponent*> getAdiComponent() override { 
        return adiComponent_ ? std::optional<AdiComponent*>{adiComponent_} : std::nullopt; 
    }

private:
    // Helper methods for cleaner code organization
    Vector2 getCurrentCenter() const;
    Vector2 getCurrentTarget() const;
    std::vector<CollisionRect*> findRidingCharacters(const Rectangle& platformSurface) const;
    void moveCarriedCharacters(const std::vector<CollisionRect*>& characters, Vector2 deltaMove, Vector2 newCenter);
    Vector2 calculateMovement(Vector2 currentCenter, double deltaTime);
    bool shouldSwitchDirection();
    void switchDirection();
    void snapToTarget(Vector2 target);

    // Member variables
    std::vector<Vector2> waypoints_; // center waypoints
    int current_ = 0;
    int dir_ = 1; // direction through waypoints
    float speed_ = 60.0f; // pixels per second
    float waitTime_ = 0.4f; // seconds to wait at endpoints
    float waiting_ = 0.0f;
    Vector2 lastCenter_{0,0}; // previous center for stable delta
    // Axis-specific accumulators for pixel-perfect movement
    float accX_ = 0.0f;
    float accY_ = 0.0f;
    bool enabled_ = true; // whether platform should move
    AdiComponent* adiComponent_ = nullptr; // Component pour être triggerable
}; // Plateforme
