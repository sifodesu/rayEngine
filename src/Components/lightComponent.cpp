#include "lightComponent.h"

#include <algorithm>

#include "gObject.h"
#include "light_m.h"

LightComponent::LightComponent(GObject* owner, const LightDesc& desc)
    : owner_(owner), desc_(desc) {
    Light_m::registerComponent(this);
}

LightComponent::~LightComponent() {
    Light_m::unregisterComponent(this);
}

bool LightComponent::isEnabled() const {
    return owner_ && desc_.enabled && desc_.radius > 0.0f && desc_.intensity > 0.0f;
}

Vector2 LightComponent::worldPosition() const {
    if (!owner_) return desc_.offset;

    Rectangle rect = owner_->getRect();
    return {
        rect.x + rect.width * 0.5f + desc_.offset.x,
        rect.y + rect.height * 0.5f + desc_.offset.y
    };
}

Rectangle LightComponent::worldBounds() const {
    Vector2 pos = worldPosition();
    float radius = std::max(desc_.radius, 0.0f);
    return {
        pos.x - radius,
        pos.y - radius,
        radius * 2.0f,
        radius * 2.0f
    };
}
