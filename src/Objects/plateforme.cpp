#include "plateforme.h"
#include "clock.h"
#include "character.h"
#include "collisionRect.h"
#include "adiComponent.h"
#include "object_m.h"
#include "portal.h"
#include <cmath>

namespace {

bool canBeCarriedByPlatform(CollisionRect* rect, const Plateforme* platform) {
    if (!rect || rect->isRenderProxy()) return false;

    GObject* owner = rect->getFather();
    if (!owner || owner == platform) return false;
    if (dynamic_cast<Portal*>(owner)) return false;

    return Object_m::level_ents_.find(owner->id_) != Object_m::level_ents_.end() ||
           Object_m::level_tiles_.find(owner->id_) != Object_m::level_tiles_.end();
}

bool isTileBody(CollisionRect* rect) {
    GObject* owner = rect ? rect->getFather() : nullptr;
    return owner && Object_m::level_tiles_.find(owner->id_) != Object_m::level_tiles_.end();
}

void moveBodyThroughPortals(
    GObject* owner,
    CollisionRect* body,
    Vector2 deltaMove
) {
    if (!owner || !body) return;
    if (fabsf(deltaMove.x) <= 0.0001f && fabsf(deltaMove.y) <= 0.0001f) return;

    Rectangle fromRect = body->getSurface();
    Rectangle toRect = fromRect;
    toRect.x += deltaMove.x;
    toRect.y += deltaMove.y;

    Portal::prepareMovement(owner, body, fromRect, toRect);
    body->setSurface(toRect);
    Portal::syncTransit(owner);
}

}


Plateforme::Plateforme(const SpawnData& data) : BasicEnt(data) {
    // Starting center
    Rectangle surf = body_->getSurface();
    Vector2 startCenter { surf.x + surf.width/2.0f, surf.y + surf.height/2.0f };
    waypoints_.push_back(startCenter);
    lastCenter_ = startCenter;
    
    enabled_ = data.interaction.enabled.value_or(true);
    
    if (data.interaction.adiComponent.has_value()) {
        const auto& desc = *data.interaction.adiComponent;
        if (desc.canReceiveAdi || desc.canBeTriggered || !desc.targetIds.empty()) {
            // Passer l'ID LDtk au constructeur pour l'auto-enregistrement
            std::string ldtkId = data.ldtk.iid.value_or("");
            adiComponent_ = new AdiComponent(desc, ldtkId);
        }
    }
    
    // Configurer le callback pour réagir aux triggers
    if (adiComponent_ && adiComponent_->canBeTriggered) {
        adiComponent_->onTriggered = [this](bool triggered) {
            this->setEnabled(triggered);
        };
    }

    // Configurer le callback pour réagir aux triggers
    if (adiComponent_ && adiComponent_->canReceiveAdi) {
        adiComponent_->onActivationChanged = [this](bool triggered) {
            this->setEnabled(triggered);
        };
    }
    
    if (data.interaction.pathPoints && !data.interaction.pathPoints->empty()) {
        for (auto absolutePt : *data.interaction.pathPoints) {
            // absolutePt is already in absolute pixel coordinates
            Vector2 worldCenter { absolutePt.x, absolutePt.y };
            waypoints_.push_back(worldCenter);
        }
    }

    // Default behavior is PING_PONG
    behavior_ = Behavior::PING_PONG;
    
    // Parse behavior from interaction config
    if (data.interaction.isLoop.value_or(false)) {
        behavior_ = Behavior::LOOP;
    }
}

Plateforme::~Plateforme() {
    delete adiComponent_;
}

Vector2 Plateforme::getCurrentCenter() const {
    Rectangle surf = body_->getSurface();
    return { surf.x + surf.width/2.0f, surf.y + surf.height/2.0f };
}

Vector2 Plateforme::getCurrentTarget() const {
    if (waypoints_.size() < 2) return getCurrentCenter();
    int idx = current_ + dir_;
    if (idx < 0) idx = waypoints_.size() - 1; // Should not happen in PING_PONG
    if (idx >= waypoints_.size()) idx = 0; // LOOP wrap
    return waypoints_[idx];
}

