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

void Plateforme::routine() {
    // Run base (sprite anim)
    BasicEnt::routine();
    if (waypoints_.size() < 2) return;
    double dt = Clock::getLap();

    Rectangle surf = body_->getSurface();
    Vector2 center { surf.x + surf.width/2.0f, surf.y + surf.height/2.0f };

    if (waiting_ > 0) {
        waiting_ -= (float)dt;
        if (waiting_ < 0) waiting_ = 0;
        lastCenter_ = center; // keep center baseline
        return;
    }

    Vector2 target = waypoints_[current_ + dir_];
    Vector2 toTarget { target.x - center.x, target.y - center.y };
    float dist = sqrtf(toTarget.x*toTarget.x + toTarget.y*toTarget.y);

    // Gather characters standing on platform BEFORE movement
    std::vector<CollisionRect*> carryList;
    Rectangle topProbe = surf; 
    topProbe.y -= 4; // Check a bit higher to catch characters properly
    topProbe.height = 6; // Larger probe area for more reliable detection
    
    for (auto* rect : CollisionRect::query(topProbe)) {
        if (!rect || rect == body_) continue;
        Character* character = dynamic_cast<Character*>(rect->getFather());
        if (!character) continue;
        
        Rectangle charRect = rect->getSurface();
        // Check if character is standing on top (bottom of character touching top of platform)
        if (charRect.y + charRect.height >= surf.y - 2 && 
            charRect.y + charRect.height <= surf.y + 4 &&
            charRect.x < surf.x + surf.width && 
            charRect.x + charRect.width > surf.x) {
            carryList.push_back(rect);
        }
    }

    if (dist < 0.5f) {
        // Snap exactly to target to avoid residual jitter
        float tlx = target.x - surf.width/2.0f;
        float tly = target.y - surf.height/2.0f;
        body_->setCoord({ tlx, tly });
        center = target;
        current_ += dir_;
        if (current_ == 0 || current_ == (int)waypoints_.size() - 1) { dir_ = -dir_; waiting_ = waitTime_; }
        lastCenter_ = center;
        return;
    }

    float step = speed_ * (float)dt;
    if (step > dist) step = dist; // clamp
    Vector2 moveDir { toTarget.x / dist, toTarget.y / dist };
    Vector2 newCenter { center.x + moveDir.x * step, center.y + moveDir.y * step };
    Vector2 deltaMove { newCenter.x - center.x, newCenter.y - center.y };

    // Apply platform motion
    body_->setCoord({ newCenter.x - surf.width/2.0f, newCenter.y - surf.height/2.0f });

    // Move carried characters by same delta
    for (auto* rect : carryList) {
        Rectangle cr = rect->getSurface();
        rect->setCoord({ cr.x + deltaMove.x, cr.y + deltaMove.y });
    }

    lastCenter_ = newCenter;
}
