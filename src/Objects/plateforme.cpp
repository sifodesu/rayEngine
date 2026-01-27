#include "plateforme.h"
#include "clock.h"
#include "character.h"
#include "collisionRect.h"
#include "adiComponent.h"
#include "killComponent.h"


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

    // Initialize KillComponent if "Kill" property is set
    if (data.interaction.isKill.value_or(false)) {
        killComponent_ = new KillComponent();
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
    delete killComponent_;
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

std::vector<CollisionRect*> Plateforme::findRidingCharacters(const Rectangle& platformSurface) const {
    std::vector<CollisionRect*> carryList;
    Rectangle topProbe = platformSurface;
    topProbe.y -= 2.0f;            // small margin above
    topProbe.height = 4.0f;        // probe just above + a bit inside the platform

    for (auto* rect : CollisionRect::query(topProbe)) {
        if (!rect || rect == body_) continue;
        
        Character* character = dynamic_cast<Character*>(rect->getFather());
        if (!character) continue;
        
        Rectangle charRect = rect->getSurface();
        float charBottom = charRect.y + charRect.height;
        float platTop = platformSurface.y;
        
        // Check if character is standing on platform
        bool onPlatform = (charBottom >= platTop - 1.0f && charBottom <= platTop + 2.0f &&
                          charRect.x < platformSurface.x + platformSurface.width &&
                          charRect.x + charRect.width > platformSurface.x);
        
        if (onPlatform) {
            carryList.push_back(rect);
        }
    }
    return carryList;
}

void Plateforme::moveCarriedCharacters(const std::vector<CollisionRect*>& characters, Vector2 deltaMove, Vector2 newCenter) {
    Rectangle surf = body_->getSurface();
    
    for (auto* rect : characters) {
        Rectangle cr = rect->getSurface();
        rect->setCoord({ cr.x + deltaMove.x, cr.y + deltaMove.y });
        
        Character* ch = dynamic_cast<Character*>(rect->getFather());
        if (ch) {
            Rectangle newR = rect->getSurface();
            float platTop = newCenter.y - surf.height/2.0f;
            float diff = (newR.y + newR.height) - platTop; // feet - platTop
            
            // Snap feet to platform and stop vertical movement
            if (diff >= -0.6f && diff <= 2.0f) {
                float desiredY = platTop - newR.height;
                if (fabsf(newR.y - desiredY) > 0.01f) {
                    rect->setCoord({ newR.x, desiredY });
                }
                Vector2 sp = ch->body_->getSpeed();
                if (sp.y != 0.0f) ch->body_->setSpeed({ sp.x, 0.0f });
            }
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
    
    // Check collisions for KillComponent (spikes)
    // We manually check for overlap because onCollision might not trigger
    // if the physics engine prevents deep interpenetration for solids.
    if (killComponent_) {
        Rectangle surf = body_->getSurface();
        // Expand slightly to catch touching characters
        Rectangle killZone = { surf.x - 1, surf.y - 1, surf.width + 2, surf.height + 2 };
        
        for (auto* other : CollisionRect::query(killZone)) {
            if (other == body_) continue;
            if (CheckCollisionRecs(killZone, other->getSurface())) {
                killComponent_->onCollision(other->getFather());
            }
        }
    }
    
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

    // Find characters riding on the platform before movement
    Rectangle platformSurface = body_->getSurface();
    auto ridingCharacters = findRidingCharacters(platformSurface);

    // Calculate new position
    Vector2 newCenter = calculateMovement(currentCenter, deltaTime);
    Vector2 deltaMove { newCenter.x - currentCenter.x, newCenter.y - currentCenter.y };

    // Apply movement to platform
    if (fabsf(deltaMove.x) > 0.0001f || fabsf(deltaMove.y) > 0.0001f) {
        Rectangle surf = body_->getSurface();
        body_->setCoord({ newCenter.x - surf.width/2.0f, newCenter.y - surf.height/2.0f });
    }

    // Move carried characters
    moveCarriedCharacters(ridingCharacters, deltaMove, newCenter);
    
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
