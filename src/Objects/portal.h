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
    
    std::optional<LinkableComponent*> getLinkableComponent() override;

    Portal* getLinkedPortal() const;
    void setIsPlayerSpawned(bool playerSpawned) { isPlayerSpawned_ = playerSpawned; }
    bool isPlayerSpawned() const { return isPlayerSpawned_; }

    static void teleportPlayerToSpawnedPortal();
    static void spawnPortalAtPlayer();
    static Portal* getPlayerSpawnedPortal() { return playerSpawnedPortal_; }

    Vector2 getCenter() const;
    Rectangle getPortalRect() const { return spriteRect_; }
    std::optional<PortalDirection> getDirection() const { return direction_; }
    bool forcesGravity() const { return forceGravity_; }

private:
    friend class Portal_m;

    LinkableComponent* linkable_;
    bool isPlayerSpawned_;
    std::optional<PortalDirection> direction_;
    bool forceGravity_ = false;
    Rectangle spriteRect_;
    std::unordered_set<int> overlappingEntities_;
    std::string targetLdtkId_;

    static Portal* playerSpawnedPortal_;

    bool performTeleportation(GObject* entity);
    void resolveLinkTarget();
    void onPortalLink(const std::string& sourceId, const std::string& message, void* data);
    bool findSafePosition(Rectangle& entityRect, CollisionRect* entityBody, const Rectangle& targetPortalRect, PortalDirection direction);
};
