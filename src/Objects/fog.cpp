#include "fog.h"

#include <vector>

#include "Managers/shader_m.h"
#include "collisionRect.h"

namespace {

std::vector<Rectangle> visibleFogAreas;

void registerArea(Rectangle rect) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) return;
    visibleFogAreas.push_back(rect);
}

} // namespace

Fog::Fog(const SpawnData& data)
    : GObject(data.id) {
    CollisionDesc col = data.physics.collision.value_or(CollisionDesc{});
    body_ = new CollisionRect(col, this);
    if (body_) {
        body_->setSolid(false);
    }
}

Fog::~Fog() {
    delete body_;
}

void Fog::draw() {
    if (body_) {
        registerArea(body_->getSurface());
    }
}

void Fog::drawAtBody(Rectangle bodyRect) {
    registerArea(bodyRect);
}

Rectangle Fog::getRect() {
    return body_ ? body_->getSurface() : Rectangle{0.0f, 0.0f, 0.0f, 0.0f};
}

void Fog::beginFrame() {
    visibleFogAreas.clear();
}

void Fog::flushFogPasses() {
    Shader_m::applyFogAreasToScene(visibleFogAreas);
    visibleFogAreas.clear();
}

void Fog::clear() {
    visibleFogAreas.clear();
}
