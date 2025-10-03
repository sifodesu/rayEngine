#include "portal.h"
#include "character.h"
#include "raycam_m.h"
#include "object_m.h"
#include "input.h"
#include "ldtk_m.h"
#include "collisionRect.h"
#include <raymath.h>
#include <vector>
#include <cmath>

Portal* Portal::playerSpawnedPortal_ = nullptr;

Portal::Portal(const SpawnData& data) 
    : BasicEnt(data), isPlayerSpawned_(false) {
    
    // Store the original sprite rectangle before modifying collision box
    spriteRect_ = body_->getSurface();
    
    if (data.interaction.direction.has_value()) {
        direction_ = data.interaction.direction.value();
        
        // Extend collision box by 1 pixel in the direction the portal faces
        // This ensures portals embedded in walls can still detect the character
        // Note: This only affects the hitbox, not the visual sprite
        Rectangle collisionRect = body_->getSurface();
        switch (direction_.value()) {
            case PortalDirection::UP:
                collisionRect.y -= 1;
                collisionRect.height += 1;
                break;
            case PortalDirection::DOWN:
                collisionRect.height += 1;
                break;
            case PortalDirection::LEFT:
                collisionRect.x -= 1;
                collisionRect.width += 1;
                break;
            case PortalDirection::RIGHT:
                collisionRect.width += 1;
                break;
        }
        body_->setSurface(collisionRect);
    }
    
    // Create linkable component using a unique identifier
    // Use the object's ID as a string for linkable identification
    std::string ownId = std::to_string(id_);
    linkable_ = new LinkableComponent(this, ownId);
        
    
    // Set up portal-specific linking callback
    linkable_->onTriggerReceived = [this](const std::string& sourceId, const std::string& message, void* data) {
        this->onPortalLink(sourceId, message, data);
    };
    
    if (data.ldtk.linkId.has_value()) {
        targetLdtkId_ = data.ldtk.linkId.value();
    }
    
    if (data.ldtk.targetIds.has_value()) {
        for (const std::string& targetId : data.ldtk.targetIds.value()) {
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

    // Prune entities that are no longer overlapping (simple AABB test)
    if (!overlappingEntities_.empty()) {
        Rectangle myRect = body_->getSurface();
        std::vector<int> toRemove;
        for (int entId : overlappingEntities_) {
            auto it = Object_m::level_ents_.find(entId);
            if (it == Object_m::level_ents_.end()) { toRemove.push_back(entId); continue; }
            GObject* obj = it->second.get();
            Character* ch = dynamic_cast<Character*>(obj);
            if (!ch) { toRemove.push_back(entId); continue; }
            Rectangle otherRect = ch->getRect();
            if (!CheckCollisionRecs(myRect, otherRect)) {
                toRemove.push_back(entId); // entity left portal
            }
        }
        for (int rem : toRemove) overlappingEntities_.erase(rem);
    }
}

void Portal::draw() {
    // Generate a distinctive color based on linked portal
    Color portalColor = WHITE; // Default color
    
    Portal* linkedPortal = getLinkedPortal();
    if (linkedPortal) {
        // Use the smaller ID of the pair to ensure both portals get the same color
        int colorSeed = std::min(id_, linkedPortal->id_);
        
        // Generate HSV color based on the seed for better color distribution
        float hue = (colorSeed * 137.5f) / 360.0f; // Golden angle approximation for good distribution
        hue = hue - floor(hue); // Keep fractional part (0.0 to 1.0)
        
        // Convert HSV to RGB with high saturation and moderate brightness
        float saturation = 0.8f;
        float brightness = 0.9f;
        
        // HSV to RGB conversion
        float c = brightness * saturation;
        float x = c * (1 - abs(fmod(hue * 6, 2) - 1));
        float m = brightness - c;
        
        float r, g, b;
        if (hue < 1.0f/6.0f) { r = c; g = x; b = 0; }
        else if (hue < 2.0f/6.0f) { r = x; g = c; b = 0; }
        else if (hue < 3.0f/6.0f) { r = 0; g = c; b = x; }
        else if (hue < 4.0f/6.0f) { r = 0; g = x; b = c; }
        else if (hue < 5.0f/6.0f) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        
        portalColor = Color{
            (unsigned char)((r + m) * 255),
            (unsigned char)((g + m) * 255), 
            (unsigned char)((b + m) * 255),
            255
        };
    } 
    
    // Set the tint and draw using the original sprite rectangle (not the extended collision box)
    sprite_->setTint(portalColor);
    sprite_->draw(spriteRect_);
    sprite_->setTint(WHITE); // Reset tint for next frame
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
    // Trigger teleport only on collision enter (not already overlapping)
    if (!other) return;
    int oid = other->id_;
    if (overlappingEntities_.find(oid) != overlappingEntities_.end()) return; // already inside

    Character* character = dynamic_cast<Character*>(other);
    if (!character) return;

    Portal* linkedPortal = getLinkedPortal();
    if (!linkedPortal) return;

    // Check if player is fully inside the portal based on direction
    Rectangle playerRect = character->body_->getSurface();
    Rectangle portalRect = body_->getSurface();
    
    bool shouldTeleport = false;
    
    if (linkedPortal->direction_.has_value()) {
        switch (linkedPortal->direction_.value()) {
            case PortalDirection::UP:
            case PortalDirection::DOWN:
                // For vertical directions, check if player is fully inside horizontally
                shouldTeleport = (playerRect.x >= portalRect.x && 
                                (playerRect.x + playerRect.width) <= (portalRect.x + portalRect.width));
                break;
            case PortalDirection::LEFT:
            case PortalDirection::RIGHT:
                // For horizontal directions, check if player is fully inside vertically
                shouldTeleport = (playerRect.y >= portalRect.y && 
                                (playerRect.y + playerRect.height) <= (portalRect.y + portalRect.height));
                break;
        }
    } else {
        // Default behavior: teleport on any collision
        shouldTeleport = true;
    }
    
    if (shouldTeleport) {
        overlappingEntities_.insert(oid);
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
    portalData.entityType = EntityType::Portal;
    portalData.layer = 1;
    
    // Set unique link ID for player spawned portal
    portalData.ldtk.linkId = "player_portal";
    
    // Set position near player
    Rectangle playerRect = player->getRect();
    Vector2 playerCenter = {playerRect.x + playerRect.width / 2.0f, 
                           playerRect.y + playerRect.height / 2.0f};
    
    // Portal sprite configuration
    SpriteDesc sprite;
    sprite.filename = "gateway.png"; // Using existing texture
    portalData.sprite = sprite;
    
    CollisionDesc collision;
    collision.rect = Rectangle{playerCenter.x - 4, playerCenter.y - 4, 8, 8};
    collision.solid = false;
    portalData.physics.collision = collision;
    
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
        if (character) { player = character; break; }
    }
    if (!player) return;

    Vector2 portalCenter = playerSpawnedPortal_->getCenter();
    Rectangle playerRect = player->getRect();
    float newX = portalCenter.x - playerRect.width / 2.0f;
    float newY = portalCenter.y - playerRect.height / 2.0f;
    if (player->body_) {
        Rectangle newRect = player->body_->getSurface();
        newRect.x = newX;
        newRect.y = newY;
        player->body_->setSurface(newRect);
    }
    // Mark overlap so manual teleport doesn't immediately retrigger until player exits
    playerSpawnedPortal_->overlappingEntities_.insert(player->id_);
}

bool Portal::findSafePosition(Rectangle& playerRect, const Rectangle& targetPortalRect, PortalDirection direction) {
    Portal* linkedPortal = getLinkedPortal();
    
    // Test the initially calculated position
    std::vector<CollisionRect*> collisions = CollisionRect::query(playerRect, true); // only solid objects
    
    // Filter out the target portal itself from collision check
    auto it = std::remove_if(collisions.begin(), collisions.end(), 
        [this, linkedPortal](CollisionRect* col) {
            return col->getFather() == this || 
                   (linkedPortal && col->getFather() == linkedPortal);
        });
    collisions.erase(it, collisions.end());
    
    if (collisions.empty()) {
        return true; // Position is safe
    }
    
    // Try to push the player away from the portal in the direction they should exit
    float pushDistance = 2.0f; // pixels to push per attempt
    int maxAttempts = 20;
    
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        Rectangle testRect = playerRect;
        
        switch (direction) {
            case PortalDirection::UP:
                testRect.y -= pushDistance * attempt;
                break;
            case PortalDirection::DOWN:
                testRect.y += pushDistance * attempt;
                break;
            case PortalDirection::LEFT:
                testRect.x -= pushDistance * attempt;
                break;
            case PortalDirection::RIGHT:
                testRect.x += pushDistance * attempt;
                break;
        }
        
        // Check if this position is safe
        std::vector<CollisionRect*> testCollisions = CollisionRect::query(testRect, true);
        
        // Filter out portals again
        auto testIt = std::remove_if(testCollisions.begin(), testCollisions.end(), 
            [this, linkedPortal](CollisionRect* col) {
                return col->getFather() == this || 
                       (linkedPortal && col->getFather() == linkedPortal);
            });
        testCollisions.erase(testIt, testCollisions.end());
        
        if (testCollisions.empty()) {
            playerRect = testRect;
            return true;
        }
    }
    
    return false; // Couldn't find a safe position
}

void Portal::performTeleportation(GObject* player) {
    Portal* linkedPortal = getLinkedPortal();
    if (!linkedPortal) return;

    Character* character = dynamic_cast<Character*>(player);
    if (!character || !character->body_) return;

    Rectangle playerRect = character->body_->getSurface();
    Rectangle sourcePortalRect = body_->getSurface();
    Rectangle targetPortalRect = linkedPortal->body_->getSurface();

    // Calculate relative position within source portal (0.0 to 1.0)
    float relativeX = (playerRect.x + playerRect.width / 2.0f - sourcePortalRect.x) / sourcePortalRect.width;
    float relativeY = (playerRect.y + playerRect.height / 2.0f - sourcePortalRect.y) / sourcePortalRect.height;
    
    // Clamp to valid range
    relativeX = std::max(0.0f, std::min(1.0f, relativeX));
    relativeY = std::max(0.0f, std::min(1.0f, relativeY));

    Vector2 spd = character->body_->getSpeed();
    float newX, newY;

    // Apply direction-based positioning and velocity modification from the destination portal
    if (linkedPortal->direction_.has_value()) {
        switch (linkedPortal->direction_.value()) {
            case PortalDirection::UP:
                // Exit from bottom of target portal, maintain horizontal relative position
                newX = targetPortalRect.x + relativeX * targetPortalRect.width - playerRect.width / 2.0f;
                newY = targetPortalRect.y + targetPortalRect.height - playerRect.height;
                spd.y = -abs(spd.y); // Force upward velocity
                break;
            case PortalDirection::DOWN:
                // Exit from top of target portal, maintain horizontal relative position
                newX = targetPortalRect.x + relativeX * targetPortalRect.width - playerRect.width / 2.0f;
                newY = targetPortalRect.y;
                spd.y = abs(spd.y); // Force downward velocity
                break;
            case PortalDirection::LEFT:
                // Exit from right side of target portal, maintain vertical relative position
                newX = targetPortalRect.x + targetPortalRect.width - playerRect.width;
                newY = targetPortalRect.y + relativeY * targetPortalRect.height - playerRect.height / 2.0f;
                spd.x = -abs(spd.x); // Force leftward velocity
                break;
            case PortalDirection::RIGHT:
                // Exit from left side of target portal, maintain vertical relative position
                newX = targetPortalRect.x;
                newY = targetPortalRect.y + relativeY * targetPortalRect.height - playerRect.height / 2.0f;
                spd.x = abs(spd.x); // Force rightward velocity
                break;
        }
    } else {
        // Default behavior: center positioning with inverted vertical velocity
        newX = targetPortalRect.x + targetPortalRect.width / 2.0f - playerRect.width / 2.0f;
        newY = targetPortalRect.y + targetPortalRect.height / 2.0f - playerRect.height / 2.0f;
        spd.y = -spd.y;
    }

    Rectangle newRect = playerRect;
    newRect.x = newX;
    newRect.y = newY;
    
    // Ensure the position is safe (not colliding with solid walls)
    PortalDirection safetyDirection = linkedPortal->direction_.value_or(PortalDirection::UP);
    if (!findSafePosition(newRect, targetPortalRect, safetyDirection)) {
        // If we can't find a safe position, fall back to center positioning
        newRect.x = targetPortalRect.x + targetPortalRect.width / 2.0f - playerRect.width / 2.0f;
        newRect.y = targetPortalRect.y + targetPortalRect.height / 2.0f - playerRect.height / 2.0f;
        
        // Try center position safety check
        if (!findSafePosition(newRect, targetPortalRect, safetyDirection)) {
            // Last resort: don't teleport if even center isn't safe
            return;
        }
    }
    
    character->body_->setSurface(newRect);
    character->body_->setSpeed(spd);
    
    // Mark the player as overlapping the destination portal to avoid immediate bounce-back
    linkedPortal->overlappingEntities_.insert(character->id_);
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