#include "light_m.h"

#include <algorithm>
#include <array>

#include "lightComponent.h"
#include "raycam_m.h"

std::vector<LightComponent*> Light_m::components_;

Light_m::Params& Light_m::params() {
    static Params params;
    return params;
}

void Light_m::resetParams() {
    params() = Params{};
}

void Light_m::registerComponent(LightComponent* component) {
    if (!component) return;
    if (std::find(components_.begin(), components_.end(), component) != components_.end()) return;
    components_.push_back(component);
}

void Light_m::unregisterComponent(LightComponent* component) {
    components_.erase(std::remove(components_.begin(), components_.end(), component), components_.end());
}

void Light_m::clear() {
    components_.clear();
}

int Light_m::activeCount() {
    int count = 0;
    for (LightComponent* component : components_) {
        if (component && component->isEnabled()) ++count;
    }
    return count;
}

bool Light_m::hasActiveLights() {
    if (!params().enabled) return false;
    for (LightComponent* component : components_) {
        if (!component || !component->isEnabled()) continue;
        return true;
    }
    return false;
}

void Light_m::upload(Shader shader) {
    Params& p = params();
    Camera2D cam = Raycam_m::getCam();
    Rectangle view = Raycam_m::getRayCam().getRect();

    std::array<float, MAX_SHADER_LIGHTS * 4> lightData{};
    std::array<float, MAX_SHADER_LIGHTS * 4> lightColors{};
    int count = 0;

    if (p.enabled) {
        for (LightComponent* component : components_) {
            if (count >= MAX_SHADER_LIGHTS) break;
            if (!component || !component->isEnabled()) continue;
            if (!CheckCollisionRecs(view, component->worldBounds())) continue;

            const LightDesc& desc = component->desc();
            Vector2 screenPos = GetWorldToScreen2D(component->worldPosition(), cam);
            float screenRadius = desc.radius * cam.zoom;
            if (screenRadius <= 0.0f) continue;

            int dataIndex = count * 4;
            lightData[dataIndex + 0] = screenPos.x;
            lightData[dataIndex + 1] = screenPos.y;
            lightData[dataIndex + 2] = screenRadius;
            lightData[dataIndex + 3] = desc.intensity * p.globalIntensity;

            lightColors[dataIndex + 0] = static_cast<float>(desc.color.r) / 255.0f;
            lightColors[dataIndex + 1] = static_cast<float>(desc.color.g) / 255.0f;
            lightColors[dataIndex + 2] = static_cast<float>(desc.color.b) / 255.0f;
            lightColors[dataIndex + 3] = static_cast<float>(desc.color.a) / 255.0f;
            ++count;
        }
    }

    int loc = GetShaderLocation(shader, "lightCount");
    if (loc >= 0) SetShaderValue(shader, loc, &count, SHADER_UNIFORM_INT);
    loc = GetShaderLocation(shader, "lights[0]");
    if (loc < 0) loc = GetShaderLocation(shader, "lights");
    if (loc >= 0 && count > 0) SetShaderValueV(shader, loc, lightData.data(), SHADER_UNIFORM_VEC4, count);
    loc = GetShaderLocation(shader, "lightColors[0]");
    if (loc < 0) loc = GetShaderLocation(shader, "lightColors");
    if (loc >= 0 && count > 0) SetShaderValueV(shader, loc, lightColors.data(), SHADER_UNIFORM_VEC4, count);
    loc = GetShaderLocation(shader, "ambientLight");
    if (loc >= 0) SetShaderValue(shader, loc, &p.ambient, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "lightFalloff");
    if (loc >= 0) SetShaderValue(shader, loc, &p.falloff, SHADER_UNIFORM_FLOAT);
}

void Light_m::drawDebug() {
    if (!params().debugDraw) return;

    for (LightComponent* component : components_) {
        if (!component || !component->isEnabled()) continue;
        Vector2 pos = component->worldPosition();
        float radius = component->desc().radius;
        DrawCircleLines(static_cast<int>(pos.x), static_cast<int>(pos.y), radius, YELLOW);
        DrawCircleV(pos, 2.0f, YELLOW);
    }
}
