#pragma once

#include <cstddef>
#include <vector>

#include "raylib.h"

class LightComponent;

class Light_m {
public:
    struct Params {
        bool enabled{true};
        bool debugDraw{false};
        float ambient{0.42f};
        float globalIntensity{1.0f};
        float falloff{1.55f};
    };

    static constexpr int MAX_SHADER_LIGHTS = 64;

    static Params& params();
    static void resetParams();

    static void registerComponent(LightComponent* component);
    static void unregisterComponent(LightComponent* component);
    static void clear();

    static int activeCount();
    static bool hasActiveLights();
    static void upload(Shader shader);
    static void drawDebug();

private:
    static std::vector<LightComponent*> components_;
};
