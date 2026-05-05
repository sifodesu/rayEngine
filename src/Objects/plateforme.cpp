#include "plateforme.h"
#include "clock.h"
#include "character.h"
#include "collisionRect.h"
#include "adiComponent.h"
#include "object_m.h"
#include "portal.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr float kSupportTolerance = 2.0f;
constexpr double kMaxPlatformStepDelta = 1.0 / 60.0;
constexpr double kMaxPlatformCatchUpDelta = 0.1;

double platformMovementDelta(double deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0) return 0.0;
    return std::min(deltaTime, kMaxPlatformCatchUpDelta);
}

int consumePixelAccumulator(float& accumulator) {
    constexpr float eps = 0.00001f;

    if (accumulator >= 1.0f - eps) {
        int move = (int)std::floor(accumulator + eps);
        accumulator -= (float)move;
        return move;
    }

    if (accumulator <= -1.0f + eps) {
        int move = (int)std::ceil(accumulator - eps);
        accumulator -= (float)move;
        return move;
    }

    return 0;
}

float rectOverlap(float aMin, float aMax, float bMin, float bMax) {
    return std::min(aMax, bMax) - std::max(aMin, bMin);
}

float vectorLength(Vector2 value) {
    return sqrtf(value.x * value.x + value.y * value.y);
}

GravityDirection gravityDirectionFor(CollisionRect* rect) {
    RigidBody* rigid = dynamic_cast<RigidBody*>(rect);
    return rigid ? rigid->getGravityDirection() : GravityDirection::DOWN;
}

Rectangle expandedPlatformQuery(Rectangle platformSurface) {
    return Rectangle{
        platformSurface.x - kSupportTolerance,
        platformSurface.y - kSupportTolerance,
        platformSurface.width + 2.0f * kSupportTolerance,
        platformSurface.height + 2.0f * kSupportTolerance
    };
}

bool isSupportedByPlatform(Rectangle objRect, Rectangle platformSurface, GravityDirection gravityDir) {
    switch (gravityDir) {
        case GravityDirection::DOWN: {
            float feetToTop = fabsf((objRect.y + objRect.height) - platformSurface.y);
            return feetToTop <= kSupportTolerance &&
                   rectOverlap(objRect.x, objRect.x + objRect.width,
                               platformSurface.x, platformSurface.x + platformSurface.width) > 0.5f;
        }
        case GravityDirection::UP: {
            float headToBottom = fabsf(objRect.y - (platformSurface.y + platformSurface.height));
            return headToBottom <= kSupportTolerance &&
                   rectOverlap(objRect.x, objRect.x + objRect.width,
                               platformSurface.x, platformSurface.x + platformSurface.width) > 0.5f;
        }
        case GravityDirection::LEFT: {
            float leftToRight = fabsf(objRect.x - (platformSurface.x + platformSurface.width));
            return leftToRight <= kSupportTolerance &&
                   rectOverlap(objRect.y, objRect.y + objRect.height,
                               platformSurface.y, platformSurface.y + platformSurface.height) > 0.5f;
        }
        case GravityDirection::RIGHT: {
            float rightToLeft = fabsf((objRect.x + objRect.width) - platformSurface.x);
            return rightToLeft <= kSupportTolerance &&
                   rectOverlap(objRect.y, objRect.y + objRect.height,
                               platformSurface.y, platformSurface.y + platformSurface.height) > 0.5f;
        }
    }
    return false;
}

Rectangle supportPoint(Rectangle rect, GravityDirection gravityDir) {
    switch (gravityDir) {
        case GravityDirection::DOWN:
            return Rectangle{rect.x + rect.width / 2.0f, rect.y + rect.height - 0.5f, 1.0f, 1.0f};
        case GravityDirection::UP:
            return Rectangle{rect.x + rect.width / 2.0f, rect.y - 0.5f, 1.0f, 1.0f};
        case GravityDirection::LEFT:
            return Rectangle{rect.x - 0.5f, rect.y + rect.height / 2.0f, 1.0f, 1.0f};
        case GravityDirection::RIGHT:
            return Rectangle{rect.x + rect.width - 0.5f, rect.y + rect.height / 2.0f, 1.0f, 1.0f};
    }
    return Rectangle{rect.x + rect.width / 2.0f, rect.y + rect.height - 0.5f, 1.0f, 1.0f};
}

