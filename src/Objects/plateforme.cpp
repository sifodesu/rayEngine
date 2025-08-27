#include "plateforme.h"
#include "clock.h"
#include "character.h"
#include "collisionRect.h"

Plateforme::Plateforme(const SpawnData& data) : BasicEnt(data) {
    // Starting center
    Rectangle surf = body_->getSurface();
    Vector2 startCenter { surf.x + surf.width/2.0f, surf.y + surf.height/2.0f };
    waypoints_.push_back(startCenter);
    lastCenter_ = startCenter;
    
    float cell = (data.layerGridSize ? (float)*data.layerGridSize : 8.0f);
    if (data.pathPoints && !data.pathPoints->empty()) {
        for (auto cellPt : *data.pathPoints) {
            // cellPt is in cell coordinates; convert to pixel center of that cell
            Vector2 worldCenter { cellPt.x * cell + cell/2.0f, cellPt.y * cell + cell/2.0f };
            waypoints_.push_back(worldCenter);
        }
    }
}

Vector2 Plateforme::getCurrentCenter() const {
    Rectangle surf = body_->getSurface();
    return { surf.x + surf.width/2.0f, surf.y + surf.height/2.0f };
}

Vector2 Plateforme::getCurrentTarget() const {
    if (waypoints_.size() < 2) return getCurrentCenter();
    return waypoints_[current_ + dir_];
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
    Vector2 target = getCurrentTarget();
    Vector2 toTarget { target.x - currentCenter.x, target.y - currentCenter.y };
    float dist = sqrtf(toTarget.x*toTarget.x + toTarget.y*toTarget.y);
    
    if (dist < 0.5f) {
        snapToTarget(target);
        current_ += dir_;
        if (shouldSwitchDirection()) {
            switchDirection();
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
            current_ += dir_;
            if (shouldSwitchDirection()) {
                switchDirection();
                if (waiting_ > 0) {
                    desired = 0;
                    break;
                }
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
