#include "door.h"

#include "triggerButton.h"

#include <algorithm>

std::unordered_map<std::string, bool> Door::persistentOpen_;

Door::Door(const SpawnData& data)
    : BasicEnt(data)
    , buttonIds_(data.ldtk.targetIds.value_or(std::vector<std::string>{})) {
    registryId_ = data.ldtk.linkId.value_or(data.ldtk.iid.value_or(std::to_string(id_)));
    if (body_) body_->setSolid(true);
    auto it = persistentOpen_.find(registryId_);
    if (it != persistentOpen_.end() && it->second) {
        setOpen(true);
    }
}

void Door::routine() {
    BasicEnt::routine();
    if (!open_ && shouldOpen()) {
        setOpen(true);
    }
}

void Door::draw() {
    if (!body_) return;

    Rectangle rect = body_->getSurface();
    if (open_) {
        DrawRectangleLinesEx(rect, 1.0f, Color{112, 116, 104, 90});
        drawIndicators(rect);
        return;
    }

    DrawRectangleRec(rect, Color{24, 27, 24, 255});
    DrawRectangleLinesEx(rect, 1.0f, Color{128, 128, 108, 255});

    float midX = rect.x + rect.width * 0.5f;
    DrawLineV(Vector2{midX, rect.y + 1.0f}, Vector2{midX, rect.y + rect.height - 1.0f}, Color{72, 76, 68, 255});

    int slatCount = std::max(1, static_cast<int>(rect.height / 8.0f));
    for (int i = 1; i < slatCount; ++i) {
        float y = rect.y + i * 8.0f;
        if (y >= rect.y + rect.height) break;
        DrawLineV(Vector2{rect.x + 1.0f, y}, Vector2{rect.x + rect.width - 1.0f, y}, Color{72, 76, 68, 255});
    }

    drawIndicators(rect);
}

bool Door::shouldOpen() const {
    if (buttonIds_.empty()) return false;

    for (const std::string& buttonId : buttonIds_) {
        if (!TriggerButton::isButtonActivated(buttonId)) {
            return false;
        }
    }
    return true;
}

void Door::setOpen(bool open) {
    if (open_ == open) return;

    open_ = open;
    if (open_ && !registryId_.empty()) {
        persistentOpen_[registryId_] = true;
    }
    if (body_) {
        body_->setSolid(!open_);
    }
}

void Door::clearPersistentState() {
    persistentOpen_.clear();
}

bool Door::isIndicatorLit(std::size_t index) const {
    if (open_) return true;
    if (index >= buttonIds_.size()) return false;
    return TriggerButton::isButtonActivated(buttonIds_[index]);
}

void Door::drawIndicators(Rectangle rect) const {
    if (buttonIds_.empty()) return;

    float count = static_cast<float>(buttonIds_.size());
    float slotHeight = std::max(1.0f, (rect.height - 2.0f) / count);
    float lightSize = std::clamp(std::min(rect.width - 2.0f, slotHeight - 1.0f), 1.0f, 3.0f);
    float x = rect.x + rect.width - lightSize - 1.0f;

    for (std::size_t i = 0; i < buttonIds_.size(); ++i) {
        float y = rect.y + 1.0f + static_cast<float>(i) * slotHeight + std::max((slotHeight - lightSize) * 0.5f, 0.0f);
        Rectangle light{x, y, lightSize, lightSize};
        bool lit = isIndicatorLit(i);
        DrawRectangleRec(light, lit ? Color{255, 220, 76, 255} : Color{42, 46, 40, 255});
        DrawRectangleLinesEx(light, 1.0f, lit ? Color{255, 248, 180, 255} : Color{92, 96, 84, 255});
    }
}
