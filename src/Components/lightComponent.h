#pragma once

#include "raylib.h"
#include "spawn.h"

class GObject;

class LightComponent {
public:
    LightComponent(GObject* owner, const LightDesc& desc);
    ~LightComponent();

    LightDesc& desc() { return desc_; }
    const LightDesc& desc() const { return desc_; }

    bool isEnabled() const;
    Vector2 worldPosition() const;
    Rectangle worldBounds() const;

private:
    GObject* owner_ = nullptr;
    LightDesc desc_{};
};