std::vector<Plateforme::CarriedObject> Plateforme::findRidingObjects() const {
    std::vector<CarriedObject> carryList;
    std::vector<Rectangle> platformSurfaces;
    platformSurfaces.push_back(body_->getSurface());
    bool platformInTransit = Portal::isEntityInTransit(const_cast<Plateforme*>(this));

    if (auto targetSurface = Portal::getTransitTargetSurface(const_cast<Plateforme*>(this))) {
        platformSurfaces.push_back(*targetSurface);
    }

    auto alreadyCarried = [&](CollisionRect* rect) {
        for (const CarriedObject& carried : carryList) {
            if (carried.body == rect) return true;
        }
        return false;
    };

    auto wasAlreadyCarried = [&](CollisionRect* rect) {
        GObject* owner = rect ? rect->getFather() : nullptr;
        if (!owner) return false;

        for (int carriedId : carriedOwnerIds_) {
            if (carriedId == owner->id_) return true;
        }
        return false;
    };

    for (size_t surfaceIndex = 0; surfaceIndex < platformSurfaces.size(); ++surfaceIndex) {
        Rectangle platformSurface = platformSurfaces[surfaceIndex];
        Rectangle topProbe = platformSurface;
        topProbe.y -= 2.0f;
        topProbe.height = 4.0f;

        for (auto* rect : CollisionRect::query(topProbe)) {
            if (!rect || rect == body_) continue;
            if (alreadyCarried(rect)) continue;
            if (!canBeCarriedByPlatform(rect, this)) continue;
            if (platformInTransit && isTileBody(rect) && !wasAlreadyCarried(rect)) continue;

            Rectangle objRect = rect->getSurface();
            float objBottom = objRect.y + objRect.height;
            float platTop = platformSurface.y;

            bool onPlatform = (objBottom >= platTop - 1.0f && objBottom <= platTop + 2.0f &&
                               objRect.x < platformSurface.x + platformSurface.width &&
                               objRect.x + objRect.width > platformSurface.x);

            if (onPlatform) {
                carryList.push_back(CarriedObject{
                    rect,
                    {objRect.x - platformSurface.x, objRect.y - platformSurface.y},
                    surfaceIndex > 0
                });
            }
        }
    }
    return carryList;
}

void Plateforme::rememberCarriedObjects(const std::vector<CarriedObject>& objects) {
    carriedOwnerIds_.clear();

    for (const CarriedObject& carried : objects) {
        GObject* owner = carried.body ? carried.body->getFather() : nullptr;
        if (!owner) continue;

        bool alreadyTracked = false;
        for (int carriedId : carriedOwnerIds_) {
            if (carriedId == owner->id_) {
                alreadyTracked = true;
                break;
            }
        }
        if (!alreadyTracked) carriedOwnerIds_.push_back(owner->id_);
    }
}

void Plateforme::attachCarriedObjects(const std::vector<CarriedObject>& objects) {
    Rectangle sourceSurface = body_->getSurface();
    std::optional<Rectangle> targetSurface = Portal::getTransitTargetSurface(this);

    for (const CarriedObject& carried : objects) {
        CollisionRect* rect = carried.body;
        if (!canBeCarriedByPlatform(rect, this)) continue;

        Rectangle platformSurface = carried.targetSide && targetSurface.has_value()
            ? *targetSurface
            : sourceSurface;

        Rectangle newR = rect->getSurface();
        newR.x = platformSurface.x + carried.offset.x;
        newR.y = platformSurface.y + carried.offset.y;

        Rectangle supportPoint{
            newR.x + newR.width / 2.0f,
            newR.y + newR.height - 0.5f,
            1.0f,
            1.0f
        };
        if (!carried.targetSide &&
            targetSurface.has_value() &&
            !Portal::isTransitSourceVisible(this, supportPoint)) {
            if (auto targetRect = Portal::transformTransitRect(this, newR)) {
                newR = *targetRect;
            }
        }

        Portal::cancelTransit(rect->getFather());
        rect->setSurface(newR);

        Character* ch = dynamic_cast<Character*>(rect->getFather());
        if (ch) {
            Vector2 sp = ch->body_->getSpeed();
            if (sp.y != 0.0f) ch->body_->setSpeed({ sp.x, 0.0f });
        }
    }
}

bool Plateforme::shouldSwitchDirection() {
    if (behavior_ == Behavior::LOOP) return false; // Loop never switches ping-pong direction
    return (current_ == 0 || current_ == (int)waypoints_.size() - 1);
}

void Plateforme::switchDirection() {
    dir_ = -dir_;
    waiting_ = waitTime_;
}

void Plateforme::snapToTarget(Vector2 target) {
    Rectangle surf = body_->getSurface();
    float tlx = target.x - surf.width/2.0f;
    float tly = target.y - surf.height/2.0f;
    body_->setCoord({ tlx, tly });
}

