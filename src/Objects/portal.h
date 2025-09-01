#pragma once
#include "basicEnt.h"
#include "linkableComponent.h"
#include <optional>

class Portal : public BasicEnt {
public:
    explicit Portal(const SpawnData& data);
    ~Portal();
    
    void onCollision(GObject* other) override;
    void routine() override;
    
    // Override to provide linkable component
    std::optional<LinkableComponent*> getLinkableComponent() override;
    
    // Portal-specific methods using generic linking
    Portal* getLinkedPortal() const;
    void setIsPlayerSpawned(bool playerSpawned) { isPlayerSpawned_ = playerSpawned; }
    bool isPlayerSpawned() const { return isPlayerSpawned_; }
    
    // Static methods for portal management using generic system
    static void teleportPlayerToSpawnedPortal();
    static void spawnPortalAtPlayer();
    static Portal* getPlayerSpawnedPortal() { return playerSpawnedPortal_; }
    
    Vector2 getCenter() const;
    
private:
    LinkableComponent* linkable_;
    bool isPlayerSpawned_;
    float cooldownTimer_; // To prevent immediate re-teleportation
    static constexpr float TELEPORT_COOLDOWN = 1.0f; // 1 second cooldown
    
    // Static portal management
    static Portal* playerSpawnedPortal_;
    
    void performTeleportation(GObject* player);
    void onPortalLink(const std::string& sourceId, const std::string& message, void* data);
};