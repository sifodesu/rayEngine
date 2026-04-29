#pragma once
#include "basicEnt.h"
#include "linkableComponent.h"
#include "spawn.h"
#include <optional>
#include <unordered_set>
#include <vector>

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
    Rectangle getPortalRect() const { return spriteRect_; }
    
    // Get portal direction
    std::optional<PortalDirection> getDirection() const { return direction_; }
    bool forcesGravity() const { return forceGravity_; }
    
    // Static method for post-load linking setup
    static void setupPortalLinks();
    static void prepareMovement(GObject* entity, CollisionRect* entityBody, Rectangle fromRect, Rectangle toRect);
    static bool isMovementBlocked(GObject* entity, CollisionRect* entityBody, Rectangle proposedRect);
    static bool isTransitProbeBlocked(GObject* entity, CollisionRect* entityBody, Rectangle probeRect);
    static bool separateTransitCollisions(GObject* entity, CollisionRect* entityBody);
    static bool syncTransit(GObject* entity, const std::vector<CollisionRect*>& carriedBodies = {});
    static void updateTransits();
    static void clearTransits();
    static void cancelTransit(GObject* entity);
    static std::optional<Rectangle> getTransitSourceSurface(GObject* entity);
    static std::optional<Rectangle> getTransitTargetSurface(GObject* entity);
    static std::optional<Vector2> getTransitVisibleCenter(GObject* entity);
    static bool isTransitSourceVisible(GObject* entity, Rectangle rect);
    static std::optional<Rectangle> transformTransitRect(GObject* entity, Rectangle rect);
    static void releaseAllDisabledTiles();
    static bool isEntityInTransit(GObject* entity);
    static void drawTransitsForLayer(int layer);
    static bool shouldIgnoreTransitCollision(GObject* moving, CollisionRect* obstacle);
    
private:
    LinkableComponent* linkable_;
    bool isPlayerSpawned_;
    std::optional<PortalDirection> direction_; // Portal direction for velocity modification
    bool forceGravity_ = false; // Whether this portal forces gravity direction on rigid bodies
    Rectangle spriteRect_; // Original sprite rectangle (before collision box extension)
    // Track which entity IDs are currently overlapping this portal. Teleport
    // only triggers on collision enter (id not already in set). When the
    // entity leaves the portal area it's removed during routine(), allowing
    // immediate reactivation without time-based cooldown.
    std::unordered_set<int> overlappingEntities_;
    std::vector<int> disabledTileIds_;
    std::string targetLdtkId_; // Store the target LDtk ID for later resolution
    
    // Static portal management
    static Portal* playerSpawnedPortal_;
    
    bool performTeleportation(GObject* entity);
    bool beginProgressiveTransit(GObject* entity);
    void onPortalLink(const std::string& sourceId, const std::string& message, void* data);
    bool findSafePosition(Rectangle& entityRect, CollisionRect* entityBody, const Rectangle& targetPortalRect, PortalDirection direction);
};
