#include "portal.h"
#include "character.h"
#include "raycam_m.h"
#include "object_m.h"
#include "input.h"
#include "ldtk_m.h"
#include <raymath.h>

Portal* Portal::playerSpawnedPortal_ = nullptr;

Portal::Portal(const SpawnData& data) 
    : BasicEnt(data), isPlayerSpawned_(false), cooldownTimer_(0.0f) {
    
    // Create linkable component using a unique identifier
    // Use the object's ID as a string for linkable identification
    std::string ownId = std::to_string(id_);
    linkable_ = new LinkableComponent(this, ownId);
        
    
    // Set up portal-specific linking callback
    linkable_->onTriggerReceived = [this](const std::string& sourceId, const std::string& message, void* data) {
        this->onPortalLink(sourceId, message, data);
    };
    
    // Store the target LDtk ID for later resolution
    if (data.linkId.has_value()) {
        targetLdtkId_ = data.linkId.value();
    }
    
    // Add additional target IDs if provided (these should be engine IDs)
    if (data.targetIds.has_value()) {
        for (const std::string& targetId : data.targetIds.value()) {
            linkable_->addTargetId(targetId);
        }
    }
}

Portal::~Portal() {
    // Clean up linkable component
    delete linkable_;
    
    // Clear static reference if this is the player spawned portal
    if (playerSpawnedPortal_ == this) {
        playerSpawnedPortal_ = nullptr;
    }
}

void Portal::routine() {
    BasicEnt::routine();
    
    // Update cooldown timer
    if (cooldownTimer_ > 0.0f) {
        cooldownTimer_ -= GetFrameTime();
    }
}

std::optional<LinkableComponent*> Portal::getLinkableComponent() {
    return linkable_;
}

Portal* Portal::getLinkedPortal() const {
    // Get the first linked portal from the linkable component
    std::vector<GObject*> linkedObjects = linkable_->getLinkedObjects();
    
    for (GObject* obj : linkedObjects) {
        Portal* portal = dynamic_cast<Portal*>(obj);
        if (portal) {
            return portal;
        }
    }
    
    return nullptr;
}

void Portal::onCollision(GObject* other) {
    // Check if the colliding object is a character (player)
    Character* character = dynamic_cast<Character*>(other);
    Portal* linkedPortal = getLinkedPortal();
    
    if (character && cooldownTimer_ <= 0.0f && linkedPortal) {
        performTeleportation(other);
    }
}

Vector2 Portal::getCenter() const {
    Rectangle rect = body_->getSurface();
    return Vector2{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
}

void Portal::spawnPortalAtPlayer() {
    // Find the player (Character object)
    Character* player = nullptr;
    for (auto& [id, obj] : Object_m::level_ents_) {
        Character* character = dynamic_cast<Character*>(obj.get());
        if (character) {
            player = character;
            break;
        }
    }
    
    if (!player) return;
    
    // Remove existing player spawned portal
    if (playerSpawnedPortal_) {
        playerSpawnedPortal_->to_delete_ = true;
        playerSpawnedPortal_ = nullptr;
    }
    
    // Create a new portal at player position
    SpawnData portalData;
    portalData.id = Object_m::genID();
    portalData.type = "Portal";
    portalData.layer = 1;
    
    // Set unique link ID for player spawned portal
    portalData.linkId = "player_portal";
    
    // Set position near player
    Rectangle playerRect = player->getRect();
    Vector2 playerCenter = {playerRect.x + playerRect.width / 2.0f, 
                           playerRect.y + playerRect.height / 2.0f};
    
    // Portal sprite configuration
    SpriteDesc sprite;
    sprite.filename = "gateway.png"; // Using existing texture
    sprite.tint = BLUE; // Blue tint for player-spawned portals
    portalData.sprite = sprite;
    
    // Portal collision configuration
    CollisionDesc collision;
    collision.rect = Rectangle{playerCenter.x - 4, playerCenter.y - 4, 8, 8};
    collision.solid = false; // Not solid so player can pass through
    portalData.collision = collision;
    
    // Create the portal
    GObject* newPortalObj = Object_m::createFromSpawn(portalData);
    Portal* newPortal = dynamic_cast<Portal*>(newPortalObj);
    if (newPortal) {
        newPortal->setIsPlayerSpawned(true);
        playerSpawnedPortal_ = newPortal;
    }
}

void Portal::teleportPlayerToSpawnedPortal() {
    if (!playerSpawnedPortal_) return;
    
    // Find the player
    Character* player = nullptr;
    for (auto& [id, obj] : Object_m::level_ents_) {
        Character* character = dynamic_cast<Character*>(obj.get());
        if (character) {
            player = character;
            break;
        }
    }
    
    if (!player) return;
    
    // Teleport player to the spawned portal
    Vector2 portalCenter = playerSpawnedPortal_->getCenter();
    Rectangle playerRect = player->getRect();
    
    // Calculate new position (center player on portal)
    float newX = portalCenter.x - playerRect.width / 2.0f;
    float newY = portalCenter.y - playerRect.height / 2.0f;
    
    // Update player position through collision body
    if (player->body_) {
        Rectangle newRect = player->body_->getSurface();
        newRect.x = newX;
        newRect.y = newY;
        player->body_->setSurface(newRect);
    }
    
    // Set cooldown on the spawned portal to prevent immediate re-teleportation
    playerSpawnedPortal_->cooldownTimer_ = TELEPORT_COOLDOWN;
}

void Portal::performTeleportation(GObject* player) {
    Portal* linkedPortal = getLinkedPortal();
    if (!linkedPortal) return;
    
    Vector2 targetCenter = linkedPortal->getCenter();
    Rectangle playerRect = player->getRect();
    
    // Calculate new position (center player on target portal)
    float newX = targetCenter.x - playerRect.width / 2.0f;
    float newY = targetCenter.y - playerRect.height / 2.0f;
    
    // Update player position
    Character* character = dynamic_cast<Character*>(player);
    if (character && character->body_) {
        Rectangle newRect = character->body_->getSurface();
        newRect.x = newX;
        newRect.y = newY;
        character->body_->setSurface(newRect);
    }
    
    // Set cooldown on both portals
    cooldownTimer_ = TELEPORT_COOLDOWN;
    linkedPortal->cooldownTimer_ = TELEPORT_COOLDOWN;
}

void Portal::onPortalLink(const std::string& sourceId, const std::string& message, void* data) {
    // Handle portal-specific linking messages if needed
    // For basic teleportation, no special handling is required
}

void Portal::setupPortalLinks() {
    // Find all portal objects and resolve their target links
    for (auto& [id, obj] : Object_m::level_ents_) {
        Portal* portal = dynamic_cast<Portal*>(obj.get());
        if (portal && !portal->targetLdtkId_.empty()) {            
            // Convert LDtk ID to engine ID
            int targetEngineId = Ldtk_m::getEngineIdFromLdtkId(portal->targetLdtkId_);
            if (targetEngineId != -1) {
                std::string targetIdStr = std::to_string(targetEngineId);
                portal->linkable_->addTargetId(targetIdStr);
            }
        }
    }
}