Vector2 Plateforme::calculateMovement(Vector2 currentCenter, double deltaTime) {
    if (waypoints_.size() < 2) return currentCenter;

    Vector2 target = getCurrentTarget();
    Vector2 toTarget { target.x - currentCenter.x, target.y - currentCenter.y };
    float dist = sqrtf(toTarget.x*toTarget.x + toTarget.y*toTarget.y);
    
    if (dist < 0.5f) {
        snapToTarget(target);
        
        if (behavior_ == Behavior::PING_PONG) {
            current_ += dir_;
            if (shouldSwitchDirection()) {
                switchDirection();
            }
        } else { // LOOP
            current_++; 
            if (current_ >= waypoints_.size()) current_ = 0;
            // Immediate transition to next waypoint
        }
        return target;
    }

    Vector2 newCenter = currentCenter;
    float desired = speed_ * (float)deltaTime;
    float remainingInSegment = dist;

    while (desired > 0.00001f && remainingInSegment > 0.00001f) {
        float step = std::min(desired, remainingInSegment);
        Vector2 dir { toTarget.x / remainingInSegment, toTarget.y / remainingInSegment };
        Vector2 continuousDelta { dir.x * step, dir.y * step };
        
        // Accumulate per-axis for pixel-perfect movement
        accX_ += continuousDelta.x;
        accY_ += continuousDelta.y;
        
        int moveX = (int)std::floor(accX_ + (accX_ > 0 ? 0.00001f : -0.00001f));
        int moveY = (int)std::floor(accY_ + (accY_ > 0 ? 0.00001f : -0.00001f));
        
        if (moveX != 0) accX_ -= (float)moveX;
        if (moveY != 0) accY_ -= (float)moveY;

        if (moveX != 0 || moveY != 0) {
            newCenter.x += (float)moveX;
            newCenter.y += (float)moveY;
        }

        desired -= step;
        toTarget = { target.x - newCenter.x, target.y - newCenter.y };
        remainingInSegment = sqrtf(toTarget.x*toTarget.x + toTarget.y*toTarget.y);

        if (remainingInSegment < 0.5f) {
            snapToTarget(target);
            newCenter = target;
            
            if (behavior_ == Behavior::PING_PONG) {
                current_ += dir_;
                if (shouldSwitchDirection()) {
                    switchDirection();
                    if (waiting_ > 0) {
                        desired = 0;
                        break;
                    }
                }
            } else { // LOOP
                current_++;
                if (current_ >= waypoints_.size()) current_ = 0;
            }
            
            target = getCurrentTarget();
            toTarget = { target.x - newCenter.x, target.y - newCenter.y };
            remainingInSegment = sqrtf(toTarget.x*toTarget.x + toTarget.y*toTarget.y);
        }
    }
    
    return newCenter;
}

void Plateforme::routine() {
    BasicEnt::routine();
    
    // Only move if platform is enabled
    if (!enabled_) return;
    
    if (waypoints_.size() < 2) return;
    
    double deltaTime = Clock::getLap();
    Vector2 currentCenter = getCurrentCenter();

    // Handle waiting period at endpoints
    if (waiting_ > 0) {
        waiting_ -= (float)deltaTime;
        if (waiting_ < 0) waiting_ = 0;
        lastCenter_ = currentCenter;
        return;
    }

    auto ridingObjects = findRidingObjects();

    // Calculate new position
    Vector2 newCenter = calculateMovement(currentCenter, deltaTime);
    Vector2 deltaMove { newCenter.x - currentCenter.x, newCenter.y - currentCenter.y };

    // Apply movement to platform
    if (fabsf(deltaMove.x) > 0.0001f || fabsf(deltaMove.y) > 0.0001f) {
        moveBodyThroughPortals(this, body_, deltaMove);
    }

    attachCarriedObjects(ridingObjects);
    rememberCarriedObjects(ridingObjects);
    
    lastCenter_ = newCenter;
}

// Public interface methods
void Plateforme::setEnabled(bool enabled) {
    enabled_ = enabled;
}

bool Plateforme::isEnabled() const {
    return enabled_;
}

void Plateforme::draw() {
    BasicEnt::draw();
    
    if (adiComponent_) {
        Vector2 pos = body_->getCoord();
        int stored = adiComponent_->getStoredAdi();
        DrawText(TextFormat("P:%d", stored), (int)pos.x, (int)pos.y - 10, 8, stored > 0 ? YELLOW : GRAY);
    }
}
