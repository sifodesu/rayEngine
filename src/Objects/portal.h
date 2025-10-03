#pragma once
#include "basicEnt.h"
#include "linkableComponent.h"
#include "spawn.h"
#include <optional>
#include <unordered_set>

class Portal : public BasicEnt {
public:
    explicit Portal(const SpawnData& data);
    ~Portal();
    
    void onCollision(GObject* other) override;
    void routine() override;
    void draw() override;
    
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
    
    // Get portal direction
    std::optional<PortalDirection> getDirection() const { return direction_; }
    
    // Static method for post-load linking setup
    static void setupPortalLinks();
    
private:
    LinkableComponent* linkable_;
    bool isPlayerSpawned_;
    std::optional<PortalDirection> direction_; // Portal direction for velocity modification
    bool forceGravity_ = false; // Whether this portal forces gravity direction on player
    Rectangle spriteRect_; // Original sprite rectangle (before collision box extension)
    // Track which entity IDs are currently overlapping this portal. Teleport
    // only triggers on collision enter (id not already in set). When the
    // entity leaves the portal area it's removed during routine(), allowing
    // immediate reactivation without time-based cooldown.
    std::unordered_set<int> overlappingEntities_;
    std::string targetLdtkId_; // Store the target LDtk ID for later resolution
    
    // Static portal management
    static Portal* playerSpawnedPortal_;
    
    void performTeleportation(GObject* player);
    void onPortalLink(const std::string& sourceId, const std::string& message, void* data);
    bool findSafePosition(Rectangle& playerRect, const Rectangle& targetPortalRect, PortalDirection direction);
};