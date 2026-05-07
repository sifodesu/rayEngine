#include "triggerButton.h"

#include "character.h"
#include "collisionRect.h"

#include <algorithm>
#include <cmath>

std::unordered_map<std::string, TriggerButton*> TriggerButton::registry_;
std::unordered_map<std::string, bool> TriggerButton::persistentActivations_;

namespace {

float axisOverlap(float aMin, float aMax, float bMin, float bMax) {
    return std::min(aMax, bMax) - std::max(aMin, bMin);
}

Rectangle expanded(Rectangle rect, float amount) {
    rect.x -= amount;
    rect.y -= amount;
    rect.width += amount * 2.0f;
    rect.height += amount * 2.0f;
    return rect;
}

} // namespace

TriggerButton::TriggerButton(const SpawnData& data)
    : BasicEnt(data) {
    if (body_) body_->setSolid(false);

    staysActivated_ = data.interaction.staysActivated.value_or(false);
    registryId_ = data.ldtk.linkId.value_or(data.ldtk.iid.value_or(std::to_string(id_)));
    if (staysActivated_) {
        auto it = persistentActivations_.find(registryId_);
        activated_ = it != persistentActivations_.end() && it->second;
    }
    registerButton();
}

TriggerButton::~TriggerButton() {
    unregisterButton();
}

void TriggerButton::routine() {
    BasicEnt::routine();
    updateActivated(detectPressedCharacter());
}

void TriggerButton::onCollision(GObject* other) {
    auto* character = dynamic_cast<Character*>(other);
    if (!character) return;
    if (isCharacterPressing(*character)) {
        updateActivated(true);
    }
}

void TriggerButton::draw() {
    if (!body_) return;

    Rectangle rect = body_->getSurface();
    Color fill = activated_ ? Color{255, 220, 76, 255} : Color{88, 92, 84, 255};
    Color rim = activated_ ? Color{255, 248, 180, 255} : Color{140, 140, 118, 255};
    Color inset = activated_ ? Color{202, 139, 34, 255} : Color{36, 40, 36, 255};

    DrawRectangleRec(rect, fill);
    DrawRectangleLinesEx(rect, 1.0f, rim);
    Rectangle inner{
        rect.x + 1.0f,
        rect.y + 1.0f,
        std::max(rect.width - 2.0f, 1.0f),
        std::max(rect.height - 2.0f, 1.0f)
    };
    DrawRectangleRec(inner, inset);
}

bool TriggerButton::isButtonActivated(const std::string& id) {
    auto it = registry_.find(id);
    if (it != registry_.end() && it->second) {
        return it->second->isActivated();
    }

    auto persistentIt = persistentActivations_.find(id);
    return persistentIt != persistentActivations_.end() && persistentIt->second;
}

bool TriggerButton::getButtonRect(const std::string& id, Rectangle& out) {
    auto it = registry_.find(id);
    if (it == registry_.end() || !it->second || !it->second->body_) {
        return false;
    }

    out = it->second->body_->getSurface();
    return true;
}

void TriggerButton::clearPersistentState() {
    registry_.clear();
    persistentActivations_.clear();
}

void TriggerButton::updateActivated(bool activated) {
    if (staysActivated_ && activated_) return;
    activated_ = activated;
    if (staysActivated_ && activated_ && !registryId_.empty()) {
        persistentActivations_[registryId_] = true;
    }
}

bool TriggerButton::detectPressedCharacter() const {
    if (!body_) return false;

    Rectangle probe = expanded(body_->getSurface(), contactTolerance_);
    for (CollisionRect* rect : CollisionRect::query(probe)) {
        if (!rect || rect == body_) continue;
        auto* character = dynamic_cast<Character*>(rect->getFather());
        if (!character) continue;
        if (isCharacterPressing(*character)) return true;
    }
    return false;
}

bool TriggerButton::isCharacterPressing(const Character& character) const {
    if (!body_ || !character.body_) return false;

    Rectangle buttonRect = body_->getSurface();
    Rectangle charRect = character.body_->getSurface();

    float xOverlap = axisOverlap(charRect.x, charRect.x + charRect.width,
                                 buttonRect.x, buttonRect.x + buttonRect.width);
    float yOverlap = axisOverlap(charRect.y, charRect.y + charRect.height,
                                 buttonRect.y, buttonRect.y + buttonRect.height);

    GravityDirection gravityDir = character.body_->getGravityDirection();
    switch (gravityDir) {
        case GravityDirection::DOWN: {
            float feetY = charRect.y + charRect.height;
            return xOverlap > 0.5f &&
                   feetY >= buttonRect.y - contactTolerance_ &&
                   feetY <= buttonRect.y + buttonRect.height + contactTolerance_;
        }
        case GravityDirection::UP: {
            float headY = charRect.y;
            return xOverlap > 0.5f &&
                   headY >= buttonRect.y - contactTolerance_ &&
                   headY <= buttonRect.y + buttonRect.height + contactTolerance_;
        }
        case GravityDirection::LEFT: {
            float leftX = charRect.x;
            return yOverlap > 0.5f &&
                   leftX >= buttonRect.x - contactTolerance_ &&
                   leftX <= buttonRect.x + buttonRect.width + contactTolerance_;
        }
        case GravityDirection::RIGHT: {
            float rightX = charRect.x + charRect.width;
            return yOverlap > 0.5f &&
                   rightX >= buttonRect.x - contactTolerance_ &&
                   rightX <= buttonRect.x + buttonRect.width + contactTolerance_;
        }
    }
    return false;
}

void TriggerButton::registerButton() {
    if (!registryId_.empty()) {
        registry_[registryId_] = this;
    }
}

void TriggerButton::unregisterButton() {
    if (registryId_.empty()) return;
    auto it = registry_.find(registryId_);
    if (it != registry_.end() && it->second == this) {
        registry_.erase(it);
    }
}
