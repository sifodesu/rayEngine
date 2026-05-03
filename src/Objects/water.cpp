#include "water.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "Managers/shader_m.h"
#include "collisionRect.h"

namespace {

struct WaterSurface {
    Rectangle rect;
    WaterVisualKind kind;
};

std::vector<WaterSurface> visibleSurfaces;

bool nearlyEqual(float a, float b) {
    return std::fabs(a - b) <= 0.01f;
}

WaterVisualKind kindFromTypeDetail(const std::string& detail) {
    std::string lower;
    lower.reserve(detail.size());
    for (unsigned char c : detail) {
        lower.push_back(static_cast<char>(std::tolower(c)));
    }

    if (lower == "waterfall" || lower == "cascade") {
        return WaterVisualKind::Waterfall;
    }
    return WaterVisualKind::Still;
}

void registerSurface(Rectangle rect, WaterVisualKind kind) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) return;
    visibleSurfaces.push_back({ rect, kind });
}

std::vector<WaterSurface> mergeStillWater(std::vector<WaterSurface> surfaces) {
    std::sort(surfaces.begin(), surfaces.end(), [](const WaterSurface& a, const WaterSurface& b) {
        if (!nearlyEqual(a.rect.y, b.rect.y)) return a.rect.y < b.rect.y;
        if (!nearlyEqual(a.rect.height, b.rect.height)) return a.rect.height < b.rect.height;
        return a.rect.x < b.rect.x;
    });

    std::vector<WaterSurface> merged;
    for (const WaterSurface& surface : surfaces) {
        if (!merged.empty()) {
            WaterSurface& last = merged.back();
            const float lastRight = last.rect.x + last.rect.width;
            if (last.kind == surface.kind &&
                nearlyEqual(last.rect.y, surface.rect.y) &&
                nearlyEqual(last.rect.height, surface.rect.height) &&
                nearlyEqual(lastRight, surface.rect.x)) {
                last.rect.width += surface.rect.width;
                continue;
            }
        }
        merged.push_back(surface);
    }
    return merged;
}

std::vector<WaterSurface> mergeWaterfalls(std::vector<WaterSurface> surfaces) {
    std::sort(surfaces.begin(), surfaces.end(), [](const WaterSurface& a, const WaterSurface& b) {
        if (!nearlyEqual(a.rect.x, b.rect.x)) return a.rect.x < b.rect.x;
        if (!nearlyEqual(a.rect.width, b.rect.width)) return a.rect.width < b.rect.width;
        return a.rect.y < b.rect.y;
    });

    std::vector<WaterSurface> merged;
    for (const WaterSurface& surface : surfaces) {
        if (!merged.empty()) {
            WaterSurface& last = merged.back();
            const float lastBottom = last.rect.y + last.rect.height;
            if (last.kind == surface.kind &&
                nearlyEqual(last.rect.x, surface.rect.x) &&
                nearlyEqual(last.rect.width, surface.rect.width) &&
                nearlyEqual(lastBottom, surface.rect.y)) {
                last.rect.height += surface.rect.height;
                continue;
            }
        }
        merged.push_back(surface);
    }
    return merged;
}

} // namespace

Water::Water(const SpawnData& data)
    : GObject(data.id), kind_(kindFromTypeDetail(data.typeDetail)) {
    CollisionDesc col = data.physics.collision.value_or(CollisionDesc{});
    body_ = new CollisionRect(col, this);
    if (body_) {
        body_->setSolid(false);
    }
}

Water::~Water() {
    delete body_;
}

void Water::draw() {
    if (body_) {
        registerSurface(body_->getSurface(), kind_);
    }
}

void Water::drawAtBody(Rectangle bodyRect) {
    registerSurface(bodyRect, kind_);
}

Rectangle Water::getRect() {
    return body_ ? body_->getSurface() : Rectangle{0.0f, 0.0f, 0.0f, 0.0f};
}

void Water::beginFrame() {
    visibleSurfaces.clear();
}

void Water::drawOcclusionMasks() {
    const Color color = Shader_m::waterParams().stillOcclusionColor;
    for (const WaterSurface& surface : visibleSurfaces) {
        if (surface.kind != WaterVisualKind::Still) continue;
        DrawRectangleRec(surface.rect, color);
    }
}

void Water::flushRefractionPasses() {
    std::vector<WaterSurface> still;
    std::vector<WaterSurface> waterfalls;
    still.reserve(visibleSurfaces.size());
    waterfalls.reserve(visibleSurfaces.size());

    for (const WaterSurface& surface : visibleSurfaces) {
        if (surface.kind == WaterVisualKind::Waterfall) {
            waterfalls.push_back(surface);
        } else {
            still.push_back(surface);
        }
    }

    for (const WaterSurface& surface : mergeStillWater(std::move(still))) {
        Shader_m::addWaterArea(surface.rect, static_cast<int>(surface.kind));
    }
    for (const WaterSurface& surface : mergeWaterfalls(std::move(waterfalls))) {
        Shader_m::addWaterArea(surface.rect, static_cast<int>(surface.kind));
    }

    visibleSurfaces.clear();
}

void Water::clear() {
    visibleSurfaces.clear();
}