void cancelFallIntoSupport(CollisionRect* rect) {
    RigidBody* rigid = dynamic_cast<RigidBody*>(rect);
    if (!rigid) return;

    Vector2 speed = rigid->getSpeed();
    switch (rigid->getGravityDirection()) {
        case GravityDirection::DOWN:
            if (speed.y > 0.0f) rigid->setSpeed({speed.x, 0.0f});
            break;
        case GravityDirection::UP:
            if (speed.y < 0.0f) rigid->setSpeed({speed.x, 0.0f});
            break;
        case GravityDirection::LEFT:
            if (speed.x < 0.0f) rigid->setSpeed({0.0f, speed.y});
            break;
        case GravityDirection::RIGHT:
            if (speed.x > 0.0f) rigid->setSpeed({0.0f, speed.y});
            break;
    }
}

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
        Rectangle supportProbe = expandedPlatformQuery(platformSurface);

        for (auto* rect : CollisionRect::query(supportProbe)) {
            if (!rect || rect == body_) continue;
            if (alreadyCarried(rect)) continue;
            if (!canBeCarriedByPlatform(rect, this)) continue;
            if (platformInTransit && isTileBody(rect) && !wasAlreadyCarried(rect)) continue;

            Rectangle objRect = rect->getSurface();
            if (isSupportedByPlatform(objRect, platformSurface, gravityDirectionFor(rect))) {
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

        if (!carried.targetSide &&
            targetSurface.has_value() &&
            !Portal::isTransitSourceVisible(this, supportPoint(newR, gravityDirectionFor(rect)))) {
            if (auto targetRect = Portal::transformTransitRect(this, newR)) {
                newR = *targetRect;
            }
        }

        Portal::cancelTransit(rect->getFather());
        rect->setSurface(newR);
        cancelFallIntoSupport(rect);
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

void Plateforme::advanceWaypoint() {
    if (behavior_ == Behavior::PING_PONG) {
        current_ += dir_;
        if (shouldSwitchDirection()) {
            switchDirection();
        }
    } else {
        current_++;
        if (current_ >= (int)waypoints_.size()) current_ = 0;
    }
}

void Plateforme::applyMovementDelta(Vector2 deltaMove) {
    if (fabsf(deltaMove.x) <= 0.0001f && fabsf(deltaMove.y) <= 0.0001f) return;

    auto ridingObjects = findRidingObjects();
    moveBodyThroughPortals(this, body_, deltaMove);
    attachCarriedObjects(ridingObjects);
    rememberCarriedObjects(ridingObjects);
    lastCenter_ = getCurrentCenter();
}

void Plateforme::advanceMovement(double deltaTime) {
    if (waypoints_.size() < 2) return;

    float desired = speed_ * (float)deltaTime;
    const int maxIterations = (int)waypoints_.size() * 4 + 8;
    int iterations = 0;

    while (desired > 0.00001f && waiting_ <= 0.0f && iterations++ < maxIterations) {
        Vector2 currentCenter = getCurrentCenter();
        Vector2 target = getCurrentTarget();
        Vector2 toTarget{target.x - currentCenter.x, target.y - currentCenter.y};
        float remainingInSegment = vectorLength(toTarget);

        if (remainingInSegment < 0.5f) {
            Vector2 snapDelta{target.x - currentCenter.x, target.y - currentCenter.y};
            applyMovementDelta(snapDelta);
            accX_ = 0.0f;
            accY_ = 0.0f;
            desired = std::max(0.0f, desired - remainingInSegment);
            advanceWaypoint();
            continue;
        }

        float step = std::min(desired, remainingInSegment);
        Vector2 dir{toTarget.x / remainingInSegment, toTarget.y / remainingInSegment};
        Vector2 continuousDelta { dir.x * step, dir.y * step };
        
        // Accumulate per-axis for pixel-perfect movement
        accX_ += continuousDelta.x;
        accY_ += continuousDelta.y;
        
        int moveX = consumePixelAccumulator(accX_);
        int moveY = consumePixelAccumulator(accY_);

        if (moveX != 0 || moveY != 0) {
            applyMovementDelta({(float)moveX, (float)moveY});
        }

        desired -= step;

        currentCenter = getCurrentCenter();
        toTarget = {target.x - currentCenter.x, target.y - currentCenter.y};
        remainingInSegment = vectorLength(toTarget);
        if (remainingInSegment < 0.5f) {
            Vector2 snapDelta{target.x - currentCenter.x, target.y - currentCenter.y};
            applyMovementDelta(snapDelta);
            accX_ = 0.0f;
            accY_ = 0.0f;
            advanceWaypoint();
        }
    }
}

void Plateforme::routine() {
    BasicEnt::routine();
    
    // Only move if platform is enabled
    if (!enabled_) return;
    
    if (waypoints_.size() < 2) return;
    
    double rawDeltaTime = Clock::getLap();
    if (!std::isfinite(rawDeltaTime) || rawDeltaTime <= 0.0) return;

    Vector2 currentCenter = getCurrentCenter();

    // Handle waiting period at endpoints
    if (waiting_ > 0) {
        waiting_ -= (float)rawDeltaTime;
        if (waiting_ < 0) waiting_ = 0;
        rememberCarriedObjects(findRidingObjects());
        lastCenter_ = currentCenter;
        return;
    }

    double movementTime = platformMovementDelta(rawDeltaTime);
    while (movementTime > 0.0 && waiting_ <= 0.0f) {
        double stepDelta = std::min(movementTime, kMaxPlatformStepDelta);
        advanceMovement(stepDelta);
        movementTime -= stepDelta;
    }
    
    rememberCarriedObjects(findRidingObjects());
    lastCenter_ = getCurrentCenter();
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
